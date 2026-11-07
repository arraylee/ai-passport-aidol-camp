# Miniapp profile compatibility extension

User authorized direct implementation and flashing replacement Miaomiao; old Miaomiao excluded. Existing v0.2.0 not flashed. No commits/push.

- Reuse original TRAECARD legacy-compatible public identity with zeroed key slot and framed config/image notifications, read cardid only. Do not initialize original NVS for writes.
- Add pure UTF-8/config/frame/upload checks with malformed length/partial/disconnect/CRC/save-failure tests.
- Persistent 2-bank custom profile area outside SN/demo_cfg; write inactive bank and commit header last. Upload scratch; streamed PNG RGB/RGBA96x156 decode to mapped RGB565 card and icon. No whole image heap allocation. Invalid uploads preserve last good profile.
- NimBLE host queue with main-loop consumer; replies stay on host task, generation-bound; idle frame timeout. Plain miniapp profile protocol matches reference, no admin/config secret endpoints.
- Edit name/intro and avatar on existing miniapp, reboot recovery; stable gameID independent. Periodically advertise bounded custom nickname to teammates, never wrong stock portrait for a changed remote avatar.
- Full common-Chinese fallback font for user text; bounds/readability and memory budget. Freeze uploads while PTT, disable PTT while phone connected.
- Build, tests, independent review, release v0.3.0, flash exact new Miaomiao MAC98:c3:77:f4:1e:1c with replacement of4c:11:ae:32:e7:64. Readback SN/originalNVS, application; physical BLE/keyboard/audio evidence separately.

Implementation: 224-byte committed header; nickname/title/signature, two 64KB image banks plus scratch; cardid readonly. Host13 tests pass, exact source rebuild matches. First replacement device flashed/readback/boot pass; liveBLE validation in progress.
