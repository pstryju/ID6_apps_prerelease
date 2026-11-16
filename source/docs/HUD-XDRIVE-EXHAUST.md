# Optional apps for Evo Y

## HUD Gauges (HudTest, connector 107)

Owner feedback confirmed that the first HUD PoC renders RPM. Boost appeared
only after a factory-navigation cycle. Navigation briefly appeared then the
canvas became blank; one fast navigation/OFF/ON sequence coincided with a full
HU restart. The exact cause of the restart is not established.

### Current repair candidate: RPM only

The new candidate keeps only the proven text handle 2 and shared frame 1 at
unique terminal 82/00, node 0101. It no longer writes boost text handles 3/4,
listens for boost UDP or launches/packages a diagnostic boost producer.
RPM and guidance status come from CDS. No engine profile is needed for HUD.
The existing terminal core and new DSI adapter remain; licenses are retained.

A code defect matched the navigation disappearance: a non-idle CDS state made
the renderer call clear, which hid frame 1 while the MOST navigation observer
could still report ready. The repair adds an independent CDS gate inside the
terminal core: guidance active/unknown drops queued calls without clearing,
hiding or drawing. Raw MOST navigation still has priority even if CDS briefly
reports idle. Missing/stale guidance blocks output. On return, redraw starts
with a fresh batch, not an old queued visibility request.

Normal stop clears only our RPM text when the terminal is ready and the last
CDS guidance sample is fresh/idle. It never hides the shared factory frame.
Failed terminal state is not forcibly reset to send cleanup. Only one request
is in flight. Quick OFF/ON stops feeding the old renderer and waits until that
process is reaped before launching/feeding its replacement. Repeated identical
checkbox events do not resubscribe CDS.

Boost's dependence on a navigation cycle suggests uninitialized/invisible
terminal text instances; it does not establish a failed diagnostic read. That
initialization hypothesis is not proven without target traffic/instance evidence.
The new RPM-only candidate avoids those unverified instances altogether.

The checkbox still starts OFF after HU sleep/restart. Autoactivation is not
implemented. A graphical RPM sweep/shift lights is the intended next feature,
not part of this stability repair; no guessed image instance or instance creation
has been added.

### Build and parked regression check

Build/check with the existing dashboard scripts, then install only this app:

```powershell
.\Install-Evo.cmd -Apps HudTest
```

After manual iDrive restart, check initial RPM without starting navigation,
navigation start while RPM is visible, continued factory guidance, route stop
and automatic RPM return. Then check OFF/ON with navigation idle and active,
and sleep/wake. Unknown or active guidance must never trigger cleanup commands.
The repair passes host tests and requires a new parked target check; it does not
prove that the reported full HU restart cannot recur.

The staged HudTest has six files: manifest, XML, plugin library, one renderer
worker, LICENSE.txt and NOTICE.txt. Installation replaces the app folder, removing
the old boost worker/config. Rollback remains receipt-based. No runtime vehicle
control command, coding or terminal-instance creation is introduced.

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
