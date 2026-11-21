#!/bin/sh
# Offline: preserve upstream hud_shape.cpp; replace only its data reader.
set -eu
cd "$(dirname "$0")"
: "${NBTEVO_FRAMEWORKS_DIR:?Missing local frameworks}"
: "${NBTAAD_SOURCE:?Missing licensed NBT-AAD source}"
export QNX_HOST="${QNX_HOST:-/opt/qnx650/host/linux/x86}"
export QNX_TARGET="${QNX_TARGET:-/opt/qnx650/target/qnx6}"
export QNX_CONFIGURATION="${QNX_CONFIGURATION:-/etc/qnx}"
export PATH="$QNX_HOST/usr/bin:$PATH"
out=out/hud-upstream-cds
mkdir -p "$out"
c() { qcc -V4.4.2,gcc_ntoarmv7le -Wc,-std=c99 -Wall -Wextra -Werror "$@"; }
cxx() { qcc -V4.4.2,gcc_ntoarmv7le_gpp -lang-c++ -Wc,-std=gnu++98 -Wall -Wextra -Werror "$@"; }
c -DPERF_LAB -DBD_STOCK_DME -Isrc -O2 -fPIC -c src/gauge_core.c -o "$out/core.o"
qcc -V4.4.2,gcc_ntoarmv7le_gpp -lang-c++ -Wc,-std=gnu++0x -O2 -D_QNX_SOURCE \
    -Isrc -Ihud -I"$NBTAAD_SOURCE" hud/hud_shape.cpp hud/cds_reader.cpp \
    "$NBTAAD_SOURCE/cluster/hud_terminal.cpp" "$NBTAAD_SOURCE/most/client.cpp" "$NBTAAD_SOURCE/dsi_client.cpp" \
    "$out/core.o" -lsocket -lm -o "$out/hud-shape-qnx"
cxx -DHUD_CDS_SOURCE -DPERF_LAB -DBD_STOCK_DME -Isrc -Isdk -O2 -fno-exceptions -fPIC -shared hud/hud_menu.cpp "$out/core.o" \
    "$NBTEVO_FRAMEWORKS_DIR/libAppFramework.A.so" "$NBTEVO_FRAMEWORKS_DIR/libCoreFramework.A.so" -lsocket \
    -Wl,--allow-shlib-undefined -Wl,-soname,libHudTest.so -o "$out/libHudTest.so"
