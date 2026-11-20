# Reference signal definitions

These are the reference Gen 1 B58/340i candidates in the current source. They are not a compatibility list for other engines. Requests use HSFZ TCP port 6801 and UDS service `22` unless marked OBD. Positive UDS replies start with `62` plus the requested DID. The table describes the data bytes after that prefix.

| Display | ECU | DID or PID | Reference conversion |
| --- | --- | --- | --- |
| Boost | DME `12` | `4205` and `4201` | Two big-endian bytes each. Absolute kPa is raw ÷ 128, ambient kPa is raw ÷ 256. The collector subtracts ambient and converts to psi. |
| AFR | DME `12` | `5889` | Two bytes. Lambda is raw × 16 ÷ 65535, then converted to gasoline-equivalent AFR. |
| Ignition timing | DME `12` | `4A49` | Two signed big-endian bytes ÷ 10. The reference `FFFF` was ambiguous and stays blank. |
| Intake air | DME `12` | `580F` | One byte, raw × 0.75 − 48 °C. `FF` is rejected. |
| Transmission temperature | EGS `18` | `DA12` | Two big-endian bytes, raw − 48 °C. |
| Rail pressure | DME `12` | `56D7` | Two big-endian bytes, raw ÷ 50 bar. |
| Oil pressure | DME `12` | `586F` | Two big-endian bytes, raw ÷ 1000 bar. |
| Coolant temperature | DME `12` | `4300` | Two big-endian bytes, raw ÷ 10 − 273.15 °C. |
| ShiftDeck RPM | DME `12` | OBD `01 0C` | Two big-endian bytes ÷ 4. |
| ShiftDeck speed | DME `12` | OBD `01 0D` | One byte in km/h, displayed in mph. |
| Oil temperature | DME `12` | OBD `01 5C` | One byte, raw − 40 °C. Values above 170 °C are rejected. Read with `--extended`. |
| Battery voltage | DME `12` | OBD `01 42` | Two big-endian bytes ÷ 1000 V (control module voltage). Accepted from 6 to 18 V. Read with `--extended`. |

The two `--extended` channels are standard SAE J1979 PIDs, not BMW DIDs. On an F22 M240i (B58 Gen 1, `NBTevo_S19322I`) at idle the DME answered `01 5C` with `94` (108 °C) and `01 42` with `39 F8` (14.84 V), consistent with the car. On the same car the DME's `01 46` ambient (32 °C against 23 °C on the cluster) and `01 2F` fuel level (78 % against a full gauge) did not match the cluster, so they are not used.

The worker enforces data length, plausibility limits, and freshness. Look at the code for exact bounds and units. If the DME reports coolant via `01 05`, confirm that reading against the intended coolant channel before adding an explicit fallback. An OBD PID and a BMW DID are distinct requests with distinct reply formats. [Request table](../native/src/collector.c) · [Decoder](../native/src/gauge_core.c)

## F30 N20 stock-DME candidate

The dashboard build uses `BD_STOCK_DME`; the upstream reference build retains
the table above. A parked read-only capture on the owner's N20 produced:

| Channel | Request | Complete positive/negative reply | Interpretation |
| --- | --- | --- | --- |
| Coolant | `01 05` | `41 05 82` | 90 C, unsigned byte minus 40. |
| BMW coolant | `22 43 00` | `62 43 00 B8` | One payload byte, rejected by the reference two-byte decoder. Its candidate 0.75 scaling yields 90 C, independently matching OBD; this build uses standard OBD instead. |
| BMW rail | `22 56 D7` | `7F 22 31` | Identifier rejected; no reading. |
| OBD rail | `01 23` | `41 23 04 10` | 104 bar, big-endian unsigned raw times 0.1 bar. |
| Oil temperature | `01 5C` | `41 5C 81` | 89 C, unsigned byte minus 40. |
| Ambient pressure | `22 42 01` | `62 42 01 62 9F` | 98.6211 kPa using the reference conversion. |
| Charge pressure | `22 42 05` | `62 42 05 31 4D` | 98.6016 kPa using the reference conversion. Difference rounds to zero gauge boost. |
| OBD barometric pressure | `01 33` | `41 33 62` | 98 kPa, a coarser independent comparison for ambient. |

OBD formulas: [CSS Electronics J1979 PID reference](https://www.csselectronics.com/pages/obd2-pid-table-on-board-diagnostics-j1979).
Fixture: `native/tests/fixtures/n20-readings.json`. This is a single stationary
capture, not validation of boost across load or of all other reference channels.

Stock coolant uses OBD 05 directly. Rail first preserves the BMW read and falls
back to OBD 23 after rejection/invalid data. A successful fallback is retained
for that worker lifetime, avoiding repeated rejected DID requests. Oil temp is
sampled every two seconds and replaces the unused ethanol slot. Temperatures
expire after 3.5 seconds. Rejected or malformed replies remain blank.

Stock nine-channel frames use `NBG5`, while the reference ethanol build retains
`NBG4`. Each rejects the other's nine-channel format to avoid mislabelling an
oil temperature as ethanol or vice versa. Update the plugin and worker together.

## Native CDS dashboard candidate

The dashboard candidate now uses BD_NATIVE_CDS and no diagnostic workers for BoostDeck/ShiftDeck. The stock-DME OBD mappings above remain in the worker/reference source, not as a fallback in these gauges. See [CDS-GAUGES.md](CDS-GAUGES.md) for exact channels, observed versus candidate status and automatic Evo units.

## Subsequent MOST evidence and current BoostDeck

A parked MOST probe returned N20 DME gauge replies and standard OBD rail 0123. See native/tests/fixtures/n20-most-readings.json and [BOOSTDECK-MOST.md](BOOSTDECK-MOST.md). Current BoostDeck uses its MOST-only read worker, with RPM and engine/oil temperatures from CDS. ShiftDeck remains CDS-only. EGS was not tested; its tile becomes RPM. These idle replies do not prove calibration across load or cable-free wake operation.

## BoostDeck selectable OBD readings

Oil temperature (`01 5C`) and module voltage (`01 42`) were checked on the F22 M240i in [PR #102](https://github.com/inotbarnacle/ID6_apps_prerelease/pull/102). The selectable readings also include RPM, speed, calculated load, manifold absolute pressure, air flow, throttle, barometric pressure, outside-air temperature, fuel rate, fuel level, and accelerator pedal. The [decoder](../native/src/gauge_core.c) specifies each [SAE J1979](https://doi.org/10.4271/J1979_201202) conversion. Unsupported or stale readings show a dash.


The original nine dashboard positions can also select spark advance (`0E`), absolute load (`43`), short- and long-term fuel trims for banks 1 and 2 (`06`–`09`), commanded lambda (`44`), bank 1 sensor 1 catalyst temperature (`3C`), and commanded throttle (`4C`). These are standardized OBD readings where the ECU reports them. MHD's full logging catalog varies by DME and tune, and the official app also accepts tuner-supplied custom channel definitions. A name from the logger does not establish the request, byte layout, or scale for an ID6 gauge.

The Diesel profile uses `01 05` coolant, `01 0F` intake air, `01 23` direct-injection rail gauge pressure, and `01 0B` manifold pressure minus `01 33` ambient pressure for boost. It retains the existing EGS transmission-temperature query over HSFZ. AFR, timing, oil pressure, and active ethanol require separate DDE-specific evidence. The standard PIDs can also be selected individually where available. A B47 or N57 parked capture is still needed to verify what each DDE supports.


## MHD+ setting display

MHD+ returns a matching echo for accepted map and Max Cooling commands. The menu shows those last accepted choices in the current app session, with **Unknown** until each command is accepted. [MHD documents a Current Map logging channel](https://mhd-tuning.atlassian.net/wiki/spaces/MFS2/pages/11763743/MHD%2BSuite). Its documentation does not publish the request, response layout, or ROM applicability. The [current iDrive7 implementation](https://github.com/cauliflowermossgrape/idrive7-apps/blob/35bca28bc82b30482d73782877ebaf92c69dcb8c/apps/live_adjustments/README.md) checks command echoes and found no changes in its sampled DIDs across a two-slot comparison. The [published map-switch and Max Cooling captures](https://github.com/cauliflowermossgrape/idrive7-apps/pull/23#issuecomment-6520952752) contain the set commands and their replies. They do not include the monitor reading the current setting. [pymhd's Current Map definition](https://github.com/whatotter/pymhd/blob/cad5f80fb6c9526ce161ebff9a37d1bec4266cbe/core/parameters.py#L53) comes from an older I8A0S/N54 logger and does not establish a mapping for the F/G-series DME. A parked capture of the official MHD Monitor while selecting map 1, map 2, then map 1 would identify which field tracks the active slot on that DME. Repeat separately for cooling if MHD exposes it on the vehicle. Changes made from the phone app or steering-wheel controls are invisible to this display. Max Cooling can also reset at engine shutdown. [Upstream MHD command captures](https://github.com/cauliflowermossgrape/idrive7-apps/blob/main/apps/live_adjustments/README.md)
