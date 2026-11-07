# Aidol home correction v0.3.1

User explicitly wants original JD Aidol namecard composition: left avatar, right nickname/signature, bottom two entries Add Friend / Walkie Talkie; editable through same miniapp. Update current prototype only; user will personalize names/avatars, no six named firmware variants.

- Preserve existing profile_store, game ID, SN and miniapp protocol. Use existing full96x156 avatar flash image without scaling.
- Home up/down toggles two tiles; center enters; long-down from activities returns home; radio PTT/volume unchanged. Home long-center opens numeric network-ID setup only; no hardcoded famous-name selector. Pair long-center accesses existing friends.
- Default uncustomized peers numbered devices, no stock famous-person portraits. Existing user Miaomiao remains available as seed and stored profile takes priority.
- Original Aidol navy/panel/red-focus styling, fixed geometry, readable long name scroll and <=28char signature wrapping in right column. Compare actual native LVGL render.
- Focused navigation/host UI/persistence tests, build/layout/release checks; preserve prior v0.3.0 package. Flash only current authorized MAC with safe_flash three segments, readback profile storage and identity, observe boot. Physical two-device audio and phone UI remain separate.
