# Implementation ledger — docs/superpowers/plans/2026-10-20-live-show.md

- Ruling: continue the existing independent untracked project, preserve existing release archives and install ledger; no Git commit/push (user agreement). No worktree reset/copy of shared changes.
- Scope confirmed by user: cards, mutually confirmed friends, gallery, proximity/PTT. Clicker separate later; other PPT features examples.
- Miaomiao confirmed as user, original supplied avatar copied unchanged; signature 简单开发，人人可造.
- Task 1: actual production-poll regression RED then GREEN; RX and TX timestamps generated after main now are no longer discarded. Fresh main/UI clock after poll.
- Task 1: signal latency regression RED then GREEN: 3-sample median + half-weight bounded EMA, HELLO120+3*id ms, trend480ms. Simulated far→near480ms; isolated-spike/threshold-noise/offline tests pass. Physical effect pending.
- Public Bilibili card API verified: 小狮日记444728505, 手工耿280793434, -LKs-125526. Original avatar URLs and full signatures saved in docs/live-show/*-public.json. Screen name remains LKs per user; use short exact intro excerpt excluding commercial contact text, record excerpt explicitly.
- No devices connected at start of this iteration. User PTT symptom: screen unchanged or uncertain. Do not claim full physical cause proved; add visible hold feedback and transition diagnostics.

- Tasks 1–3 implemented: six source-verified profiles, native static RGB565 assets, scoped menu/card/gallery/QR/radio/volume controls, two-sided session-bound social confirmation and separately versioned per-id NVS.
- Review P2 fixed: social state requires HELLO <800ms, not generic packet liveness; isolated production-code regression RED then GREEN. 1/3 HELLO loss retry test passes.
- Task4 code/build complete: input voltage/action logs, capture/playback/peak counters, 11 host tests, 530 rendered label/glyph checks, 18 asset pixel checks, five QR decodes, IDF build/size. Release/source rebuild underway.
- No current device connected. No v0.2.0 flash/real audio/six-device claims. Old v0.1.1 release and install ledger untouched.

- Final independent review passed after two P2 fixes. Firmware/source release verified. Independent source ZIP rebuild produced byte-identical bootloader/partition/app; build inputs match final source. Device acceptance remains pending (no USB).

## 2026-10-20 user steering: editable miniapp profiles BEFORE flashing
- Connected /dev/cu.usbmodem101 is new MAC 98:c3:77:f4:1e:1c, not original Miaomiao 4c:11:ae:32:e7:64. Safe-flash exact-MAC guard prevented all writes. Read-only current partition table is original demo; demo_cfg area allFF, no HRC1 records.
- User explicitly confirmed current new device replaces Miaomiao; old Miaomiao will not participate. Six roles otherwise unchanged. No v0.2.0 write has happened.
- User now requires avatar/nickname/signature editable from the existing miniapp like original demo. v0.2.0 is static only and not sufficient; extend BEFORE flashing. Need real BLE protocol compatibility and durable dynamic profiles.
- Rechecked reference work/ai-passport-event-firmware doc/ble-protocol-spec.md + main/event_ble.c + event_settings.c + config_json.c + png_decode.c. TRAECARD 128-bit service, cmd0010/notify0011/identity0012, 4-byte framed messages, type1 config JSON and type2 BEGIN/DATA/END image. Current miniapp avatar PNG96x156. Identity72B v2, SN read cardid folotoy-key DeviceKey, key never public.
- Prepare v0.3.0. Core game IDs stay stable when display names change. Dynamic own card/thumbnail stored separately with power-loss safe two-slot commit. BLE connection blocks PTT during edit; retain ESP-NOW proximity; disconnect resumes game. Must budget BLE+PNG transient heap and common Chinese dynamic font coverage. Peer nickname sync needed; do not silently retain wrong stock nickname after change.
- output/install-six-v020 contains generated compatible configs1..6, adapted installer with --replace-mac guard and boot observer. Neither was executed. Previous failed flash.log is only MAC mismatch. Do not mark installed. Read-only detected-device.json available.

## Replacement device v0.3.0 result
- MAC98:c3:77:f4:1e:1c installed as player1; original player1 e7:64 excluded by user. SN/originalNVS preserved at flash; identity rechecked unchanged after BLE runtime.
- Flash/readback/boot pass. BLE config text and avatar DONE; 220byte chunks32.58s. Full/card/small flash pixels match upload. Profile revision8 persisted and restart requested.
- 13 host groups +5 layout +4 flash mock pass; independent source rebuild all3bins identical.
- User screen check pending; real phone miniapp UI and six device audio/RSSI live acceptance pending. Other5units require common version; oldid2/3 remainv0.1.1,4-6uninstalled.

- User confirmed actual LCD avatar/nickname/signature correct after update and reboot. Phone miniapp UI and multi-device audio remain pending.
