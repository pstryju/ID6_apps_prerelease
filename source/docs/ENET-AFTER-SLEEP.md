Historical investigation. CdsProbe has now been retired after the successful owner test; use the integrated apps and Remove-Evo-Tests.cmd to remove the old test menu.

# Diagnostic transport after vehicle sleep

Status: offline investigation; no vehicle connected. No SSH attempts, coding,
reboots, service restarts, wiring or network changes made for this investigation.

## Observations and decision

Gauges worked before vehicle sleep. After waking with the OBD socket empty,
EngineCodes reported `Gateway 169.254.20.21:6801 unavailable (Connection timed out)`.
The owner subsequently plugged an ENET cable into OBD without a laptop during
an existing drive; this did not restore data. Cable insertion alone is therefore
not a demonstrated recovery method on this vehicle. This observation does not
isolate activation, physical link, address or routing as the exact cause.

A TCP timeout occurs before the OBD/UDS decoder. Earlier successful reads
validated the external HSFZ endpoint while available, not standalone operation
after sleep. Retrying or changing the temperature conversion cannot repair that
transport. External ENET is unsuitable as the assumed always-on gauge source.
Fault-memory access remains separate; no read/clear functionality was removed.

EngineCodes opens a fresh connection per read; gauge workers reconnect after
failure. A retained pre-sleep TCP socket cannot explain all the failures.
The copied EthernetDiagnosticsSlaveHU mentions VLAN73 and routing, but provides
no verified internal DME transport. Earlier internal .99/.253 TCP tests produced
no expected DME reply. No unverified activation or keep-alive workaround added.

## Native CDS candidate

The supplied Y libAppFramework exports native PluginApp methods:

- cdsAddPropertyChangedEventHandler at 0x7f64, 76 bytes;
- cdsGetPropertyAsync at 0x7fb0, 68 bytes;
- cdsRemovePropertyChangedEventHandler at 0x5854, 84 bytes.

The add method copies a two-word Delegate into the existing PluginApp map at
base offset 296. The SDK adds nonvirtual declarations only; no class layout or
vtable changes. Static import verification passes against the copied libraries.
These facts establish build compatibility, not runtime CDS availability.

A separate CdsProbe app (connector 114, My Vehicle / CDS test) subscribes only
while its screen is visible. It requests an initial value and subsequent changes
for engine.RPMSpeed, driving.speedActual, engine.temperature and engine.torque.
Names and example payloads come from the primary reverse-engineering project:
https://bimmergestalt.github.io/BMWConnectedAnalysis/cds/

It displays bounded raw JSON, event count and seconds since the last event.
Age is not a freshness verdict: unchanged properties may not emit events.
An error JSON is evidence of a CDS error, not a numeric reading. The published
engine.temperature contains engine/oil fields; actual semantics, invalid values,
units and availability on this N20 still require checking before gauge migration.
Boost, rail, AFR and oil pressure are not established CDS channels here.

The test has no worker, socket, HSFZ session or ECU/CDS setter. Existing gauges
are unchanged. Compilation and static checks passed; runtime is unverified.

## Next parked test, when the vehicle is available

Install only the separate test using `Install-Evo.cmd -AppSet CDS`, then restart
iDrive manually. Open My Vehicle / CDS test while parked. Record the four raw
rows and whether event counts advance. RPM should respond to engine speed;
stationary speed zero alone does not establish a live stream.

Disconnect ENET, allow a full vehicle sleep, wake normally without an OBD cable,
and reopen the test while parked. Check that values arrive again. Do not connect
SSH before this observation: connecting a laptop can change the network state.
This is a proposed test, not a completed validation. No need to repeat the bare
cable insertion experiment. Remove the test via `Uninstall-Evo.cmd -RunId ID`
using the deployment ID printed during installation, and restart manually.

## Subsequent result

The owner reported that CDS also works after vehicle sleep. The integrated CDS gauge candidate and remaining limits are documented in [CDS-GAUGES.md](CDS-GAUGES.md). The proposed bare-cable experiment is closed; external ENET is no longer the gauge transport.
