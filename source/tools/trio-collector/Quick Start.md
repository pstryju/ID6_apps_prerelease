# NBT Evo Trio Compatibility Collector

Use this tool when the [compatibility matrix](../../docs/COMPATIBILITY.md) does not list your firmware or when BoostDeck, ShiftDeck, or MHD+ Suite needs a car-specific build.

1. Park the car, wake iDrive, and connect the computer by wired ENET. Enable temporary SSH with [Bavarian SSH Client](https://ssh.bavarian-aa.uk/) first.
2. Extract the macOS or Windows download and open NBT Evo Trio Collector. It opens a local page in your browser. If building from this folder, run `go run .` with the Go version in `go.mod`.
3. Select **Find car**. If the head unit address does not populate, enter the **head unit SSH IP** from Bavarian SSH Client. Use the separately displayed **gateway IP** for app diagnostics.
4. Select **Collect and review**. Check the fingerprint against your known SSH connection, enter the temporary root password, and select **Trust this fingerprint and collect**. Enter the exact `NBTevo_…` code shown in iDrive if automatic firmware detection needs help.
5. Select **Download ZIP** and email the resulting `Trio-compatibility-*.zip` to [nordicbmwdealer@gmail.com](mailto:nordicbmwdealer@gmail.com) so a fix can be prepared quickly. Include your car model and which app fails.

The ZIP contains `compatibility.json` with firmware, QNX node and version, display dimensions and HMI build when available, and framework hashes, sizes, and exported symbols. The collector reads fixed paths, including on split Jacinto/OMAP units. Credentials, VINs, and framework binaries are excluded from the emailed report.

## Download or build

- [macOS, Apple silicon and Intel](../../../packages/NBT-Evo-Trio-Collector-macOS.zip)
- [Windows x64](../../../packages/NBT-Evo-Trio-Collector-Windows-x64.zip)
- [Download checksums](../../../packages/Trio-SHA256SUMS.txt)

Use `./build-release.sh DESTINATION` on macOS to build both downloads with Go and the Apple command line tools. The macOS app is ad-hoc signed and the Windows executable is unsigned.
