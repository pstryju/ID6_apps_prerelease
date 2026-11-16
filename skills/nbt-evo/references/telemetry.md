# Telemetry, diagnostics and engine profiles

## Choose a source per channel

| Source | Useful for | Limits to establish |
| --- | --- | --- |
| Native CDS | Exposed RPM, speed, temperatures, units and other factory properties | Actual property availability, JSON shape, enums and refresh behavior |
| HSFZ over diagnostic Ethernet | Diagnostic services and supported ECU targets | Gateway address, activation after sleep and access without an external tester |
| Internal MOST UDS tunnel | Remaining DME diagnostic readings without an external TCP gateway | Target/service support, latency, DSI ABI, contention and wake behavior |

Do not force all data onto one source solely because a transport exists.
Use measured data-rate needs: RPM from CDS can be useful while slower pressure
reads use MOST. The external diagnostic gateway address is not the HU SSH IP.

## CDS

Native methods exercised on the Y framework are
`cdsAddPropertyChangedEventHandler`, `cdsGetPropertyAsync` and
`cdsRemovePropertyChangedEventHandler`. Check exports and calling signatures
for a new firmware. A binary string inventory identifies candidates, not
runtime-supported properties or field meanings.

Useful observed properties include `engine.RPMSpeed`, `driving.speedActual`,
`engine.temperature`, `vehicle.units` and `vehicle.unitSpeed`. Captured shapes:

```json
{"RPMSpeed":703}
{"speedActual":0}
{"temperature":{"engine":78,"oil":75}}
```

Parse bounded payloads strictly. Invalid/error objects, duplicate relevant keys,
truncation, nonfinite numbers and sentinel enum values invalidate affected
channels. Subscribe at the appropriate lifecycle point, unsubscribe on stop
and clear cached validity. Periodic asynchronous gets can renew unchanged
values, but only an actual response may renew their sample age. Synchronize
framework callbacks with UI consumption.

Read unit settings independently per quantity. Do not infer pressure units
from Celsius/Fahrenheit or guess an enum from its numeric ordering. CDS may
expose litres, km/h and C in canonical values while a different property
specifies user display units.

`engine.info` appears in the studied remoting library. The published
[CDS reverse-engineering inventory](https://bimmergestalt.github.io/BMWConnectedAnalysis/cds/)
describes displacement, cylinder count, fuel and gearbox fields. Those fields
do not distinguish all engine families. Do not invent an `engineType` property.

## MOST tunnel

The studied implementation uses `DSYSMOSTServer`, FBlock `06`, instance `F0`,
FktID `050`, routine `31 01 0F0B`, diagnostic source `63`, for inner DME target
`12`. These are implementation-specific proven candidates, not a universal
BMW diagnostic API. An outer routine-control service is the transport envelope;
distinguish it from the inner ECU service when reviewing a read-only worker.

Open the DSI client, register, observe a matching target and wait for target-up.
Load the matching DSI library dynamically only if supported; `/opt/sys/lib`
may be needed in the library search path. Unregister target/client and stop/join
the DSI thread on teardown. Use distinct client names for concurrent apps.

Serialize diagnostic ownership across workers. Loop workers should lease per
request and yield to bounded one-shot operations. Publish new samples without
waiting for the entire slow scan. Refresh fast and slow channels at different
rates; omit irrelevant queries in a HUD-only producer.

Bound open, response/pending handling, retry and shutdown. Native DSI sends
can block beyond a request timeout, so a process watchdog may be needed.
Signal handlers should request stop; cleanup must not wait forever for a thread.
Keep wrong-prefix replies, malformed sizes, negative responses, no-route and
transport loss distinct. Avoid retrying unsupported identifiers continuously.

HSFZ-first fallback is a policy choice. In the merged upstream implementation
studied here, loops switch after five failed connects, one-shots after one,
and a process that has connected via HSFZ never switches to MOST. This does
not fix unsupported DIDs on a connected gateway. A MOST-only gauge worker is
reasonable when that is the proven intended path.

## Mapping and identification

Store queries and formulas in separate engine/software profiles. Include
service, identifier, target, exact byte count, signedness, unavailable sentinel,
scale/offset, canonical unit, plausible range, freshness, poll period and
evidence. OBD PID `01 xx` is not a BMW DID `22 xxxx`; preserve response-prefix
differences. Family names alone may not identify a compatible DME revision.

For boost subtract a recent measured ambient absolute pressure from charge
absolute pressure before unit conversion. A near-zero value at idle does not
establish a failure; revving without load is not a calibrated boost reference.
Require independent comparison across the intended operating range.

On unknown engines, start with explicit read-only control reads and preserve
raw de-identified responses. Do not accept a profile merely because its values
look plausible. Standard OBD can be an explicit fallback when supported and
validated; latch a successful fallback to avoid repeating a rejected DID.

Read-only DME identification candidates used by the project are `22 F150`,
`22 F187`, `22 F189`. They may return only part/software numbers. An explicit
family token can support experimental selection; a validated exact identifier
table is stronger evidence. Unknown/conflicting identity must not silently
select N20 or another convenient mapping. Exclude VIN queries from routine
profile discovery unless specifically needed. Label synthetic identity tests
as tests of selection logic, not proof of real ECU responses.

## Freshness and wire compatibility

Keep sample timestamps per channel, in a monotonic clock domain shared by
sender and receiver. UDP delivery is not a new measurement. Reject pre-open
queued frames and stale/invalid values; publish invalidity on worker exit.
Verify constants refresh correctly and stop/start clears old data.

If a wire slot changes meaning, version the protocol and reject incompatible
versions. The case project uses different nine-channel versions for ethanol
and oil temperature so an old worker cannot silently mislabel them.
