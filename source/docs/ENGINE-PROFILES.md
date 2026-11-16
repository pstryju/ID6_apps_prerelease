# Engine signal profiles

Gauge mapping is separate from transport and UI:

- `native/src/profiles/n20.inc`: the owner's stock N20 MOST query list,
  payload widths, signedness, sentinels, scale/offset, plausibility bounds,
  output slots, refresh periods and output conversion.
- `profiles/reference.inc`: the original B58 candidate DID decoder table.
- `profiles/obd.inc`: standard OBD decoder table for the reference/probe paths.
- `signal_decode.h`: common bounded decoder, with no engine-specific formula.
- `engine_profiles.h`: profile registry and selection; `boost_most.c` handles
  transport, scheduling, ambient freshness and publication.

These are source data tables. Editing mappings requires rebuilding the workers,
but changing the selected existing profile does not. BoostDeck and HUD share
the same table. CDS readings and Evo units are independent of the engine map.
Keep the two builds' internal wire units unchanged: boost psi, rail/oil bar,
IAT C, gasoline-equivalent AFR. The UI converts for display.

## Selection

`Install-Evo.cmd -EngineProfile n20` preserves the current car's mapping and is
the default. The installer adds a checksummed `engine-profile.txt` to BoostDeck
and/or HudTest and records the choice in the deployment receipt. The worker
reads this file beside its own executable; a missing file preserves legacy N20
behavior. Invalid/unreadable settings fail rather than falling back silently.
An explicit worker `--profile` argument overrides the file for testing.

`Install-Evo.cmd -EngineProfile auto` enables experimental identification:

1. Open MOST and read only DME `22 F150`, `22 F187` and `22 F189`, once per run,
   under the shared diagnostic lease and bounded process alarm.
2. Accept only an explicit engine-family token (for example `N20B20O0`) in a
   positive identification reply. Conflicting families reject selection.
3. Unknown identification or a family without a validated mapping produces no
   diagnostic gauge reads. CDS RPM/temperatures continue independently.

This is not proven automatic identification on a real car. Those DIDs may
return part/software numbers without an engine designation. The worker does
not infer family from plausible gauge replies, vehicle badge, displacement or
VIN, and does not yet contain an ECU part-number compatibility database.
Do not enable auto on the current car until its identification is checked.
Raw identification is not logged by the normal worker.

CDS `engine.info` is present in the copied Y remoting library. The published
[CDS reverse-engineering inventory](https://bimmergestalt.github.io/BMWConnectedAnalysis/cds/)
describes cylinder count, displacement, fuel and gearbox fields, rather than
an N20/B48/N13 family. Four cylinders and two litres cannot distinguish N20
from B48. No runtime `engine.info` response has been captured from these cars.

## B48 and N13

Their registry entries are deliberately empty. `-EngineProfile b48` or `n13`
does not reuse the N20 table and therefore does not show diagnostic gauges.
Some DIDs may be shared, but family alone is not proof of matching DME software,
payload length or calibration. Even the N20 profile retains the stationary
capture limitations in [BOOSTDECK-MOST.md](BOOSTDECK-MOST.md).

Next parked check: run `Test-Evo-Engine.cmd` on each car. It performs only the
three identification reads above (no VIN request, install, ECU write or HU
restart), saves the private capture in `.local/diagnostics`, and is bounded to
25 seconds. An ENET cable is needed for the laptop's SSH access, but all three
DME reads run inside Evo over MOST. Check gauge replies separately with the
existing read-only MOST probe and independent readings before filling a new
profile. Add exact DME identification evidence before allowing auto selection
for that software variant. Never commit private identity captures.

Host tests cover the existing N20 capture, unsupported and conflicting families,
absent explicit identity, payload shape, sentinels, read allowlists and cleanup.
Synthetic identification replies test selection logic, not real DME support.
