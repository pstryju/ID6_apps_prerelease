#ifndef BD_BOOST_OBD_H
#define BD_BOOST_OBD_H
/* Standard Mode 01 candidates; acceptance by the DME is still required.
 * CDS-owned channels are intentionally absent from the MOST polling list. */
static const struct { unsigned pid,slot,diesel_only; } bd_boost_obd[]={
    {0x33,BD_BARO,0}, {0x42,BD_BATTERY,0}, {0x04,BD_LOAD,0},
    {0x0b,BD_MAP,0}, {0x10,BD_MAF,0}, {0x11,BD_THROTTLE,0},
    {0x5e,BD_FUEL_RATE,0}, {0x2f,BD_FUEL_LEVEL,0},
    {0x0f,BD_IAT,1}, {0x23,BD_RAIL,1}, {0x0b,BD_BOOST,1},
    {0x0e,BD_SPARK_ADVANCE,0}, {0x43,BD_ABS_LOAD,0},
    {0x06,BD_STFT_B1,0}, {0x07,BD_LTFT_B1,0},
    {0x08,BD_STFT_B2,0}, {0x09,BD_LTFT_B2,0},
    {0x44,BD_LAMBDA_TARGET,0}, {0x3c,BD_CAT_TEMP,0}, {0x4c,BD_THROTTLE_TARGET,0}
};
#define BD_BOOST_OBD_COUNT (sizeof bd_boost_obd/sizeof bd_boost_obd[0])
#endif
