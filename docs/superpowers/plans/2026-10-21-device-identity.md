# v0.6.0 identity repair implementation plan

Approved scope: preceding chat + device-identity-design.md; execute in this existing standalone workspace. No commit/push/PR or device writes.

1. Add failing `tests/test_identity.c`: six same-role MACs, identity routing, reboot roster, room coordinator and private/all voice.
2. Add pure-C `hr_identity.[ch]`: HSW2 pack/unpack, HELLO-only slot mapping, atomic roster encode/decode; authoritative callback MAC. Register in main/host CMake.
3. Add coordinator field/helpers in `hr_core.[ch]`; preserve legacy default in existing direct-core tests, elect dynamically in production adapter; replace fixed1 dependencies in core/audio/UI. Test lease-safe host transitions and non-home election.
4. Integrate adapter at platform RX/TX boundary, persist roster/friends atomically in demo_cfg, load before card init. Clear per-slot live profile when a nonfriend MAC is replaced. Preserve six-profile role APIs and SN/miniapp format.
5. Upgrade friend-store headers to HFC2 with owner/peer MAC, bind before read/write, update paired-card/fake-store tests. Ignore old role-only friends rather than guessing identities.
6. Remove unused profile.gallery seed through generator and prove remaining bitmap pixels/UI unchanged. Set firmware version0.6.0. Update console new-version duplicate-role semantics and tests, without writes to real hardware.
7. Run complete host + helper tests, UI/native screenshot checks, IDF build/size. Package versioned source/split/web artifacts, independently rebuild source and compare binaries, verify current partitions/SN boundary and copied desktop BIN. Document all evidence and remaining physical acceptance.

Review focus: asymmetric local slot maps, host change mid-speech, power interruption between roster/snapshot, slot reuse with stale packets, mixed old/new firmware. Each receives an explicit regression test.
