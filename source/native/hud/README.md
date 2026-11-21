# HUD Gauges source

See [Build HUD Gauges](../../docs/BUILD.md#build-hud-gauges) for the QNX build. The menu and read-only vehicle sampler use the repository license. `hud_shape.cpp` uses the NBT-AAD HUDTerminal and MOST client under the PolyForm Noncommercial 1.0.0 license.

Required notice. Copyright (c) 2026 nanaisu – NBT-AAD. [License and upstream source](https://github.com/Sq00ky/Supra-iDrive6-Android-Auto/blob/f97f1a34bdf68f1861862bd6361a2918949a48f2/LICENSE.md)

## Evo Y comparison packages

`tools/build-hud-upstream.ps1` prepares two private install candidates:

- `Upstream`: the original four app files from `packages/hud-gauges.zip`,
  unchanged. RPM and boost require HSFZ.
- `UpstreamCds`: builds the same `hud_shape.cpp` without changes, replacing
  `parked_check.c` with `cds_reader.cpp`. `HUD_CDS_SOURCE` in `hud_menu.cpp`
  supplies fresh CDS RPM over loopback UDP. No diagnostic transport is linked
  into this worker. Boost is unavailable and the original field displays `--`.
  RPM alone never satisfies `--check-parked` or `--draw-parked`.

Arc geometry, drawing, handles, navigation ownership and clearing remain the
upstream implementation. This is a comparison candidate, not a demonstrated
repair of the owner's navigation interaction.

The build exports licensed NBT-AAD source from local Git revision
`7aec566e63182043371a484f04487f396197788d`, whose Config still exposes
`checkNav`, `navTexts` and `navBuffers`. It neither changes the neighboring
checkout nor downloads dependencies. Build hashes/revision stay in ignored
`native/out/dashboard/hud-variants/build-evidence.json`.

Run `tools/check-hud-upstream.ps1` before installing. It verifies byte identity
of the original variant, candidate checksums, ARM ELF types and BMW imports
against the local Evo libraries. No runtime compatibility is implied.