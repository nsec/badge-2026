"""
Pre-build script: patch the ST25R3916 RFAL library for ESP32 compatibility.

1. Fix BR macro clash: Xtensa SDK defines  #define BR 4  (CPU special register)
   which collides with  uint8_t BR;  in rfal_nfcDep.h.

2. Fix chip variant: The upstream library defaults to ST25R3916B, but the
   NSec badge uses the ST25R3916 (non-B).  Patch the default config header.
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
    config_path = os.path.join(search_root, "STM32duino ST25R3916", "src",
                               "st25r3916_default_config.h")
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


patch_rfal(env)
