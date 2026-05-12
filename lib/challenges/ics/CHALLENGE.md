# ICS Challenge

The badge simulates an Industrial Control System node exposing an MQTT broker over its own Wi-Fi access point. Two obstacles stand between the player and the flag.

## Part 1 — Broker Authentication

The badge connects to an MQTT broker at `192.168.4.137:8883` using TLS and authenticates with the username `PLANT_SYSTEM` and an obfuscated password. The password is stored in the firmware as a 35-byte RC4 ciphertext and decrypted at runtime using the key `DEBUG`.

**Goal:** Recover the plaintext password by reversing the RC4 stream cipher, then authenticate to the broker as `PLANT_SYSTEM`.

## Part 2 — DRM State Machine

Once authenticated, the badge listens on the `drm` MQTT topic and runs a state machine that controls a simulated industrial valve. Each state reacts to an integer received on `drm` and either publishes a response or transitions silently.

The state machine is obfuscated: all branch conditions are wrapped in opaque predicates that use runtime ESP32 hardware values (timer, heap counters) and algebraic identities to defeat naive static analysis.

```
S1 ──(val > 50)──→ S2 ──(val < 67)──→ S3 ──(val even)──→ S4 ──→ S5 ──→ S1
│                   │                   │
│ pub "67"          │ pub "10"          └──(val odd)──→ S5
│                   └──(val ≥ 67)──→ S5, pub "5"
└──(val ≤ 50)──→ S1B ──→ S3
     pub "420"    (odd → pub "42" | even → pub "9000")
```

**S4** publishes the running product of all integers received across S1–S3 to the `drm` topic, then publishes `open_valve` to the `water` topic — this is the flag trigger.

**Goal:** Drive the state machine to S4 by sending the correct sequence of integers to `drm`, causing the badge to publish `open_valve` on the `water` topic.
