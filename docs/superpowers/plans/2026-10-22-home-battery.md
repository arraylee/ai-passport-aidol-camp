# v0.7.2 home battery icon

User: show only a battery icon to the left of FoloToy on the home page, using the Aidol firmware as reference. Build/package only; no hardware writes.

Reference inspected: Aidol ui_profile.c BATT_BODY_W22 / H14 / tip2, percent fill, neutral >=40%, yellow20-39%, red0-19%, gray unknown; battery_feed_pct accepts SOC0..100, keeps last valid on invalid read.

1. Reuse attached/woken CW2017, no calibration/profile writes. Read once at boot, then at most every5s during idle. Cache valid SOC including zero; invalid read retains last valid; no valid read is unknown. Poll outside LVGL and avoid held keys/audio/game/phone/flash work. Bound SOC read to5ms so an absent/stalled gauge cannot stall input for100ms. Sampling does not count as user activity or delay20min sleep.
2. Add three small LVGL shapes,22x14 body with proportional inset fill plus2x6 tip atx136,y16; move FoloToy tox168,width60, keep title and existing profile/buttons. Home only, hide on all other screens/overlays; no percent text.
3. Verify actual LVGL screenshot, unknown/zero/low/normal/full rendering and geometry; power sampling/failure/rate/wrap/busy. Full host/helper tests, fonts, build/size, release layout and independent source rebuild. Packagev0.7.2 and Desktop copies with exact hashes. Keepv0.7.1 untouched. Final read-only review; no commit/push/PR.

Physical SOC accuracy and actual screen readability remain pending. No source-only claim of battery runtime/charging status.
