# v0.7.1 crowded discovery and complete room list

Base: work/hideseek-radio-ble v0.7.0 from the user's other task, now idle. Preserve Bluetooth live config/clear/no-reboot, home default Team,20min idle, avatars/friends, audio/game, original identities/partitions. No device reads/writes in this build turn unless later explicitly needed.

Observed source causes: room renderer paginates four choices without a page indicator (five devices => four peers+all/game/back); near-contact matcher already uses strongest smoothed RSSI but requires12dB lead, preventing a stable7dB winner in a crowded group. A single fresh reciprocal intent frame can pop one badge while the other already changes targets. RSSI does not prove physical distance or intention.

1. Failing regressions: all five peers and all/game/back visible for every selection; crowded six-node actual HSW2 roundtrip topology where closest pair leads7dB, observers do not pop; transient one-HELLO reciprocal intent rejected.
2. Show all eight room choices on240x320 with20px names and compact single-line signal, no pages; actual peer count in header. Reuse existing25 signal bars; add only two name labels/two frames to44KiB LVGL pool.
3. Keep strict absolute-30dBm/three raw samples/1.8s/ten-new-samples/activation/both-confirmation. Lower dominance to6dB, retain ambiguous ties as no-popup, require300ms and2 new reciprocal HELLOs. Keep locked popup target and clear mutual state on switches/session/gates. Add diagnostic peer/room masks and touch reason/signal ranking to distinguish actual dropouts from UI.
4. Full host, BLE helper, LVGL bound/glyph/heap checks; IDF5.5.3 build/size; isolated source rebuild, web/split/source hash checks; v0.7.1 BIN to Desktop. New version keeps HSW2/BLE management v1 compatibility. Final fresh read-only review per executing-plans skill. No push/PR.

Live six-badge proximity calibration, RF loss, bilateral popups and audio/UI acceptance remain separate from simulation. Never claim RSSI guarantees physical nearest.
