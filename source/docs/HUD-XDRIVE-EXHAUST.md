# Optional apps for Evo Y

## HUD Gauges (HudTest, connector 107)

Owner feedback confirmed that the first HUD PoC renders RPM. Boost appeared
only after a factory-navigation cycle. Navigation briefly appeared then the
canvas became blank; one fast navigation/OFF/ON sequence coincided with a full
HU restart. The exact cause of the restart is not established.

### Current graphical candidate: RPM line

The owner confirmed the filled-polygon build draws correctly, but reported
unusable lag while numeric RPM had been fast. The next candidate removes the
renderer-side serial round-trip bottleneck: the baseline and thick segment
form one polygon, and palette setup happens once per acknowledged image-buffer
initialization. Steady updates have three calls: clear back buffer, draw shape,
swap buffers. Construction uses a bounded three-call pipeline, with an explicit
ACK barrier before any swap/frame-visibility operation. No frame backlog is
allowed; only the latest CDS value is drawn when ready. Unchanged pixel widths
are not redrawn. Minimum frame-start spacing is 50 ms; CDS/feed and actual
terminal latency still bound the visible rate. No predicted RPM is generated.

A delayed-response host test finishes a steady frame in two 120 ms round trips
and verifies that an out-of-order answer cannot expose an unfinished image.
Navigation can cancel all unsent work even with construction in flight.
Actual windshield latency remains a target test, not a measured speedup claim.

Owner feedback on the first line build: enabling it immediately produced
"HUD profile/error: overlay stopped", with no drawing. That build's command
sequence has not been validated on the windshield terminal. The updated
candidate replaces line-colour/line-width/two-point polyline commands with
filled four-point strips and ImageSetFillColor, matching the packaged
reference renderer's primitive type. This is a candidate correction, not a
confirmed diagnosis of the observed refusal.

Renderer status is now versioned HUS2 and carries the exact terminal failure
reason. The plugin shows function, handle and error reply (or timeout), rather
than only a generic profile/error message. Install plugin and worker together.
If the first image-clear call on handle 5 is rejected, changing polygon style
cannot fix it; the next step is identifying the actual instance/profile.
No automatic reset, handle sweep or instance creation is attempted.

The latest renderer draws a thin baseline and a thick active segment, 0–8000
RPM from CDS, instead of numeric RPM text. It uses the packaged reference
renderer’s image buffers 5/6, drawing to the inactive image and switching only
after geometry/style calls have been acknowledged. The previous navigation
and OFF/ON repairs remain in place. The numeric textarea is cleared on the
first graphic frame. There is no boost, new instance creation or autoactivation.

The reference ELF `.local/hud-reference/hud-shape-qnx` contains draw() at
0x1047c8 selecting image 5/6 and SwitchVisibility at 0x104eec; its tachBand()
uses x=0..116. The candidate straight line uses that width at y=30, inside
the reference band footprint. Strip heights are 1/5; palette role 4 is a candidate
foreground, not a guaranteed physical colour on this windshield. Geometry,
placement, readability and cold-start availability need a parked target check.

Each batch is complete and bounded (at most six startup calls, three steady
calls). No animation frames queue up: the renderer waits for the current batch,
then draws the latest RPM subject to the ACK barrier and terminal latency.
Navigation cancels unsent drawing/swap calls. Stop removes items only
from image buffers 5/6 and clears text when guidance is fresh/idle; shared frame
1 is never hidden. A terminal error stops output rather than force-resetting.

For the next parked test, install only HudTest and check a short thick segment
at idle, growth with RPM, navigation takeover/return and OFF/ON. If graphics
fail, report the app status and restore the previous deployment. Host tests
cover endpoints, monotonic scaling, wire shapes, buffer ordering, ACK pacing,
cleanup scope and navigation preemption. They do not verify the physical HUD.

### Previous repair candidate: RPM only

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

### Parked MOST identification, 2026-11-17

The complete ExhaustInfo script ran successfully after normalizing CRLF in
the uploaded deploy.sh (not only in the SSH command). Firmware and both target
framework hashes matched. DME F150 returned `62F1500F1810`; F187, F189 and
60FD each returned `7F2231`. MOST open/close succeeded. These are actual
read-only target results; no actuation, ECU write or session change was sent.
The 60FD read rejection does not establish that every possible flap-control
service is unavailable. Raw logs remain private in `.local/diagnostics`.
