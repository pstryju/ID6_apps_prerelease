# Native binary analysis and build environment

## Tools and binary identity

Check ELF architecture, class, endianness, dependencies and executable/shared
object type first. The application environment examined here is QNX 6.5 ARM
little-endian; inspect individual components instead of assuming all processors
have identical runtime environments.

Existing Windows tools:

```text
C:\Users\pawel\Developer\qnx650-docker\.local-qnx\research\ntoarmv7-readelf.exe
C:\Users\pawel\Developer\qnx650-docker\.local-qnx\research\ntoarmv7-objdump.exe
```

Useful readelf options: `-h` for ELF identity, `-d` for dynamic dependencies,
`-s -D -W` for dynamic symbols. Use bounded disassembly with
`objdump -d --start-address=... --stop-address=...` and actual numeric addresses.
These older tools may not support newer symbol-specific convenience options.

## Framework analysis

Analyze the actual `libAppFramework.A.so` and `libCoreFramework.A.so` from
`C:\Users\pawel\evo\opt\hmi\ID5\lib`. Record SHA-256 and preserve original bytes.
Constructor stores, allocation sizes and vtable order establish stronger layout
evidence than a reconstructed header from another firmware.

For C++ ABI inspect alignment, inheritance, virtual slots, destructor variants,
string/delegate layout, ownership and message/parcel handling. Mangled exports
do not completely define object layout or lifecycle. Adding proven nonvirtual
declarations can preserve layout; inventing virtual slots can corrupt the host.

The studied framework exports native CDS add/get/remove methods. Verify their
actual signatures/behavior for a target rather than treating names as a public
SDK. Compare imports to framework exports and check system dependencies
separately. Linker success, especially with undefined-symbol allowances, is
not runtime compatibility. Keep firmware-specific offsets/sizes in fingerprinted
analysis notes, not as platform-wide constants.

## Available QNX environment

Toolchain project: `C:\Users\pawel\Developer\qnx650-docker`. Existing native
builds use licensed QNX SDP 6.5 Linux tools, ARM GCC 4.4.2 and a local image
`qnx650-sdk`. Verify the image and Docker daemon before use; not every offline
analysis needs Docker. Relevant variables: `QNX_HOST`, `QNX_TARGET`,
`QNX_CONFIGURATION` and host-toolchain `PATH`. Existing scripts establish the
known `qcc` target selection and framework inputs.

Old compiler host tools can be 32-bit. Docker Desktop bind-mount inode numbers
can cause `EOVERFLOW`; a demonstrated workaround is copying inputs/source into
the container's native temporary filesystem, compiling there and copying outputs
back. Mount evidence and licenses read-only; do not redistribute SDK licenses
or proprietary target libraries.

The copied `opt/var` tree is not a complete sysroot. Missing libc/libm/libsocket/
libstdc++ inputs may require SDK libraries or a matching authorized private
runtime capture. Absence of `/lib` in the copy does not mean the HU lacks it.
Do not silently mix incompatible target and SDK runtime versions.

Host tests, QNX builds, symbol checks and runtime loading establish different
things. Report them separately. If live loading becomes necessary, prepare a
minimal reversible artifact rather than installing an unrelated feature suite.
