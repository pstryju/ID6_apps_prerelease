---
name: nbt-evo
description: Analyze BMW NBT Evo (iDrive 5/6) architecture, QNX nodes, filesystem copies, native libraries and internal services. Use for navigating Evo dumps, distinguishing OMAP from Jacinto, reverse-engineering interfaces or preparing target-compatible native code.
---

# NBT Evo platform

Use this skill as a map of the head-unit platform and local evidence. It is
independent of vehicle chassis, engine and applications being developed.

## Architecture and available evidence

NBT Evo is a distributed QNX system. Distinguish the OMAP HMI node from Jacinto,
a node's own filesystem from remote Qnet mounts, and HU access from vehicle
diagnostic endpoints. Establish where a command executes and what a path addresses.

| Node | Observed address | Role |
| --- | --- | --- |
| OMAP / hu-omap | 169.254.199.119 | HMI, application frameworks and plugins |
| Jacinto / hu-jacinto | 169.254.199.99 | Separate QNX node; alternative access to OMAP via Qnet |

These are known addresses for the available unit, not universal Evo defaults.
Verify node identity with `uname -a` when connected.

The private filesystem copy is **C:\Users\pawel\evo**, containing **opt/ and var/**.
Start there for platform questions; do not ask for files already available.
This is not a complete root filesystem, disk image or running-system snapshot.
Missing /lib, /proc, /net or boot images do not prove absence on the head unit.

Read the relevant reference:

- [Filesystem and evidence](references/filesystem.md): local paths, subsystem
  directories, resources, persistence and investigation of the copied files.
- [Architecture](references/architecture.md): QNX nodes, Qnet, execution context,
  HMI frameworks, DSI, MOST and CDS.
- [Native analysis](references/native-analysis.md): ELF/ABI analysis, available
  tools, QNX toolchain and target dependencies.

## Working method

Identify the relevant subsystem and inspect actual files before proposing APIs
or commands. Use filename/string searches for candidates, dynamic symbols for
available entry points and disassembly for implementation details. Label results
as observed files/exports, code-derived inference or runtime observation. A string
match is not proof of a usable API; an absent string does not prove absence.

Keep the copied Evo tree as evidence. Write inventories, extracted data and
patched copies into the current workspace, preferably private ignored directories.
Do not publish proprietary binaries, credentials or personal persistence data.
Check current binaries before relying on historical notes from another version.

Offline analysis needs no live vehicle. If runtime access becomes necessary,
establish the node and authorized scope first. Preparing code does not itself
authorize installation, restart, coding or service changes; preserve authorization
already given without inventing additional approval steps.
