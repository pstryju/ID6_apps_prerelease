# QNX architecture and node context

## Nodes and execution

Known unit: OMAP `169.254.199.119` (`hu-omap`), Jacinto `169.254.199.99`
(`hu-jacinto`). OMAP runs HMI/frameworks/plugins. Jacinto is a separate QNX
node. SSH on one does not establish paths or service availability on the other.

Useful read-only discovery commands, subject to availability:

```sh
uname -a
pidin ar
mount
ls /net
```

Use actual QNX tool options; do not assume Linux `/proc`, systemd, GNU utilities
or shell builtins. On the studied unit even `printf` failed as unavailable in
a remote script. The offline copy cannot establish the complete live OS layout.

## Qnet

Qnet exposes remote nodes/resources under namespaces such as `/net/hu-omap`.
Framework path candidates observed from Jacinto:

```text
/net/hu-omap/fs/sda0/repository/istep/opt/hmi/ID5/lib
/net/hu-omap/fs/sda0/opt/hmi/ID5/lib
```

Discover actual mounts. Do not infer writable `/var` paths by substituting an
assumed partition. The flattened local `opt/var` copy is not a mount table.

QNX `on` can execute on another node:

```sh
on -f hu-omap /sbin/ifconfig
on -f hu-omap sh -c 'export LD_LIBRARY_PATH=/opt/sys/lib:/usr/lib:/lib; /opt/sys/bin/LayerManagerControl get screen 0'
```

Reading files over Qnet and running processes remotely are different operations.
Track node, namespace, working directory and library search path. A binary
obtained from OMAP need not execute correctly in Jacinto's own environment.

Firmware-file candidates: OMAP `/opt/sys/etc/nbt_version.txt`, Jacinto
`/etc/nbt_version.txt`. LayerManager reports active screen geometry; asset
dimensions do not establish it.

## Internal layers

| Layer | Role |
| --- | --- |
| QNX | Processes, shared objects, IPC, device/resource-manager namespaces, remote-node facilities |
| HMI frameworks | Plugin lifecycle, dispatching, parcels/messages, model/action binding, framework services |
| HMI remoting / CDS | Named properties, asynchronous requests and data callbacks |
| DSI | Internal client/service interfaces; registry/targets, notifications, requests and stream I/O |
| MOST | Addressed FBlocks, instances and functions on the head-unit/vehicle bus |
| HSFZ | Vehicle diagnostics over Ethernet, separate from SSH and Qnet |
| Coding/persistence | Variant inputs and stored state, distinct from live data and RPC interfaces |

Observed `/opt/sys/lib` files include `libsys_dsi_base.cpp-ne.so.4`,
`libsys_dsi_base.gpp.so.4`, `libsys_dsi_colibry.cpp-ne.so.8`, service-broker and
trace libraries. Similar filenames with different suffixes are not interchangeable:
inspect compiler ABI, dependencies and exports. MOST-related candidate:
`libimp_sys_most_base_nbt.cpp-ne.so.17`.

DSI work exercised `DSIInit`, `DSIClientInit`, `DSIClientRun`, registration,
notification/request and stream methods. These observations concern the actual
studied library, not a stable public SDK guaranteed across firmware. Recover
callback layout/framing before calling an internal entry point.

For CDS distinguish a name in remoting-library strings, an exported framework
method and a successful runtime subscription. Likewise, a MOST library name
does not prove a particular service/target is currently available. Trace the
implementing process/service and its dependencies when investigating behavior.

## Network boundaries

Link-local IPs identify observed interfaces, not a universal Evo address map.
An external vehicle diagnostic endpoint is not the HU SSH endpoint. Diagnostic
Ethernet routing/activation can change after sleep while HMI and internal data
services continue. A TCP timeout alone does not identify the failed internal layer.

Working SSH access detail for this unit:

```text
ssh -o HostKeyAlgorithms=+ssh-rsa -o MACs=+hmac-sha1 root@169.254.199.119
```

Keep host-key verification and legacy options scoped to the connection. Prefer
offline analysis whenever existing files answer the question.
