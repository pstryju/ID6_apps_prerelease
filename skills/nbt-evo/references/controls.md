# Controls and terminal output

Do not treat successful read-only diagnostics as validation of writes. Establish
the exact target, software identity, service semantics, guards and recovery for
the requested operation. Retain existing user confirmations and state checks.
Offline preparation can continue while live mutation remains unapproved.

## Fault memory

Ordinary DME read/clear services do not inherently require an MHD or Bootmod3
tune. The studied app uses `19 02 AF` for reads and `14 FF FF FF` for clear;
confirm target support and status masks. Preserve read access even if clearing
is refused. Clear is a separate state-changing action.

After a positive clear acknowledgment, re-read. A refused clear must not become
a blind retry loop. If a known refusal requires an extended session, repeat
clear only after a positive session acknowledgment and return to the normal
session where appropriate. Bound and surface unknown results. MOST reply caps
can truncate large DTC lists: do not announce an exhaustive list when capped.

## Factory HUD

A factory windshield HUD and a cluster display are different profiles.
Terminal FBlock/instance/node and available handles must match observed hardware.
Do not copy cluster instance IDs into a windshield HUD merely because the
protocol core compiles. Keep renderer, terminal protocol and data producer
separate so target-specific placement is explicit.

Wait for a unique supported registry target, target-up and fresh inputs.
Factory navigation must win: combine verified guidance state with observed
terminal ownership where supported. Unknown navigation state should block an
experimental overlay. On timeout, wrong/duplicate target or stale data, stop
drawing instead of issuing stronger terminal commands. Clear only owned output
when the terminal is available and factory navigation does not own it.

Do not add terminal instance creation, communication disable, coding or firmware
changes as a workaround for unknown handles. Start an unverified overlay disabled
after reboot; persistence is a separate feature decision after runtime validation.

## Exhaust flap

Factory hardware and a stock tune do not by themselves establish which
diagnostic control works. An MHD-specific control requires its matching tune;
another app's success over HSFZ does not prove the same request passes MOST.
Keep hardware presence, DME service support and transport support separate.

`2F 60FD` appeared in investigated control code for other DME families; it was
not validated for the studied N20. `22 60FD` is only an identifier read: success
would not prove that a particular `2F` payload is safe or effective. Do not send
guessed output-control bytes, sessions or unlocks based solely on the DID number.
Prepare identification/read probes and investigate exact software-specific
command evidence before a user-authorized parked actuation test.

## xDrive / DSC / VTG

The demonstrated DME tunnel does not establish DSC, VTG or EGS reachability.
Keep the known transport until an alternative is demonstrated. Check supported
module identities and capabilities and verify all required state reads; unknown
Park/RPM/speed is not a safe substitute for confirmed values. In the case app,
both AWD-to-RWD and RWD-to-AWD operations require the engine off and a separate
confirmation. Do not generalize its DSC identifiers or write payload to another
vehicle. A positive write acknowledgment still needs a verified state readback.
