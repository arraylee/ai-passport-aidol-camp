# v0.3.2 observed interaction repair

Observed ADC trace: ID2 DOWN release twice on RADIO selected offline ID3; next UP hold generated PTT, capture remained zero. Keep requested long-UP PTT and short-UP/DOWN navigation. Skip offline/conflicting peers on explicit navigation only; ALL always available; do not automatically retarget private calls to broadcast. Give ALL a stable visual option and all selections a red outline. Raise UP hold to600ms and test a500ms tap. Preserve held target lock/release semantics.

Source-confirmed social issue: complete implies accepted, and old select guard rejected all accepted states; allow selecting a different eligible friend after completion by starting a new session, retaining friend mask. Keep incomplete confirmation locked with explicit UI guidance.

Add transition-only diagnostics (page,target,online,phone,held,blocked,speaker,can-talk,volume-mode,setup,ready) and max input polling gap. No audio protocol change without evidence. Build/test/package then safely flash current ID2 only; observe actual keys/radio audio before claiming fixes accepted. Original SN/NVS/demo settings/profile persist.

## v0.3.3 timing hypothesis

User rejects v032 hardware interaction/audio. Reattached v032 baseline: home max input gap 80–81ms; app main priority1 is below LVGL4. Minimal experiment: raise control loop to6, retain5ms sleep, instrument own UI render time, whole work time and typed radio RX/TX counters. No gesture/protocol change. Compare real baseline and candidate; if gaps remain, isolate the measured blocking operation instead of tuning hold thresholds again. Hardware speech remains unverified.
