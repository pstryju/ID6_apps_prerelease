/* service, identifier, bytes, signed, sentinel, scale, offset, min, max.
 * Original B58 candidate definitions: docs/SIGNALS.md. Do not treat as a
 * verified universal BMW map. FFFF timing remains deliberately unavailable. */
{0x22,0x4201,2,0,   -1, 1./256,      0,      40,115},
{0x22,0x4205,2,0,   -1, 1./128,      0,      20,450},
{0x22,0x5889,2,0,   -1, 16./65535,   0,      .5,2},
{0x22,0x4a49,2,1,65535, .1,          0,     -64,64},
{0x22,0x580f,1,0,  255, .75,       -48,     -48,143.25},
{0x22,0xda12,2,0,   -1, 1,         -48,     -48,180},
{0x22,0x56d7,2,0,65535, 1./50,       0,       0,500},
{0x22,0x586f,2,0,65535, .001,        0,       0,20},
{0x22,0x4300,2,0,65535, .1,       -273.15,  -48,150},
