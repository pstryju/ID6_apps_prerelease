# Dashboard compatibility

Match the reported firmware and both SHA-256 hashes before using the dashboard package. The desktop installers also check the display and generate layouts for 800 × 480, 960 × 480, 1280 × 480, 1440 × 540, and 1920 × 720. The raw dashboard ZIP uses the 1280 × 480 layout.

| NBTevo firmware | AppFramework SHA-256 | CoreFramework SHA-256 | Evidence |
| --- | --- | --- | --- |
| N18032I | `93f4c7ab7e59c0adf7923b9b491de06165ba9d97bbe7412292e7ddc418ca2123` | `64ba154c68c772f9ca779fc18507f5b34715be58ded4a198b8a62b90d16c3ee0` | Dashboard menus rendered on the reference head unit |
| N18092I | Same as N18032I | Same as N18032I | Exact framework match and [owner report](https://github.com/inotbarnacle/ID6_apps_prerelease/issues/31) |
| P18362I | `d9c674c210272e8f07473e73e59ed8f23d984d4d30162971bedb56d21c682b28` | `896d9de22fa85f7f5aa4fc2a0e62003c63c41c47186b06f39a2fd4fee06fa5d5` | Offline ABI comparison and QNX emulator plugin load with the captured frameworks |
| V20153I | `085b7f2a9f96734be51621a70a04c24d01525eb5e9852f45c845a7d2f4ae4c67` | `56705e56c349d9ae57a7782bd39b795cfc01391ba8ff155046f32fbe8ffad2cb` | Exact match to the capture used for the earlier QNX load check |
| Y24073J | `ef2488b007d592301d8f21d9aa60771ad552d06b5e97c29e7813e3946644d820` | `cb0e488d1ebe2e30de87bb56f0e93d6a2d76b44309b0b47fef5b0b7cec048973` | Exact match to the capture used for the earlier QNX load check |

These checks concern the head unit. Available gauges and controls depend on the vehicle ECU and tune. The two distinct gateway paths reported for [T23235J](https://github.com/inotbarnacle/ID6_apps_prerelease/issues/37) and the reference car are handled through `gateway-ip.txt`. The installer still requires a matched firmware profile. For S19322I or T23235J, use the [read-only capture tool](../tools/capture_headunit.py) to collect the framework evidence for a port. See [issue #27](https://github.com/inotbarnacle/ID6_apps_prerelease/issues/27) for the reported R19144I HMI freeze.

## xDrive Control

The Status command reports whether DSC identification timed out, returned a UDS refusal, or identified an unsupported SGBD index. This distinguishes the missing replies in [issue #23](https://github.com/inotbarnacle/ID6_apps_prerelease/issues/23) from an identified 0F2100 in [issue #25](https://github.com/inotbarnacle/ID6_apps_prerelease/issues/25). Mode changes retain the stationary, engine-off Park checks. The write allowlist remains 0F2510 and 0F2850. The immediate AWD restore is limited to 0F2850, as in the [upstream measurements and implementation](https://github.com/cauliflowermossgrape/idrive7-apps/blob/main/apps/live_adjustments/xdrive.js).
