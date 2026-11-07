# AIdol Live Experience Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans; independent storage modules may use dispatching-parallel-agents. User already authorized direct implementation; no commits or publication.

**Goal:** deliver the four requested experience additions on the working v033 firmware.
**Architecture:** retain main-owned state and audio scheduling; add room membership state, atomic Flash card/album stores, minimal motion and reference-based deep sleep. Gallery and friend storage own separate files and partitions; parent integrates UI, core, BLE, builds and hardware.
**Tech Stack:** ESP-IDF5.5.3 C, LVGL9.5, ESP-NOW, NimBLE, esp_new_jpeg1.0.2.
**Spec:** ../specs/2026-10-20-live-experience-design.md

## Global Constraints
- No new backend/router, keep six IDs and editable own profile.
- Preserve protected partitions and existing user data, no full erase.
- Ten-minute idle threshold600000ms; do not claim six-hour life unmeasured.
- Long DOWN returns; physical mapping record required.
- Main priority6 and5ms yield retained; no whole-screen animation layers.

## Review Focus
- Offline/left-room peer must not receive private or broadcast speech (room test).
- Interrupted/corrupt upload must preserve previous image (gallery storage fault test).
- Stale peer/session or out-of-order card packets must not save wrong friend (card sync tests).
- Waking held key must not trigger navigation/PTT (idle/input tests).
- No new flash segment may overlap protected data; old profiles survive migration (layout/storage tests).

### Task1 Album (isolated module)
Files main/hr_gallery.c,h; tests/host/test_gallery.c; tests/gallery_fakes/*.
Interfaces bool hr_gallery_init(void); unsigned hr_gallery_image(const uint8_t*,size_t,uint32_t); void hr_gallery_abort(void); void hr_gallery_expire(uint32_t); bool hr_gallery_busy(void); const lv_image_dsc_t* hr_gallery_image_descriptor(void).
- [x] Test BEGIN/DATA/END sequence, truncation, size bounds, corrupt decode/flash, reboot old/new bank.
- [x] Implement streaming JPEG/Flash using bounded allocation; no UI mutation inside module.
- [x] Run module tests; report esp_new_jpeg dependency and memory peak.

### Task2 Friend cards (isolated module)
Files main/hr_cards.c,h (sync) and main/hr_friend_store.c,h (storage), dedicated tests only.
Interfaces bool hr_cards_init(unsigned); void hr_cards_tick(hr_core_t*,uint32_t); bool hr_cards_send(hr_core_t*,hr_packet_t*,uint32_t); void hr_cards_receive(hr_core_t*,const hr_packet_t*,uint32_t); bool hr_cards_ready(const hr_core_t*,unsigned); unsigned hr_cards_progress(void); const hr_profile_t* hr_friend_profile(unsigned); void hr_cards_reset(void).
Parent provides packet kinds7/8/9 and invokes receive only after core validates peer MAC/boot/seq. Parent calls saved-mask persistence only after cards_ready. Module never changes existing core/platform/UI/profile files.
- [x] Tests malformed/stale/duplicate/reordered chunks, retries, two peers, owner change, CRC/flash failure, reboot.
- [x] Implement explicit acknowledged pull transfer and double-bank persisted profile.
- [x] Run isolated tests; provide integration call requirements.

### Task3 Room and UI (parent)
Files main/hr_core.c,h,hr_controls.c,hr_ui.c,h,hr_room.c,h; tests/test_room.c,test_controls.c,host/test_ui.c.
- [x] Tests join/leave/restart, skip unjoined targets, no voice outside room, coordinator departure, long DOWN every subpage and three home tiles.
- [x] Add real lobby membership, join animation; friends cached card/list; gallery entry and empty demoQR.
- [x] Wire card transfer into platform; postpone social_saved until snapshot persisted; show actual progress/completion.
- [x] Generate fonts, render screenshots, inspect240x320 and run native tests.

### Task4 Power and final integration (parent)
Files main/hr_idle.c,h/main.c,components/bsp power adapters; BLE config mode; partition layout, CMake/dependencies; tests/test_idle.c and release tests.
- [x] Read previous AIdol first-frame/deep sleep code and match wake suppression.
- [x] Test600000ms boundary, activity inhibitors, rollover, wake-key suppression.
- [x] Add safe shutdown, boot/wake visual transition, battery/brightness handling.
- [x] Build/size/host/tool/partition tests, source rebuild and versioned package.
- [ ] Safely install connected device exact MAC; separately verify miniapp gallery, both-device room/card/PTT, keys, animations, real10-minute sleep/wake. Record six-device and six-hour battery pending if unavailable.

User deferred physical tests; no v0.4.0 device was flashed. Parent implementation uses hr_core room state and hr_power rather than separate room/idle units. Build/package evidence is in docs/evidence; physical checklist stays pending.
