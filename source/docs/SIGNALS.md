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
