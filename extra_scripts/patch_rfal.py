"""
Pre-build script: patch the ST25R3916 RFAL library for ESP32 compatibility.

1. Fix BR macro clash: Xtensa SDK defines  #define BR 4  (CPU special register)
   which collides with  uint8_t BR;  in rfal_nfcDep.h.

2. Fix chip variant: The upstream library defaults to ST25R3916B, but the
   NSec badge uses the ST25R3916 (non-B).  Patch the default config header.

3. Enable NFC-A listen mode for card emulation.

4. Deferred ISR: ESP32 cannot do SPI in ISR context.  Make the ISR just set
   a flag; rfalWorker() and WaitForInterruptsTimed() poll for it.

5. Deferred ISR in rfalWorker() — process interrupts via flag + pin-level
   check (catches missed RISING edges).

6. Disable low-power mode in listen POWER_OFF state — the oscillator takes
   700µs to wake, causing missed REQA from phone readers.

7. Suppress false EOF interrupts in NFC-A listen mode — 100% ASK modulation
   causes spurious EOF that resets the passive target state machine.
"""
Import("env")
import os


def patch_rfal(env):
    libdeps = env.subst("$PROJECT_LIBDEPS_DIR")
    env_name = env.subst("$PIOENV")
    search_root = os.path.join(libdeps, env_name)

    if not os.path.isdir(search_root):
        return

    # --- Patch 1: BR macro conflict in rfal_nfcDep.h ---
    for root, _dirs, files in os.walk(search_root):
        if "rfal_nfcDep.h" in files:
            fpath = os.path.join(root, "rfal_nfcDep.h")
            with open(fpath, "r") as f:
                content = f.read()

            marker = "/* ESP32_BR_PATCHED */"
            if marker not in content:
                patch = (
                    "\n"
                    f"{marker}\n"
                    "#ifdef BR\n"
                    "#undef BR\n"
                    "#endif\n"
                )
                last_include = content.rfind("#include")
                if last_include != -1:
                    end_of_line = content.index("\n", last_include)
                    content = content[: end_of_line + 1] + patch + content[end_of_line + 1 :]
                else:
                    content = patch + content

                with open(fpath, "w") as f:
                    f.write(content)
                print(f"  [patch_rfal] Patched BR macro conflict in {fpath}")

    # --- Patch 2: ST25R3916B -> ST25R3916 in default config ---
    lib_st25r_dir = os.path.join(search_root, "STM32duino ST25R3916", "src")
    config_path = os.path.join(lib_st25r_dir, "st25r3916_default_config.h")
    if os.path.exists(config_path):
        with open(config_path, "r") as f:
            content = f.read()

        if "#define ST25R3916B" in content:
            content = content.replace("#define ST25R3916B", "#define ST25R3916")
            with open(config_path, "w") as f:
                f.write(content)
            print(f"  [patch_rfal] Patched chip variant: ST25R3916B -> ST25R3916")

    # --- Patch 3: Enable NFC-A listen mode for card emulation ---
    listen_config = os.path.join(search_root, "STM32duino NFC-RFAL", "src",
                                 "rfal_default_config.h")
    if os.path.exists(listen_config):
        with open(listen_config, "r") as f:
            content = f.read()

        old = "#define RFAL_SUPPORT_MODE_LISTEN_NFCA              false"
        new = "#define RFAL_SUPPORT_MODE_LISTEN_NFCA              true"
        if old in content:
            content = content.replace(old, new)
            with open(listen_config, "w") as f:
                f.write(content)
            print("  [patch_rfal] Enabled LISTEN_NFCA in rfal_default_config.h")

        # Re-read in case we just wrote
        with open(listen_config, "r") as f:
            content = f.read()

        old_ap2p = "#define RFAL_SUPPORT_MODE_LISTEN_ACTIVE_P2P        false"
        new_ap2p = "#define RFAL_SUPPORT_MODE_LISTEN_ACTIVE_P2P        true"
        if old_ap2p in content:
            content = content.replace(old_ap2p, new_ap2p)
            with open(listen_config, "w") as f:
                f.write(content)
            print("  [patch_rfal] Enabled LISTEN_ACTIVE_P2P in rfal_default_config.h")

    # --- Patch 4: Deferred ISR in st25r3916_interrupt.cpp ---
    # ESP32 cannot run SPI in ISR context.  Make ISR just set a flag.
    # Also add pin-level check in st25r3916CheckForReceivedInterrupts
    # and poll inside WaitForInterruptsTimed busy-wait loop.
    isr_file = os.path.join(lib_st25r_dir, "st25r3916_interrupt.cpp")
    if os.path.exists(isr_file):
        with open(isr_file, "r") as f:
            content = f.read()

        modified = False

        # 4a: Replace ISR to just set flag (no SPI in ISR context)
        old_isr = ("void RfalRfST25R3916Class::st25r3916Isr(void)\n"
                   "{\n"
                   "  st25r3916CheckForReceivedInterrupts();\n"
                   "\n"
                   "  // Check if callback is set and run it\n"
                   "  if (NULL != st25r3916interrupt.callback) {\n"
                   "    st25r3916interrupt.callback();\n"
                   "  }\n"
                   "}")
        new_isr = ("void RfalRfST25R3916Class::st25r3916Isr(void)\n"
                   "{\n"
                   "  /* On ESP32, ISR context cannot do SPI. Just set the flag. */\n"
                   "  isr_pending = true;\n"
                   "}")
        if old_isr in content:
            content = content.replace(old_isr, new_isr)
            modified = True

        # 4b: Process deferred ISR in WaitForInterruptsTimed busy-wait loop
        old_wait = ("  /* Run until specific interrupt has happen or the timer has expired */\n"
                    "  do {\n"
                    "    status = (st25r3916interrupt.status & mask);\n"
                    "  } while ((!timerIsExpired(tmrDelay) || (tmo == 0U)) && (status == 0U));")
        new_wait = ("  /* Run until specific interrupt has happen or the timer has expired */\n"
                    "  do {\n"
                    "    /* Process deferred ISR: check flag OR pin level (catches missed edges) */\n"
                    "    if (isr_pending || digitalRead(int_pin) == HIGH) {\n"
                    "      isr_pending = false;\n"
                    "      st25r3916CheckForReceivedInterrupts();\n"
                    "    }\n"
                    "    status = (st25r3916interrupt.status & mask);\n"
                    "  } while ((!timerIsExpired(tmrDelay) || (tmo == 0U)) && (status == 0U));")
        if old_wait in content:
            content = content.replace(old_wait, new_wait)
            modified = True

        if modified:
            with open(isr_file, "w") as f:
                f.write(content)
            print("  [patch_rfal] Patched st25r3916_interrupt.cpp: deferred ISR with pin-level check")

    # --- Patch 5, 6, 7: Fix rfal_rfst25r3916.cpp ---
    worker_file = os.path.join(lib_st25r_dir, "rfal_rfst25r3916.cpp")
    if os.path.exists(worker_file):
        with open(worker_file, "r") as f:
            content = f.read()

        modified = False

        # Patch 5: Process deferred interrupts in rfalWorker() with pin-level fallback
        old_worker = ("void RfalRfST25R3916Class::rfalWorker(void)\n"
                      "{\n"
                      "  switch (gRFAL.state) {")
        new_worker = ("void RfalRfST25R3916Class::rfalWorker(void)\n"
                      "{\n"
                      "  /* Process deferred interrupt: check flag OR pin level (catches missed RISING edges) */\n"
                      "  if (isr_pending || digitalRead(int_pin) == HIGH) {\n"
                      "    isr_pending = false;\n"
                      "    st25r3916CheckForReceivedInterrupts();\n"
                      "    if (NULL != st25r3916interrupt.callback) {\n"
                      "      st25r3916interrupt.callback();\n"
                      "    }\n"
                      "  }\n"
                      "\n"
                      "  switch (gRFAL.state) {")
        if old_worker in content:
            content = content.replace(old_worker, new_worker)
            modified = True

        # Patch 6: Disable low-power mode in listen POWER_OFF state.
        # The oscillator takes 700µs to wake, causing missed REQA from readers.
        old_lpm = ('#if 1  /* Perform bit rate detection in Low power mode */\n'
                   '        else {\n'
                   '          st25r3916ClrRegisterBits(ST25R3916_REG_OP_CONTROL, '
                   '(ST25R3916_REG_OP_CONTROL_tx_en | ST25R3916_REG_OP_CONTROL_rx_en | ST25R3916_REG_OP_CONTROL_en));\n'
                   '        }\n'
                   '#endif')
        new_lpm = ('#if 0  /* Disabled: low power mode causes 700us oscillator wake delay, missing REQA */\n'
                   '        else {\n'
                   '          st25r3916ClrRegisterBits(ST25R3916_REG_OP_CONTROL, '
                   '(ST25R3916_REG_OP_CONTROL_tx_en | ST25R3916_REG_OP_CONTROL_rx_en | ST25R3916_REG_OP_CONTROL_en));\n'
                   '        }\n'
                   '#endif')
        if old_lpm in content:
            content = content.replace(old_lpm, new_lpm)
            modified = True

        # Patch 7: Suppress EOF → POWER_OFF in IDLE state for NFC-A card emulation.
        # NFC-A 100% ASK modulation causes false EOF interrupts that reset the passive
        # target state machine, preventing data exchange from ever completing.
        eof_target = ('      /* If EOF has already been received processing of other events is neglectable */\n'
                      '      if (((irqs & ST25R3916_IRQ_MASK_EOF) != 0U) && (!gRFAL.Lm.dataFlag)) {\n'
                      '        rfalListenSetState(RFAL_LM_STATE_POWER_OFF);')
        eof_replacement = ('      /* EOF suppressed: NFC-A 100% ASK modulation causes false EOF that resets */\n'
                           '      /* the passive target state machine. Field loss handled by discovery timer. */\n'
                           '      if (((irqs & ST25R3916_IRQ_MASK_EOF) != 0U) && (!gRFAL.Lm.dataFlag)) {\n'
                           '        /* rfalListenSetState(RFAL_LM_STATE_POWER_OFF); -- DISABLED for NFC-A CE */')
        if eof_target in content:
            content = content.replace(eof_target, eof_replacement)
            modified = True

        if modified:
            with open(worker_file, "w") as f:
                f.write(content)
            print("  [patch_rfal] Patched rfal_rfst25r3916.cpp: deferred ISR + disabled low-power + EOF fix")


patch_rfal(env)
