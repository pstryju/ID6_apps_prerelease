Current candidate: ShiftDeck remains as below. BoostDeck now uses MOST for DME readings and CDS for RPM/temperatures. See [BOOSTDECK-MOST.md](BOOSTDECK-MOST.md). The all-CDS BoostDeck layout below describes the previous candidate.

# Standalone CDS gauges for the owner's Evo Y / F30 N20

The owner confirmed the separate CDS test works after vehicle sleep/wake.
The photo shows RPM 703 with 180 events, speed 0, engine temperature 78,
oil temperature 75 and torque 0 with 123 events. This verifies native callbacks
and candidate values, not torque accuracy under load or all additional channels.

The dashboard candidate now uses BD_NATIVE_CDS. ShiftDeck uses RPM and speed
only. BoostDeck retains its name, connector and geometry, with these nine cards:

| Card | CDS property / field | Canonical unit |
| --- | --- | --- |
| RPM | engine.RPMSpeed / RPMSpeed | rpm |
| Speed | driving.speedActual / speedActual | km/h |
| Torque | engine.torque / torque | Nm |
| Accelerator | driving.acceleratorPedal / position | percent |
| Gear | driving.gear / gear | 1 N, 2 R, 3 P, 4..11 D1..D8 |
| Fuel | sensors.fuel / tanklevel | litres |
| Outside temperature | sensors.temperatureExterior / temperatureExterior | C |
| Engine temperature | engine.temperature / engine | C |
| Oil temperature | engine.temperature / oil | C |

Pedal, gear, fuel and outside temperature are documented candidates; they have
not yet been observed on this N20. No diagnostic fallback is used. Missing,
malformed, sentinel, out-of-range or expired data displays `--`. The engine
temperature card deliberately says ENGINE TEMP, not a newly validated coolant
sensor. Zero torque is permitted but is not a claim of accuracy at idle/load.

## Evo units and lifecycle

vehicle.unitSpeed selects 1 km/h or 2 mph. vehicle.units.temperature selects
1 C or 2 F; fuel selects 1 litre, 2 UK gallon, 3 US gallon. Settings are independent:
using Fahrenheit does not imply mph. Unknown/missing settings blank affected
readings rather than silently selecting a locale. The candidate XML removes
both manual Units buttons and actions; color and shift-light options remain.
Torque is explicitly labelled Nm: sportTorque enum mapping has not been
established here, so it is not guessed from temperature or speed settings.

Subscriptions begin one second after a screen opens. Callbacks capture bounded
numeric JSON under a mutex; the HMI tick updates models. Asynchronous CDS reads
every second renew unchanged values/settings. Each output expires after 3.5 s
without a valid response. Errors immediately invalidate the affected channel.
Leaving/finalizing the screen removes subscriptions and invalidates data;
reopening starts fresh. No keep-alive or ECU writes are involved.

Neither gauge opens sockets or launches a diagnostic worker. Their staged
folders contain only manifest, XML, shared library and images. EngineCodes
retains its separate HSFZ worker, read and confirmed-clear functionality.
The reference build.sh/profile still uses its original transport and resources;
the CDS switch is confined to build-dashboard.sh and staged candidate resources.

## Investigation of the missing original measurements

Offline audit found 233 property-like names in this Evo's libhmiremoting.so,
including eight engine.* names. It found no named CDS property for boost, AFR,
ignition timing, intake temperature, rail/oil pressure or transmission temperature.
This is a string inventory, not proof that every internal service was exhausted.
Run tools/audit-cds.ps1 to produce a private hash-tagged inventory under .local.
Dynamic api.* properties and lower-level vehicle services remain a separate
research path; no guessed property or conversion is enabled in these gauges.

The iDrive7 car_vitals and Cluster projects expose boost through FES and still
use diagnostic reads for several other engine channels. They target a different
platform and do not establish a native CDS mapping on NBT Evo.

Primary sources inspected:

- https://bimmergestalt.github.io/BMWConnectedAnalysis/cds/
- https://github.com/BimmerGestalt/AAIdrive/blob/main/app/src/main/java/me/hufman/androidautoidrive/cds/CDSValues.kt
- https://github.com/BimmerGestalt/AAIdrive/blob/main/app/src/main/java/me/hufman/androidautoidrive/cds/CDSMetrics.kt
- https://github.com/BimmerGestalt/AAIdrive/issues/870
- https://github.com/cauliflowermossgrape/idrive7-apps/blob/main/apps/car_vitals/app.json
- https://github.com/cruzen991/Cluster

## Installation and checks

Run `Install-Evo.cmd -AppSet Gauges` while parked, then restart iDrive manually.
This replaces BoostDeck and ShiftDeck together, retaining rollback backups.
Use `Uninstall-Evo.cmd -RunId ID` with this installation's ID to restore them.
The separate CDS test can remain installed, but close its screen for testing.

Check both menus, mixed Evo unit settings, zero/idle readings, screen reopen,
and sleep/wake without OBD. Confirm extra channels against the car before
relying on them. New integrated gauges still require this owner runtime test.
Host CDS tests cover captured payloads, error objects, invalid numbers, duplicate
keys, per-output expiration, settings changes, refresh and subscription reopen.
Static candidate checks reject gauge socket/worker imports and manual unit menus.
