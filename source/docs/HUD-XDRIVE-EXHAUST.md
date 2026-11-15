# Optional apps for the owner's Evo Y / F30 328i xDrive N20

Status: prepared and tested offline. The owner confirms factory windshield HUD
and a factory exhaust flap. Neither optional app has been loaded on this car.
No vehicle connection, ECU operation or installation was made during this work.

## HUD Gauges (HudTest, connector 107)

The original archive's renderer accepts only the old 169.254.21.3 gateway and
uses the old diagnostic path. That known limitation was not shipped as a Y port.
The candidate rebuilds the plugin against the copied Y frameworks and uses a
new display adapter with the existing HUDTerminal protocol/core.

This first candidate displays text RPM and boost in bar, rather than the old
curved tachometer graphic. RPM comes from native CDS; a dedicated MOST producer
reads only DME 224201/224205 for gauge boost. No external diagnostic gateway is
used. The UI menu can be left while enabled. The checkbox starts OFF after an
iDrive restart; automatic boot persistence is deliberately not included before
this car's HUD profile is verified.

The packaged windshield profile's instance handles 1..6 and unique HUDTerminal
82/00 at node 0101 are retained, not the neighbouring project's 6WA display
profile. The reference binary's setup enables these handles and nav frame 1.
The new minimal view uses text instances 2/3/4 and frame 1; their exact physical
placement and text clipping on this car remain unverified. It sends no instance
creation, MOST communication disable, HUD coding or cluster firmware change.

The renderer waits for client registration, a unique supported registry host,
target-up, fresh CDS RPM, and a CDS navigation status indicating idle. Unknown
navigation status blocks drawing. A raw MOST observer also feeds the terminal
core so factory-navigation activity wins over the overlay, including copies of
the terminal's replies. It pauses on wrong host, duplicate host, timeout, error
or stale input. Turning it off clears its texts/frame when the terminal is
available and not owned by navigation, then unregisters the DSI client.

HUD and BoostDeck have separate DSI names and share the existing per-request
diagnostic lease. Simultaneous HUD and BoostDeck clients still need a car test.
The HUD producer omits AFR/timing/rail/oil scans to reduce bus traffic.

```powershell
.\Install-Evo.cmd -Apps HudTest
```

After manual iDrive restart, open My Vehicle / HUD Gauges and enable Show HUD
gauges while parked. Check text placement, fresh RPM, idle boost, disabling,
factory-navigation priority and sleep/wake with no OBD. The menu status reports
missing/unsupported terminal, missing CDS/navigation state, active HUD, nav yield
or terminal failure. If the overlay fails, switch it off and restore the previous
deployment. An absent boost reply stays `--`; idle near zero is expected.

HudTest's seven local files include two workers and the source/notice licenses.
The terminal core/protocol are PolyForm Noncommercial 1.0.0 from NBT-AAD;
third-party/hud-NOTICE.txt records the derivation. No proprietary libraries
are distributed with it. The new adapter uses the same base DSI API exercised
by the earlier MOST DME probe, with a separate 82/00 target.

## xDrive Control (connector 112)

Rebuilt for Y with gateway-ip.txt set by the installer to 169.254.20.21.
The old hardcoded gateway in the plugin was replaced for this build.
It retains HSFZ: the demonstrated MOST tunnel reaches DME, not the DSC, VTG
and EGS required by this app. Thus it is not established as cable-free.

```powershell
.\Install-Evo.cmd -Apps XDriveControl
```

Restart manually and begin with Read transfer case mode. This sends only reads.
Unknown DSC or unavailable capability/state blocks control. The existing
allowlist is DSC indices 0F2510 and 0F2850; restore-by-button is only implemented
for 0F2850. The second screen explicitly confirms a requested change.
Immediately before the single 2E DBE1 write the worker requires the expected
EGS DA2E Park response, zero DME RPM and zero speed. Ignition must be on, engine
OFF, Park selected and parking brake applied for either direction. Parking-brake
application is an owner requirement, not an electronically verified input here.

The DSC indices, VTG DB32 interpretation and especially EGS DA2E mapping were
inherited from the upstream profile; they are not validated on this F30.
A successful installation or menu does not validate switching. Read the real
module/profile evidence before a first write. A request is only reported as
confirmed if VTG readback agrees; an acknowledgement alone is insufficient.
An ignition cycle is the upstream restore path for the temporary RWD request;
this is not an xDelete calibration/flash. Uninstalling an HMI menu does not itself
restore or verify drivetrain state.

The archive source's BM3/MHD controls remain excluded from the candidate.
Host tests exercise read-only status, unknown DSC, missing capability, missing
Park, nonzero RPM/speed, mismatched readback and both supported write directions.
Those simulated tests establish code guards, not real-car compatibility.

## Install together / rollback

`Install-Evo.cmd -AppSet Extras` selects only HudTest and XDriveControl.
No automatic restart or ECU command is executed by installation.
Preserve the deployment ID. `Uninstall-Evo.cmd -RunId ID` restores the prior
app folders with the usual verified transaction. Individual installations are
recommended for isolating the first runtime tests.

## Stock exhaust flap investigation

BimmerLink describes exhaust-flap control for factory-flap-equipped vehicles,
without stating a requirement for a remapped DME:
https://bimmerlink.app/#
This supports investigating stock diagnostic control, not assuming a tune is
required. It does not document the N20's actual command or confirm this port.

The upstream Engine Config command 2F 60FD is specifically documented for the
MG1/DME8 electric actuator on B48/B58/S58. N20 command compatibility must not be
inferred from those engines or from a positive MOST connection:
https://github.com/cauliflowermossgrape/idrive7-apps

Prepared `Test-Evo-Exhaust.cmd` performs only bounded MOST reads of DME F150,
F187, F189 and the candidate 60FD identifier, saving the raw output privately.
It sends no 2F actuation, no session change, no ECU write, and requires parked
confirmation. A rejection of 2260FD is not proof that a different stock flap
job does not exist; a positive read is not proof that 2F control is supported.
Next evidence needed is the actual DME variant and a matching diagnostic job
definition, or an authorized capture of working BimmerLink control on this car.
No unverified open/close commands or permanent override service were enabled.
