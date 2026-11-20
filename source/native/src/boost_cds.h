#ifndef BD_BOOST_CDS_H
#define BD_BOOST_CDS_H
#include "gauge_core.h"

/* CDS channels are canonical values, not diagnostic wire slots. */
inline int bd_cds_slot(unsigned signal) {
    switch (signal) {
    case BD_RPM: return 0;
    case BD_SPEED: return 1;
    case BD_COOLANT: return 2;
    case BD_OIL_TEMP: return 3;
    case BD_ACCELERATOR: return 5;
    case BD_AMBIENT: return 8;
    default: return -1;
    }
}
inline unsigned bd_diagnostic_mask(unsigned selected) {
    for (unsigned i=0;i<BD_COUNT;++i)
        if (bd_cds_slot(i)>=0 || i==BD_TRANS || i==BD_ETHANOL) selected &= ~(1u<<i);
    return selected;
}
/* Pressure keeps the validated bar/kPa convention; Evo exposes temperature
 * and speed units independently. Never infer one from the other. */
inline bool bd_evo_reading(unsigned signal, double raw, unsigned temperature,
                           unsigned speed, double& value, const char*& unit) {
    value=bd_display_value(signal,raw,1);
    unit=bd_unit(signal,1);
    if (signal==BD_IAT || signal==BD_TRANS || signal==BD_COOLANT ||
        signal==BD_OIL_TEMP || signal==BD_AMBIENT || signal==BD_CAT_TEMP) {
        if (temperature!=1 && temperature!=2) return false;
        value=temperature==2 ? raw*1.8+32 : raw;
        unit=temperature==2 ? "\260F" : "\260C";
    } else if (signal==BD_SPEED) {
        if (speed!=1 && speed!=2) return false;
        value=speed==2 ? raw/1.609344 : raw;
        unit=speed==2 ? "mph" : "km/h";
    }
    return true;
}
#endif
