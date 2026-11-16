#!/bin/sh
# Local candidate build only; does not install or connect to the vehicle.
set -eu
cd "$(dirname "$0")"
: "${NBTEVO_FRAMEWORKS_DIR:?Set NBTEVO_FRAMEWORKS_DIR to the target framework directory}"
export QNX_HOST="${QNX_HOST:-/opt/qnx650/host/linux/x86}"
export QNX_TARGET="${QNX_TARGET:-/opt/qnx650/target/qnx6}"
export QNX_CONFIGURATION="${QNX_CONFIGURATION:-/etc/qnx}"
export PATH="$QNX_HOST/usr/bin:$PATH"
out="${NBTEVO_OUTPUT_DIR:-out/dashboard}"
mkdir -p "$out"
frameworks="$NBTEVO_FRAMEWORKS_DIR"
for lib in libAppFramework.A.so libCoreFramework.A.so; do
    test -f "$frameworks/$lib" || { echo "Missing $lib" >&2; exit 1; }
done
c() { qcc -V4.4.2,gcc_ntoarmv7le -Wc,-std=c99 -Wall -Wextra -Werror "$@"; }
cxx() { qcc -V4.4.2,gcc_ntoarmv7le_gpp -lang-c++ -Wc,-std=gnu++98 -Wall -Wextra -Werror "$@"; }
c -DPERF_LAB -DBD_STOCK_DME -Isrc -O2 -fPIC -c src/gauge_core.c -o "$out/core.o"
for item in BoostDeck:plugin ShiftDeck:shift_deck EngineCodes:engine_codes; do
    app=${item%:*}; src=${item#*:}
    extra=""; if [ "$app" = BoostDeck ]; then extra="-DBD_BOOST_MOST"; fi
    cxx -DPERF_LAB -DBD_STOCK_DME -DNATIVE_SHIFT_SMOOTH -DBD_NATIVE_CDS $extra -Isrc -Isdk -O2 \
        -fno-strict-aliasing -fno-exceptions -fPIC -shared "src/$src.cpp" "$out/core.o" \
        "$frameworks/libAppFramework.A.so" "$frameworks/libCoreFramework.A.so" -lm -lsocket \
        -Wl,--allow-shlib-undefined "-Wl,-soname,lib$app.so" -o "$out/lib$app.so"
done
most="src/most/dsi_bind.c src/most/dsi_client.c src/most/most_link.c src/most/most_proto.c"
c -DNATIVE_FAST -DPERF_LAB -DBD_STOCK_DME -Isrc -O2 src/collector.c src/diag_link.c $most "$out/core.o" -lm -lsocket -o "$out/boostdeck-collector-qnx"
c -DNATIVE_FAST -DPERF_LAB -DSHIFT_DECK_ONLY -Isrc -O2 src/collector.c src/diag_link.c $most "$out/core.o" -lm -lsocket -o "$out/shift-deck-qnx"
c -DENGINE_CODES_ONLY -DPERF_LAB -Isrc -O2 src/collector.c src/diag_link.c $most "$out/core.o" -lm -lsocket -o "$out/engine-codes-qnx"
c -O2 -static fileops.c -o "$out/gaugedeck-fileops"
c -DDIAGNOSTIC_PROBE_ONLY -Isrc -O2 src/collector.c src/diag_link.c $most "$out/core.o" -lm -lsocket -o "$out/diagnostic-probe-qnx"
c -DPERF_LAB -DBD_STOCK_DME -Isrc -O2 src/boost_most.c $most "$out/core.o" -lm -lsocket -o "$out/boostdeck-most-qnx"
c -DPERF_LAB -DBD_STOCK_DME -DBD_HUD_SOURCE -Isrc -O2 src/boost_most.c $most "$out/core.o" -lm -lsocket -o "$out/hud-boost-most-qnx"
c -Isrc -O2 src/most_probe.c $most "$out/core.o" -lm -lsocket -o "$out/most-probe-qnx"
c -DXDRIVE_ONLY -Isrc -O2 src/collector.c src/diag_link.c $most "$out/core.o" -lm -lsocket -o "$out/xdrive-control-qnx"
cxx -DXDRIVE_ONLY -Isrc -Isdk -O2 -fno-exceptions -fPIC -shared src/mhd_maps.cpp \
    "$frameworks/libAppFramework.A.so" "$frameworks/libCoreFramework.A.so" -lsocket \
    -Wl,--allow-shlib-undefined -Wl,-soname,libXDriveControl.so -o "$out/libXDriveControl.so"
cxx -DPERF_LAB -DBD_STOCK_DME -Isrc -Isdk -O2 -fno-exceptions -fPIC -shared src/hud_app.cpp "$out/core.o" \
    "$frameworks/libAppFramework.A.so" "$frameworks/libCoreFramework.A.so" -lsocket \
    -Wl,--allow-shlib-undefined -Wl,-soname,libHudTest.so -o "$out/libHudTest.so"
c -Isrc -O2 -c src/most/dsi_bind.c -o "$out/hud-dsi-bind.o"
c -Isrc -O2 -c src/most/dsi_client.c -o "$out/hud-dsi-client.o"
cxx -DPERF_LAB -DBD_STOCK_DME -Isrc -Ivendor/hud -O2 -fexceptions src/hud_display.cpp vendor/hud/hud_core.cpp \
    "$out/hud-dsi-bind.o" "$out/hud-dsi-client.o" "$out/core.o" -lsocket -lm -o "$out/hud-display-qnx"
