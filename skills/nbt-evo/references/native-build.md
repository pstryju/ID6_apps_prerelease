# Native QNX port and deployment

## Establish a target, not a firmware-letter guess

Record firmware, active screen dimensions and SHA-256 of the actual
`libAppFramework.A.so` and `libCoreFramework.A.so`. Layout resolution and C++
ABI compatibility are separate checks. Linking against copied libraries is
necessary evidence, not proof that reconstructed class layout is correct.

For a reconstructed SDK inspect constructors, object offsets, primary vtable
slot order, destructor forms, string/delegate layouts, alignment, ownership and
queued Parcel/Message behavior. Avoid adding virtual methods to a recovered
base class unless the target vtable establishes their position. Add proven
nonvirtual declarations without changing object layout where appropriate.

Compare imports against the target exports using a compatible `readelf`,
including dynamic symbols, and check system/DSI dependencies separately.
`--allow-shlib-undefined` does not establish that symbols exist at runtime.
Verify manifest names, connector IDs, model/action IDs, XML references and
image resources together. Preserve a reference build while adding a target
profile so unrelated users do not inherit an unverified port.

## Toolchain lessons

The originating port uses licensed QNX SDP 6.5 Linux tools and ARM GCC 4.4.2.
Confirm requirements for the current binary rather than selecting a modern
compiler based on architecture alone. Keep SDK licenses and BMW libraries out
of public source and distributable artifacts.

Older 32-bit compiler host tools can fail with inode overflow on a Docker
Desktop bind mount. A demonstrated workaround is to copy source and required
inputs to the container's native temporary filesystem, compile there, then copy
only outputs back. Mount vehicle libraries/license inputs read-only. Use offline
builds once dependencies are available.

Host simulation proves protocol and lifecycle properties under the simulator.
Cross-compilation and import checks prove a different, limited set of properties.
Neither establishes target UI behavior, physical ECU scaling or car wake behavior.

## Fast, reversible deployment

Repeated SSH startup can dominate installation time. Batch an inventory read,
then transfer one complete bundle and execute a transaction on the HU. Prefer
an existing tested binary-safe bundle/helper over assuming `tar` is installed.

Discover QNX shell capabilities. On one Evo `printf` failed with "cannot
execute"; do not assume GNU utilities or Linux shell builtins. Use a tested
portable subset or the project's verified native helper.

Before commit of the remote transaction:

- Verify target fingerprints, geometry, conflicting connector/name entries,
  complete staged files, checksums and executable modes.
- Keep backups outside Plugins; do not leave a second discoverable app copy.
- Use an exclusive deployment transaction, bounded validated paths and a durable
  deployment ID/receipt. Include dynamically generated configuration in hashes.
- Provide progress for connection, transfer, verification, commit and rollback;
  stream output without corrupting binary uploads or exposing credentials.

During failure, verify rollback bytes rather than assuming a trap succeeded.
If SSH disconnects after commit begins, inspect transaction state and final
hashes before retrying; a disconnect does not prove nothing changed. Leave the
recovery location available. Restore deployments in reverse order where they
replace each other's app versions.

Separate installation from HU restart. Do not automatically restart unless
that action is included in the user's authorization. Prefer a minimal parked
target loading check, then verify actual features separately. Retire temporary
smoke apps after their purpose is fulfilled; preserve receipts needed to remove
older installations, not obsolete test applications in every new bundle.
