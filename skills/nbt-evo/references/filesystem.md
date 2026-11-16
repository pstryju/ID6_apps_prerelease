# Filesystem and local evidence

## Local roots

| Location | Role |
| --- | --- |
| `C:\Users\pawel\evo` | Private copy of the unit's `opt/` and `var/` trees |
| `C:\Users\pawel\Developer\ID6_apps` | Existing analysis scripts, native source and platform notes |
| `C:\Users\pawel\Developer\qnx650-docker` | Local QNX toolchain environment and analysis tools |

The copied firmware reports `NBTevo_Y24073J`. Use actual binary hashes when
comparing firmware: its software letter alone does not establish ABI identity.

## Copied tree

Observed `opt/` directories: `car`, `conn`, `core`, `hmi`, `mm`, `nav`, `share`,
`speech`, `sys`, `tuner`. These narrow a subsystem search, but do not by
themselves establish which processor runs each binary. `var/opt/` contains
writable subsystem data including `car`, `conn`, `hmi`, `mm`, `nav`, `speech`,
`sys`. Directory names are not a process topology.

| Path on HU / beneath the copy | Investigation use |
| --- | --- |
| `/opt/hmi/ID5/bin` | HMI executables |
| `/opt/hmi/ID5/lib` | HMI frameworks and shared libraries |
| `…/libAppFramework.A.so`, `…/libCoreFramework.A.so` | Native framework exports, ABI and implementation |
| `/opt/hmi/ID5/lib/libhmicoding.so` | HMI coding/variant implementation candidate |
| `/opt/hmi/ID5/data/ro/hmiplugins` | Factory HMI plugin binaries |
| `…/hmiplugins/libhmiremoting.so` | Remoting/CDS property names and implementations |
| `/opt/hmi/ID5/data/ro/Plugins`, `PluginsCoded` | Factory plugin resource/configuration trees; inspect contents |
| `/opt/hmi/ID5/data/ro/common`, `bmw`, `bmwm`, `rr` | Resource/layout/asset trees; not proof of the active variant |
| `/var/opt/hmi/data/ID5` | Writable HMI data |
| `/var/opt/hmi/data/ID5/Plugins` | Custom plugin location used on OMAP; verify live before writing |
| `/opt/sys/bin` | System executables, including LayerManager and syset tools |
| `/opt/sys/lib` | Internal system and DSI libraries |
| `/opt/sys/etc` | System configuration and firmware-file candidate |
| `/var/opt/sys/persistence` | Persistent system data, not a live service API |
| `/opt/car/{bin,lib,etc,data}` | Vehicle-related subsystem binaries/configuration/data |
| `/opt/core/{bin,etc}` | Core subsystem executables/configuration |

The `…` shorthand above refers to the preceding full directory; brace notation
groups paths and is not a guarantee that the target shell supports expansion.
`ID5` stays in filesystem paths on iDrive 6: do not rename it to `ID6`.
Multiple brands' resources can coexist without being selected at runtime.

## Investigation workflow

Locate files with `rg --files` and bounded filename filters. Extract printable
strings and dynamic symbols from likely components, then follow referenced
service names, libraries, configuration files and adjacent resources. Avoid
dumping whole binaries as text. Search ASCII/UTF-16 as appropriate.

Mangled C++ names contain number/name fragments that can resemble domain
terms. Distinguish standalone strings from symbol fragments. A found name is
a lead, not an established API; names can also be generated or indirect.

Persistence files may be binary, versioned or stale. Establish encoding/schema
and provenance before treating their content as current state. Do not write
directly to persistence as a substitute for discovering the owning service.

Existing helpers in the project: `source/tools/audit-cds.ps1` inventories
property-like strings/classes from `libhmiremoting.so`; `capture_headunit.py`
collects firmware, screen and framework evidence with split-node handling.
Read current parameters before reuse. The `opt/var` copy lacks live mounts,
device state, process state and parts of the root runtime filesystem.
