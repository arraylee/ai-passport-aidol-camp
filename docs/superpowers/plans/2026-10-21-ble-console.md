# Bluetooth Console Implementation Plan

> For agentic workers: execute inline using superpowers:executing-plans; final independent code review.

**Goal:** Deliver the approved Bluetooth batch helper and matching web-flashable firmware.
**Architecture:** Extend the existing device GATT, reset state incrementally on the main task, and reuse avatar/text uploads. Keep the local web UI and replace serial transport with a native BLE event-loop worker.
**Tech Stack:** ESP-IDF 5.5.3 C, NimBLE, Python 3.11, Bleak 2.0.0, existing HTML/CSS/JS.
**Spec:** ../specs/2026-10-21-ble-console.md

## Global Constraints
- Preserve identity 0x356000+0x4000, original NVS, firmware and partition table.
- No USB or device reboot during configuration; one-time upgrade is separate.
- No commits, pushes, PRs or device flashing without separate authorization.
- Work from isolated source snapshot because the active project is untracked and another chat edits it.

## Review Focus
- Device replaced, duplicate SN, wrong MAC or stale nonce must never modify another device.
- Disconnect/power loss during reset/upload cannot produce a success report; pending resets recover.
- Legacy firmware is identifiable but cannot reset through unsupported commands.
- One failed badge cannot stop the remaining batch; only two BLE jobs active.
- Readback includes stored pixel CRC/text/state and unchanged physical boot token.

### Task 1: Firmware management protocol and incremental reset
Files: main/hr_configure.[ch], main/hr_ble.[ch], main/main.c, main/hr_platform.[ch], tests/test_configure.c, tests/CMakeLists.txt.
- [ ] Write reset state-machine tests with fake bounded flash and pending/settings callbacks; prove wrong target/role, failure, retry/recovery and complete erase boundaries.
- [ ] Run tests RED, implement minimal management state machine, run GREEN.
- [ ] Add GATT status and command; apply runtime state without reboot; build firmware and run host suite.

### Task 2: Bluetooth desktop transport and existing UI
Files: tools/device_console/ble_protocol.py, ble_console.py, server.py, web/app.js, web/index.html, launcher, tests/test_ble_console.py.
- [ ] Test fixed status framing, uploads, target guards, readback mismatch and partial batch failure RED.
- [ ] Implement native BLE discovery and limited-concurrency job execution; preserve loopback/token checks and existing cards/role selectors.
- [ ] Run desktop suite GREEN and exercise HTTP/UI, attempt read-only live BLE discovery.

### Task 3: Package and verify
Files: tools/package_ble_release.py, README, protocol doc, releases/v0.7.0, desktop helper.
- [ ] Run full host/tool tests, font verification, IDF build/size, web image validation, package hashes/source checks.
- [ ] Obtain fresh review and fix significant findings with regression tests.
- [ ] Install updated helper on Desktop without overwriting active source; deliver matching BIN/source/package and explicit physical acceptance limits.
