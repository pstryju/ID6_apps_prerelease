# Factory navigation and HUD terminal initialization

## Trace quarantined after target incident

Do not run the observer. On 2026-11-17 it registered DSI client 2062 but
captured zero terminal records. After printing COMPLETE it terminated with
SIGSEGV in target libstdc++.so.6 `__gnu_cxx::__exchange_and_add`, with null
reference. The owner then reported an Evo restart. A unit restart cannot be
explained as harmless process-local teardown; causality requires investigation.
Both launchers are blocked before SSH, and the build no longer emits the tool.
An older executable can remain in private output or temporary target folders;
its existence is not authorization to run it directly. No further target
experiment is part of the recovery. Instructions below are historical only.

The observed client registration establishes only transport initialization,
not successful passive reception. Registering the same terminal target has
not been proven non-disruptive. Offline review must separately examine DSI
registration ownership and process-exit C++ runtime/library compatibility;
skipping destructors alone would not establish a safe observation mechanism.

## Offline evidence, 2026-11-17

Analyzed the private copied `opt/nav/eceusa/bin/NavNBTEvoController`, SHA-256
`82b3eee9b737710d88a02c9a20af9e9a50809d23b32c616bacb2f6b6b71d74b6`.
The executable has dynamic symbols but the relevant implementation is stripped;
objdump's nearby THBVector symbol labels are not names of these functions.
The following locations were found by ARM PC-relative references to trace
strings, then checked in bounded disassembly:

| Location | Observed evidence |
| --- | --- |
| `0x6de388` | HUD-only communicator has an initialization-state guard |
| `0x77cfec`, trace reference `0x77d014` | IC/HUD instance initialization for TopFrame 11–13 |
| `0x77d080–0x77d0a4` | That initialization passes selector/type pairs `(5,3)` and `(6,3)` to helper `0x7780f0` |
| `0x780cf4–0x781c98` | Repeated instance-to-frame attachment code and trace references |
| `0x782c7c–0x782c9c` | Completion trace, then attachment helper `0x777874` and virtual continuation |
| `0x771024–0x77107c` | ImageSetPositionRotation result handler logs five signed 16-bit fields: coX, coY, ctX, ctY, angle |

Other trace strings establish HUDInit/connection notifications, pending init,
instance existence checks and distinct HUD-only versus instrument-cluster
initialization paths. The executable contains handlers for frame attachment,
position/rotation, fill mode/color and visibility.

These observations show that the factory implementation has explicit setup
and geometry state. They do not recover actual on-wire initialization values
for this windshield. In particular, creating local controller instance objects
does not prove that it sends MOST CreateInstance (`200`) at that point. Selector
numbers and returned physical terminal handles must not be assumed identical.
TopFrame 11–13 should not simply replace our frame 1 without tracing the mapping.

## Consequences for our renderer

The current renderer reuses frame 1, textarea 2 and image handles 5/6. It sets
fill color, adds polygons and switches image visibility. It does not establish
image transforms, clipping/layout, frame attachment or the factory init state.
It therefore relies on pre-existing terminal configuration.

The owner's observation that idle fill becomes visible after a navigation cycle
weakens the earlier fixed-left-clipping hypothesis. The committed x=16 shift
is an unverified layout workaround, not a proven initialization repair. Do not
use that change as evidence that the actual drawable viewport is known.

Temporary overlap at navigation start can arise before CDS reports guidance or
before a terminal copy is recognized. At route end, factory cleanup can occur
after CDS reports idle; our raw-terminal observer may then yield again. The
reported disappear/reappear sequence fits this, but a timed capture is needed
to distinguish it from geometry/visibility reconfiguration and reply-copy
classification. Do not replay unknown initialization writes based on strings.

## Passive terminal capture prepared

`Test-Evo-HUD-Trace.cmd` uploads a temporary observer, not a plugin. It opens
a DSI client/target registration to receive `82/00` traffic; it sends no HUD
method calls, DME requests, coding, restart or drawing commands. Registration
itself is required transport activity, so passive means no terminal writes,
not absence of any DSI traffic.

The native observer is bounded to 45 seconds / 4096 records, with a shutdown
watchdog. It records addresses, function, operation, transport metadata and up
to 64 payload bytes. Text/bitmap contents are redacted; full logs remain in
ignored `.local/diagnostics`. Position/rotation payload decoding is an aid;
raw bytes remain the primary evidence. Captured replies may be copied or
status records, not necessarily the original factory requests.

Build using the regular dashboard build script. The trace executable is output
as `source/native/out/dashboard/hud-trace-qnx` and is not installed into HudTest.
The new observer has been cross-built; actual reception on this target still
requires a parked check.

For capture, keep our HUD overlay OFF. Ideally begin before the first route
after wake, then run:

```powershell
.\Test-Evo-HUD-Trace.cmd
```

Wait for `HUD_TRACE START`, start factory guidance, wait about 10 seconds, then
cancel guidance and leave the overlay off for the remainder. Do not trigger an
automatic HU restart just to collect the trace. Capture a later cycle separately
if a cold-start sample is unavailable.

Compare functions `200`, `22B`, `C06`, `C04`, `204`, `20E`, `21B`, `21C` and
`220`, actual handle assignments, coordinate values, visibility ordering and
the timing of final cleanup. Only then define the minimal required setup and
resume boundary. An absence from a DSI observation stream is not proof a
function was never sent.
