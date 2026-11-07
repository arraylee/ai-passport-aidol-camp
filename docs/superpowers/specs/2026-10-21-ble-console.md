# Bluetooth batch configuration

User approved: retain the existing desktop helper interface; scan nearby badges, choose each role, set and reset selected badges in one batch without USB or device reboot. Existing v0.5.9 needs a one-time firmware upgrade. Preserve SN, original NVS and all program/partition regions.

Use the local Python web server and macOS native BLE through Bleak. Read identity and management status from each device; address every change to its actual MAC/SN, never the advertising nickname or chosen persona. One computer may operate multiple peripherals; cap active transfers at two for predictable memory/radio use. Each row has independent progress, failure and verified completion. Scanning and modifying are separate; GET never initiates a reset. Existing per-launch token/loopback access rules stay.

Extend existing GATT with a read-only management status and reset command type 0x30. Reset command contains protocol version, operation, role, per-connection nonce, transaction and expected MAC. Reject stale nonce, bad target, unsupported protocol, concurrent upload/reset. Reset named demo/profile/friend/gallery stores only, incrementally. A durable pending marker permits recovery after power loss. After completion rebuild in-memory state and reload stores while staying on the same BLE connection; do not call esp_restart, reinitialize BLE or write firmware. Send the existing profile-text and PNG commands, then read back role, text, pixel CRC, reset state and unchanged physical boot token. Disconnect afterward and restore normal gameplay/audio/activation.

Keep the six existing portraits and wording. Treat desktop BLE discovery, one-device configuration, six-device batch operation, and firmware build as distinct evidence. No commits/push/PR or device flashing without separate authorization.

User additionally requires independent Bluetooth clear-data action: preserve current name/signature/avatar/role and original identity; clear friends, scores, activation/welcome and custom album; restore volume 90. Separate action 2, skip profile erase.
