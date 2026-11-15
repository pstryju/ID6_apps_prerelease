# BoostDeck MOST candidate / F30 N20 / Evo Y

## Result of the parked test

Two temporary probes ran on the owner's Evo Y with the engine idling and HMI
home visible. The second read eight explicit DME measurements and closed the
DSI client successfully. No plugin installation, restart, DME coding, clearing
or tuning command was performed. Full logs remain private under .local/diagnostics.
De-identified raw replies: native/tests/fixtures/n20-most-readings.json.

Ambient 98.535156 kPa and charge 98.601563 kPa differ by 0.066406 kPa:
approximately 0.000664 bar gauge boost, appropriately displayed as 0.00 bar
at idle. AFR candidate lambda was 1.005631 (gasoline-equivalent AFR 14.783),
timing -2 degrees, intake temperature 33 C, oil pressure candidate 2.517 bar.
BMW rail 2256D7 was rejected (7F2231); standard OBD 0123 returned 102.6 bar.

These are observed replies with existing decoders, not independent calibration
across engine load. The original probe log incorrectly labelled every decoded
DID as kPa; only 4201/4205 are kPa. Units above and in the fixture are corrected;
subsequent probe builds print a per-channel unit. The raw replies are unchanged.

The OBD cable was attached for SSH during the test. MOST was explicitly used,
without connecting to the ENET diagnostic gateway. Standalone operation after
sleep/wake with no OBD cable is still a required owner test. The source PR author
reports that mode on an N20, but that is not proof for this car.

## Candidate implementation

ShiftDeck remains CDS-only. BoostDeck has these cards:

| Field | Source | Display unit |
| --- | --- | --- |
| Boost | MOST 224205 minus 224201 | bar gauge |
| AFR | MOST 225889, lambda times 14.7 | gasoline-equivalent AFR |
| Timing | MOST 224A49 | degrees |
| Intake air | MOST 22580F | Evo C/F setting |
| RPM | CDS engine.RPMSpeed | rpm |
| Rail pressure | MOST OBD 0123 | bar |
| Oil pressure | MOST 22586F | bar |
| Engine temperature | CDS engine.temperature.engine | Evo C/F setting |
| Oil temperature | CDS engine.temperature.oil | Evo C/F setting |

RPM replaces the transmission-temperature card because this verified tunnel
targets DME 12 only, not EGS 18. There is no invented gearbox temperature or
EGS fallback. Pressure is explicitly in bar; no guessed airPressure enum
mapping or C/F-based pressure-unit inference is used. Manual units remain
removed; temperature follows vehicle.units.temperature independently.

The dedicated boostdeck-most-qnx worker has a fixed read-only request list.
It never connects to HSFZ, never sends the rejected rail DID, and accepts no
arbitrary service, target or request argument. The DSI tunnel itself uses
routine 31 01 0F0B as its transport envelope. It sends no inner DME write,
session change, tune/map command, DTC clear or tester-present keep-alive.
Its only sockets are loopback UDP for UI IPC and the shared diagnostic lease.

Each reply is decoded with exact length, prefix, sentinel and range checks.
The worker publishes after each read. Boost requires a measured ambient sample
less than 1.8 seconds old. UI freshness uses sender/sample times, not UDP arrival;
failed reads invalidate their channel. RPM/temperatures can continue through
CDS if MOST is unavailable. Pre-open queued frames are rejected when reopening.
Leaving the gauge screen stops the child and CDS subscriptions. MOST open and
requests are bounded; watchdogs bound a stuck native DSI send/close. SIGTERM
requests cleanup and final publication invalidates all diagnostic fields.

## Build, install and rollback

Build locally with source/tools/build-dashboard.ps1. The BoostDeck folder has
five files, including its MOST worker. ShiftDeck still has four and no worker.
The installer transfers the verified bundle in two SSH sessions and preserves
the prior app folder. It adds no gateway-ip.txt to BoostDeck.

From the repository root while parked:

```powershell
.\Install-Evo.cmd -Apps BoostDeck
```

Restart iDrive manually. No need to replace ShiftDeck for this change. Preserve
the deployment ID; `Uninstall-Evo.cmd -RunId ID` restores that previous app.
`Test-Evo-MOST.cmd` runs the bounded temporary probe, without app installation.

Initial checks: idle values, close/reopen the screen, and retained Evo C/F.
Then allow real sleep and reopen without OBD. Boost across load remains to be
compared against a trusted logger; idle alone cannot validate boost under load.
If MOST cannot open, engine measurements show dashes while CDS fields can work.

## Provenance and offline verification

MOST transport adapted from 0x424D57's PR #9, pinned to
e05b5d334e19eb78ebf1163d7f55e7968db1c572:
https://github.com/inotbarnacle/ID6_apps_prerelease/pull/9
Attribution: third-party/most-pr9-NOTICE; MIT license in source/LICENSE.
The copied DSI library exports all 14 imported functions, and the temporary
probe exercised the same ABI successfully on this Evo.

Host tests run the upstream 130 MOST checks plus new worker cases using the
captured N20 payloads: decoded values, no fabricated boost on missing ambient,
rail rejection, missing service/library, link loss, slow-tunnel SIGTERM and
cleanup. Existing reference transport, CDS and signal tests remain in make test.
Static checks retain ShiftDeck's no-network requirement and verify the exact
BoostDeck worker, BMW imports, resources and absence of manual unit menus.
The integrated plugin still needs its owner runtime and cable-free wake tests.
