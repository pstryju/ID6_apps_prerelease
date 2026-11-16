/* Stock owner F30 N20 candidate; evidence and remaining calibration limits:
 * docs/BOOSTDECK-MOST.md, tests/fixtures/n20-most-readings.json.
 * {signal definition}, output slot (BD_COUNT = ambient), period ms,
 * output multiplier (pressure difference kPa -> psi; lambda -> gasoline AFR).
 * Temperatures/RPM come from CDS; EGS and rejected 2256D7 are not queried. */
{{0x22,0x4201,2,0,   -1,1./256,    0, 40,115},    BD_COUNT, 800,1},
{{0x22,0x4205,2,0,   -1,1./128,    0, 20,450},    BD_BOOST,  80,.145037738},
{{0x22,0x5889,2,0,   -1,16./65535, 0, .5,2},      BD_AFR,   300,14.7},
{{0x22,0x4a49,2,1,65535,.1,        0,-64,64},     BD_TIMING,300,1},
{{0x22,0x580f,1,0,  255,.75,     -48,-48,143.25}, BD_IAT,  2000,1},
{{0x22,0x586f,2,0,65535,.001,      0,  0,20},     BD_OIL,   500,1},
{{0x01,0x23,  2,0,   -1,.1,        0,  0,6553.5}, BD_RAIL,  500,1},
