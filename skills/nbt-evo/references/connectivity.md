# Connectivity and node discovery

## Distinguish the failing layer

Capture the exact executable, destination, username, options and last verbose
SSH message. A GUI SCP client and Windows OpenSSH may negotiate differently.
Reuse a known working project deployment script's transport implementation
when practical, instead of assuming only its destination matters.

| Observation | Next useful distinction |
| --- | --- |
| TCP connect times out | Address, route, selected NIC, reachability or listener; cryptographic flags cannot fix a failed TCP connect |
| Key exchange completes | TCP and negotiated encryption already work; inspect the following authentication/session phase |
| Stops at password authentication | Check prompt delivery, client askpass/TTY behavior and selected auth method; do not diagnose this as failed key exchange |
| Authenticated but no interactive shell | Compare a bounded explicit command and a shell/PTY request |
| SCP works, interactive SSH fails | Compare actual clients, options, authentication and session type |

For a unit that specifically needs them, these per-invocation options worked
in the case study:

```text
ssh -o HostKeyAlgorithms=+ssh-rsa -o MACs=+hmac-sha1 root@CONFIRMED_OMAP_IP
```

Add other legacy algorithms only when negotiation evidence requires them.
Keep host-key verification. Use password-first authentication or a secure
askpass channel if the actual client needs it; do not embed passwords in
tracked scripts, process arguments or logs. Preserve a working local credential
mechanism and binary-safe stdin/stdout transport.

## Jacinto and OMAP

Run `uname -a` and verify paths. On split-node Evo, Jacinto can accept SSH while
OMAP owns HMI. Missing `/opt/hmi/ID5/lib` on Jacinto does not prove the libraries
are absent on OMAP. Candidate read-only Qnet paths observed in the project:

```text
/net/hu-omap/fs/sda0/repository/istep/opt/hmi/ID5/lib
/net/hu-omap/fs/sda0/opt/hmi/ID5/lib
```

Discover which exists. OMAP commands can be run from Jacinto with QNX `on`,
for example:

```sh
on -f hu-omap /sbin/ifconfig
on -f hu-omap sh -c 'export LD_LIBRARY_PATH=/opt/sys/lib:/usr/lib:/lib; /opt/sys/bin/LayerManagerControl get screen 0'
```

Record the HMI node's address independently of the SSH entry point. Discover
the writable OMAP `/var` mapping independently of the read-only `/opt` mapping;
do not infer it by replacing a path segment. In the case project the plugin
directory on OMAP is `/var/opt/hmi/data/ID5/Plugins`.

## Minimal compatibility capture

Collect exact firmware, screen geometry, node identity, framework hashes and
only the necessary libraries. Firmware candidate paths include OMAP
`/opt/sys/etc/nbt_version.txt` and Jacinto `/etc/nbt_version.txt`; verify which
belongs to the intended unit. A copied `opt/` and `var/` is not a full runtime
filesystem. Missing system libraries in such a copy are incomplete evidence,
not proof of missing libraries on the car.

Use binary-safe transfer and verify ELF identity/length/hash. Do not pipe an
ELF through text decoding, newline conversion or a text-only SSH wrapper.

## ENET after sleep

An external diagnostic gateway can work during a laptop session and disappear
after vehicle sleep. Cable insertion alone is not proven activation. Treat
activation, physical link and routing as hypotheses until distinguished.
Neither a HU restart nor more frequent polling guarantees recovery.

Test intended autonomous operation after actual sleep/wake without OBD attached.
Do not introduce keep-alives, routing changes, daemon restarts or coding as
unrequested experiments. If the app needs no laptop, prefer verified CDS/internal
MOST over dependence on an externally activated diagnostic Ethernet path.
