# Originating case: F30 N20 and Evo Y

This reference records the specific project observations available when the
skill was created. It is not a set of default addresses, profiles, connector
IDs, ABI layouts or commands for every NBT Evo. Read current project code and
latest owner feedback before relying on a historical state.

## Target and local project

- F30 328i xDrive, N20B20, factory windshield HUD and exhaust flap.
- NBTevo_Y24073J, observed screen 1280×480.
- OMAP SSH `169.254.199.119`; Jacinto SSH `169.254.199.99`.
- Working external HSFZ gateway during a session: `169.254.20.21:6801`.
  The reference build's `169.254.21.3` was wrong for this vehicle.
- Project `ID6_apps`, fork `pstryju/ID6_apps_prerelease`; local command list
  `KOMENDY.md`. Windows install/uninstall wrappers call tested PowerShell scripts.
- Licensed QNX SDK lives separately. The private Evo copy contains `opt/` and
  `var/`, not the complete root. Private outputs are under ignored `.local/`.

The reconstructed Y `PluginApp` base was assessed as 352 bytes, `Parcel` as 20,
`Message` as 80 with alignment 8, and the primary vtable as 34 entries. A minimal
ABI app loaded and worked. Do not reuse those declarations for another target
without checking its actual binaries.

## Transport evidence and app state

External diagnostic reads worked, then timed out after sleep without the OBD
cable. Plugging the cable alone later did not demonstrate recovery. CDS RPM,
speed and temperatures were shown on the owner's screen; the owner confirmed
CDS still worked after sleep. That success does not prove MOST sleep behavior.

A parked MOST DME probe with the engine idling and ENET attached for SSH returned:

| Request | Positive reply | Candidate interpretation |
| --- | --- | --- |
| `224201` | `6242016289` | Ambient 98.53515625 kPa |
| `224205` | `624205314D` | Charge 98.6015625 kPa absolute |
| `225889` | `6258891017` | Lambda 1.0056306 |
| `224A49` | `624A49FFEC` | Timing -2 degrees |
| `22580F` | `62580F6C` | Intake temperature 33 C |
| `22586F` | `62586F09D5` | Oil pressure 2.517 bar |
| `0123` | `41230402` | Rail pressure 102.6 bar |

`2256D7` was refused with `7F2231`. Pressure difference is approximately
0.000664 bar, consistent with a zero boost display at idle. These captures
prove transport and response bytes, not independent calibration across load.
Cable-free MOST sleep/wake remained unverified at skill creation.

The candidate ShiftDeck uses CDS only. BoostDeck uses MOST-only DME reads plus
CDS RPM, engine/oil temperatures and Evo temperature units; pressure is explicitly
bar rather than a guessed CDS pressure enum. EngineCodes gained upstream common
HSFZ-to-MOST fallback; its MOST read/clear path passed simulations, not a vehicle
validation. Engine read/clear remains available without a tuning suite.

HUD is an offline-prepared text RPM/boost prototype using an existing terminal
core with a new DSI adapter, not a proven port of the old curved tachometer.
Its candidate windshield profile is unique terminal `82/00`, node `0101`,
handles 1–6, frame 1; placement and concurrent clients require a car test.
xDrive remains an unverified guarded HSFZ candidate. N20 exhaust control is
not enabled; only identification/read probes were prepared.

## Profiles and useful project entry points

The mapping was extracted into `source/native/src/profiles/*.inc`, with a
generic decoder. Current N20 MOST data is in `n20.inc`; original reference DID
and standard OBD tables remain separate. B48/N13 registry entries are empty,
not copies of the N20 profile. Mapping edits require a rebuild.

The installer defaults to explicit `n20`, preserving the owner's configuration.
Experimental `auto` reads F150/F187/F189 and accepts explicit unambiguous family
tokens. Synthetic tests cover logic, but real identity replies and validated
software-ID matching remain needed. A missing profile file defaults to N20 only
as a legacy behavior of this particular application, not a skill recommendation.

Relevant existing commands, when working inside this project:

- `Install-Evo.cmd`, `Uninstall-Evo.cmd`: reversible deployment with receipts;
  no automatic HU restart. Uninstall restores one specific deployment.
- `Test-Evo-SSH.cmd`: current transport check.
- `Test-Evo-MOST.cmd`: bounded known DME gauge reads.
- `Test-Evo-Engine.cmd`: only F150/F187/F189 identification reads.
- `Test-Evo-Exhaust.cmd`: identification and candidate 60FD read; no actuation.
- `Remove-Evo-Tests.cmd`: retirement of old AbiSmoke/CdsProbe using receipts.
- `source/tools/build-dashboard.ps1`, `check-dashboard.ps1`: offline target
  build and framework/resource checks. Check parameters in current code.

Detailed provenance lives in the project's `source/docs/` documents,
de-identified `source/native/tests/fixtures/` and third-party notices. The MOST
transport originated in upstream PR #9 (initial adapted revision
`e05b5d334e19eb78ebf1163d7f55e7968db1c572`); later merge work preserved an
additional raw-message callback needed by the HUD. Do not overwrite such local
extensions by taking one side of a transport merge wholesale.
