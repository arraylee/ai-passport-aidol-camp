# Voice-gated course implementation plan

Goal: Replace timed auto-running with a finite, voice-gated high/low challenge. The user has delegated song and level choices; implement inline without another approval gate. No commit or push.

Architecture: Keep microphone pitch extraction, room membership and audio exclusion. Update the deterministic game model, then render it through the existing small Flash sprites and LVGL scene.

Spec: ../specs/2026-10-21-voice-gated-song-course-design.md

Constraints: ESP32-C3, no PSRAM; retain 44 KiB LVGL pool and 6144-byte main stack; explicit split flashing only; preserve original NVS and protected identity; reset only demo partitions as previously authorized.

- [x] Write a failing silence/voice course test: silence indefinitely cannot advance or clear; short dropout tolerance; stopped voice lands on safe support; player can wait there.
- [x] Implement 50 px/s voiced movement, a fixed 1000 px course, checkpoint rewind with -50 points, one-shot collectibles and one-shot completion. Prepare for 3 seconds; celebration lasts 1.8 seconds after actual completion. No play deadline.
- [x] Author 12 coin positions with four clearly marked high-note rewards. Reference voice trajectory must collect every coin without death; ordinary sustained voice must not collect high rewards. Retain same-tone height hold and baseline-relative high/low control.
- [x] Update score rule version and CRC magic to 5, invalidating incompatible old bests. Check clock wrap, idle stalls and no repeated score farming.
- [x] Update menu, in-game HUD and native UI trajectory. Render 240x320 and inspect the exact Flash assets; retain all other screens.
- [x] Run full host/font/tool tests, IDF build and size. Package v0.5.4, independently rebuild source ZIP and compare all three images.
- [ ] Flash the exact Miao device if its port is available, verify protected ranges and all images, reset demo settings, establish one serial baseline, then request physical pitch/gameplay acceptance. Do not interrupt live play with another serial or BLE connection.

Review focus: silence no auto-win; rescue no forward skip; revisiting coins no score farming; high rewards reachable only with deliberate higher pitch; finish cannot save twice or depend on a 20s wall clock. This is an original free-humming level with a Red Face theme, not a transcribed or recognized song.
