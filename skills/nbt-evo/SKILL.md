---
name: nbt-evo
description: Develop, port, audit and troubleshoot software for BMW NBT Evo (iDrive 5/6), including QNX plugins, SSH/Qnet access, CDS telemetry, MOST diagnostics and reversible deployment. Use for NBT Evo head-unit work; do not assume these interfaces apply to CIC, EntryNav or MGU/iDrive 7.
---

# NBT Evo engineering

Work from the actual head unit, framework binaries and ECU responses. Keep
head-unit compatibility, display layout and engine signal calibration separate:
a working menu does not establish correct diagnostic values or ECU controls.

## Start with the relevant evidence

Identify what the user needs: connectivity, an offline port/build, live data,
deployment, or an ECU/terminal control. Respect existing project instructions
and authorization. Do useful offline work when the vehicle is unavailable;
do not attempt vehicle access merely because credentials or an address exist.

Read only the references needed:

- [Connectivity](references/connectivity.md): SSH, Jacinto versus OMAP, Qnet,
  private filesystem capture, and ENET sleep/wake failures.
- [Native build and ABI](references/native-build.md): framework fingerprints,
  reconstructed C++ declarations, QNX toolchain and deployment transactions.
- [Telemetry and diagnostics](references/telemetry.md): CDS versus HSFZ/MOST,
  parameter profiles, identification, freshness and transport lifecycle.
- [Vehicle controls](references/controls.md): HUD, fault clearing, exhaust flap
  and xDrive, where a command changes more than a displayed reading.
- [F30 N20 / Evo Y case](references/f30-n20-evo-y.md): concrete results from
  the originating project. Consult as a case study, not universal defaults.

When instructions or old documentation conflict, check current code, dated
captures and owner feedback. Historical smoke tests and old candidate notes
must not silently become claims about the present build.

## Essential distinctions

- SSH to the HU and diagnostics to the vehicle gateway are different endpoints.
  Successful SCP does not prove the same SSH client/options/authentication work.
- OMAP hosts HMI/plugins; an SSH session on Jacinto may require Qnet to reach
  OMAP files. Verify the node before writing anything.
- Prefer CDS for exposed routine vehicle readings and user units. Use a proven
  internal diagnostic path for remaining data. Do not assume external ENET
  diagnostics remain available after sleep without a tester.
- A MOST DME tunnel is not demonstrated access to every ECU or every service.
  Reusing a tunnel does not validate an engine's identifiers or scaling.
- Do not infer engine family from displacement, cylinder count, badge or a
  plausible gauge value. Unknown identity/mapping must stay explicit.
- Exact lengths, service/identifier prefixes, sentinels, plausible ranges and
  per-sample age determine validity. Never replace unavailable data with zero,
  a constant ambient pressure or a renewed timestamp on cached values.

## Scope of live actions

Use bounded parked reads for discovery. Live installation, HU restart and ECU
state changes need authorization for that action; reuse authorization already
given for the current scope. Preparing and testing a build offline does not
itself authorize deploying it. Routine read access is not authorization for
fault clearing, coding, output control, drivetrain switching or service changes.

Keep raw identification, VINs, credentials, filesystem captures, SDK licenses
and proprietary libraries private. Publish de-identified fixtures and provenance
for observed formulas. Preserve third-party notices when adapting a transport
or HUD implementation.

## Finish with a reviewable result

For code changes, run relevant host tests first, cross-build for the actual
target and check imported symbols/resources before proposing a live install.
Use existing project scripts rather than inventing a second deployment path.
Report separately what passed simulation, static checks, target loading,
parked reads and sleep/wake testing. Keep remaining uncertainty attached to
the specific interface, signal or control it affects.
