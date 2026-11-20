# Change notes

- **Engine Fault Codes and MHD+ Suite** wait up to three seconds for another app to release diagnostics. MHD+ sends the requested map command once after acquiring the gateway.
- **HUD Gauges** polls after a 30 ms gap between reads and checks for redraws every 5 ms.
- **Desktop installers v0.7.4 beta** add exact framework matches for M17345I, R19101I, S19221I, and T23235J.

- **BoostDeck** has nine configurable gauge positions in the original layout and separate saved gasoline and diesel layouts. New choices include OBD oil temperature, control module voltage, RPM, load, air flow, and fuel rate.
- **LaunchDeck** adds 0–60 and 60–130 mph plus 0–100 and 100–200 km/h timing with three saved results per mode.
- **MHD+ Suite** shows the last map and Max Cooling settings acknowledged by the DME.
- **xDrive Control** incorporates the DSC_25/LMV 0F1480 profile with engine-running, Park, and zero-speed checks.
- **Diagnostic gateway** lookup supports a shared address and link-local HSFZ discovery. Bootmod3 and HUD Gauges report more specific compatibility failures.

- **Desktop installers v0.7.3 beta** add eleven firmware and framework profiles and manual firmware and display selection when automatic detection cannot read them.
- **BoostDeck** saves color and units across iDrive restarts. **ShiftDeck** saves units and shift-light RPM.

- **MOST fallback** covers Bootmod3 map switching, Exhaust Flap and Active Sound, plus the dedicated Exhaust Flap submenu in MHD+ Suite. Bootmod3 burble presets and xDrive still require HSFZ.
- **Desktop installers v0.7.1 beta** embed both rebuilt workers.

- **BoostDeck** has faster gauge updates, a new layout, an Active Ethanol gauge, and metric measurements under OPTIONS.
- **ShiftDeck** performance improvements.
- **MHD+ Suite** groups map switching, exhaust flap control, and Max Cooling. The new Max Cooling tiles show the last request accepted by the DME.
- **Bootmod3 Suite** adds a new control menu for compatible CustomROM tunes. Beta.
- **xDrive Control (experimental)** adds an xDelete-style menu for temporary rear-wheel drive on supported DSC variants.
- **Engine Fault Codes** reads engine codes and shows their descriptions when available. It can read codes even when the DME refuses to clear them.
- **HUD Gauges** adds a curved windshield tachometer and numeric boost in PSI, with a persistent on/off checkbox. The packaged build targets the reference 340i with its factory HUD.
- **Dashboard diagnostics** uses the MOST fallback from PR #9 in the packaged workers. A per-app gateway address can be set in gateway-ip.txt.
- **Exhaust Flap** moves under MHD+ Suite → Options, alongside Max Cooling.
- **Desktop installers** add a read-only compatibility check and recognize matching N18092I and P18362I framework captures.
- **xDrive Control (experimental)** distinguishes missing DSC replies, negative replies, and detected DSC indices outside the write allowlist.
