# HUD observer incident: offline review

## Evidence retained

On 2026-11-17 the observer received client address `2062`, waited for its
45-second window, printed `HUD_TRACE COMPLETE records=0`, then crashed in
target `/fs/sda0/lib/libstdc++.so.6`, reported as
`__gnu_cxx::__exchange_and_add(...)+0x40`, null reference. The owner observed
an Evo restart afterwards. The raw private log is
`.local/diagnostics/20261117T164846Z-hud-trace.txt`.

Observer ELF SHA-256:
`4dfe6e26bb4e76210bf45666210daee01f326e742da371a4a31640c41ebe8dfe`.
The ELF remains in the private build output for examination. It must not run.
No target cleanup or additional SSH access was performed during this review.

Both entry points are blocked before connection; the normal QNX build excludes
the observer. Its source now also fails an accidental direct compilation.
Formatter-only host tests do not execute a DSI client and remain usable.

## What the order of events establishes

The COMPLETE line is after unregister target/client, stop, and thread join in
our main function. The reported fault therefore occurred after that point,
during subsequent process completion, rather than while formatting a received
HUD record. This does not prove which destructor, callback or library object
faulted, nor rule out earlier memory corruption. A core backtrace is needed.

The unit restart must be treated separately. A failed subprocess alone is not
an adequate explanation of why the HU restarted. The temporal relationship
does not establish the restarting component or watchdog trigger.

## Runtime differences found offline

| Component | Observed direct ELF dependencies |
| --- | --- |
| Observer, compiled as C++ | libsocket.so.3, libstdc++.so.6, libm.so.2, libc.so.3 |
| Existing C MOST probe | libm.so.2, libsocket.so.3, libc.so.3 |
| Copied DSI base cpp-ne library | libsys_dsi_servicebroker.cpp-ne.so.4, libcpp-ne.so.4, libc.so.3 |

Thus the observer adds GNU C++ runtime participation to the process compared
with the exercised C probe, while the dynamically loaded DSI path uses the
other QNX C++ library family. This is a teardown/ABI/load-order lead, not proof
that loading both runtimes is inherently invalid or caused the restart.
The existing graphical renderer also needs a lifecycle audit; do not change
its runtime libraries merely on the strength of this comparison.

The copied DSI base's `DSIClientFree` at `0x3e58` checks for a null pointer and
branches to `memset(app,0,224)` through the PLT. Its name must not be interpreted
as proof of a C++ destructor or full resource disposal. The reconstructed
client buffer/callback ownership and DSO completion remain relevant.
Do not bypass finalization with `_exit` and call that a complete repair: it
would not establish safe routing/registration or explain a unit-wide restart.

## Reception was never established

The observer log proves MOST client registration only. It installed no
registry/target-up diagnostic callbacks and counted only FBlock 82 instance 00
messages of selected kinds. Zero records cannot distinguish absent target,
server routing/filter policy, unsuitable subscription, or no matching traffic.
Client creation is not evidence of promiscuous reception.

The copied `/opt/sys/bin/most` has SHA-256
`b114af0fed2a02a89245c133b79f68e0df0120084b5b67ab1fb4abba444af511`.
Its strings describe per-client message lookup/dispatch, registration of
device controllers, registry filtering with permission checks, and a component
trace tag `sys_most_mss_MOSTSpy`. That tag is not a recovered API or role name.
The binary also links watchdog/heartbeat libraries; this is an architectural
dependency, not evidence that a particular watchdog caused this incident.

An arbitrary new MOSTSubSystem client cannot be assumed a safe bus mirror.
Do not try guessed role names, communication-control requests or ownership
changes to obtain other clients' messages.

## Safer investigation paths

Continue recovering factory geometry/init from the private Nav binary and
inspect existing native trace infrastructure offline. The copy contains
`opt/sys/etc/persistTracemost.bin`, `Trace.tr`, `Trace_bolo.tr`, `tracectl` and
system trace libraries. These are leads, not instructions to enable logging
or edit persistence on the car.

In particular, `opt/car/bin/startSyslinkTraceDaemon.sh` deliberately disables
that old trace path and documents a filesystem-fill issue. It is not a suitable
replacement to enable blindly. Prefer a documented, bounded existing trace
source once its cost, permissions and relevant output have been established.

When live access is available again, useful evidence is the preserved process
core, the actual loaded-runtime identities and existing HU/service/watchdog
logs around the timestamp. Collect only necessary files privately; no full
filesystem scrape, service restart or repeat observer run is required for that.
Do not erase the failed run directory or dumps before reviewing this evidence.

No new executable observer or initialization writes are ready for a vehicle
test. The HUD viewport and navigation handover diagnosis remains open.
