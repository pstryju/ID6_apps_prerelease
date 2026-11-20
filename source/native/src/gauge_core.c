/* Evo Y uses its validated nine-channel protocol and CDS layout. */
#if defined(PERF_LAB) && defined(BD_STOCK_DME) && !defined(BD_CONFIGURABLE)
#include "evo_y_gauge_core.inc"
#else
#define _POSIX_C_SOURCE 200809L
#include "gauge_core.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

uint64_t bd_mono_ms(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) return 0;
    return (uint64_t)t.tv_sec * 1000 + (uint64_t)t.tv_nsec / 1000000;
}

#include "signal_decode.h"
static const bd_signal did_signals[]={
#include "profiles/reference.inc"
};
static int decode_signal(const bd_signal *signals,size_t count,unsigned id,
                         const unsigned char *data,size_t len,double *out){
    size_t i;for(i=0;i<count;++i)if(signals[i].identifier==id)
        return bd_signal_decode(&signals[i],data,len,out);
    return 0;
}
int bd_decode_did(unsigned did,const unsigned char *data,size_t len,double *out){
    return decode_signal(did_signals,sizeof did_signals/sizeof did_signals[0],did,data,len,out);
}


/* Mode 01 data bytes only, after the 41 <pid> prefix. SAE J1979 formulas.
   Read at idle on an F22 M240i (B58 Gen 1, NBTevo_S19322I): 5C 94 -> 108 C,
   42 39 F8 -> 14.840 V, both agreeing with the car. */
int bd_decode_obd(unsigned pid, const unsigned char *p, size_t n, double *out) {
    double x, lo, hi;
    if (!p || !out) return 0;
    switch (pid) {
    case 0x04: case 0x11: case 0x2f: case 0x49: case 0x4c:
        if (n != 1) return 0;
        x = p[0] * (100.0 / 255); lo = 0; hi = 100; break;
    case 0x06: case 0x07: case 0x08: case 0x09:
        if (n != 1) return 0;
        x = ((int)p[0] - 128) * (100.0 / 128); lo = -100; hi = 100; break;
    case 0x0e:
        if (n != 1) return 0;
        x = p[0] / 2.0 - 64; lo = -64; hi = 63.5; break;
    case 0x3c: case 0x43: case 0x44:
        if (n != 2) return 0;
        x = ((unsigned)p[0] << 8) | p[1];
        if (pid == 0x3c) { x = x / 10 - 40; lo = -40; hi = 6513.5; }
        else if (pid == 0x43) { x *= 100.0 / 255; lo = 0; hi = 25700; }
        else { x /= 32768; lo = 0; hi = 2; }
        break;
    case 0x05: case 0x0f: case 0x46:
        if (n != 1) return 0;
        x = (double)p[0] - 40; lo = -40; hi = 215; break;
    case 0x0b: case 0x0d: case 0x33:
        if (n != 1) return 0;
        x = p[0]; lo = 0; hi = 255; break;
    case 0x0c: case 0x10: case 0x23: case 0x5e:
        if (n != 2) return 0;
        x = ((unsigned)p[0] << 8) | p[1];
        x *= pid == 0x0c ? .25 : pid == 0x10 ? .01 : pid == 0x23 ? .1 : .05;
        lo = 0; hi = 65535; break;
    case 0x5c: /* engine oil temperature, A - 40 C; the top of the raw range is implausible */
        if (n != 1) return 0;
        x = (double)p[0] - 40; lo = -40; hi = 170; break;
    case 0x42: /* control module voltage, (256 A + B) / 1000 V */
        if (n != 2) return 0;
        x = (((unsigned)p[0] << 8) | p[1]) / 1000.0; lo = 6; hi = 18; break;
    default: return 0;
    }
    if (!isfinite(x) || x < lo || x > hi) return 0;
    *out = x;
    return 1;
}

int bd_encode(const bd_frame *f, char *out, size_t len) {
    int n;
    size_t used;
    unsigned i;
    if (!f || !out || !len) return -1;
    #ifdef PERF_LAB
    const char *version = "NBG7";
#else
    const char *version = "NBG3";
#endif
    n = snprintf(out, len, "%s %lu %u %u %llu", version,
                 (unsigned long)f->seq, f->demo, f->valid,
                 (unsigned long long)f->sent_ms);
    if (n < 0 || (size_t)n >= len) return -1;
    used = (size_t)n;
    for (i = 0; i < BD_COUNT; ++i) {
        n = snprintf(out+used, len-used, " %.6f %u", f->value[i], f->age_ms[i]);
        if (n < 0 || (size_t)n >= len-used) return -1;
        used += (size_t)n;
    }
    if (len-used < 2) return -1;
    out[used++] = '\n';
    out[used] = 0;
    return (int)used;
}

static int unsigned_field(char **cursor, uint64_t max, uint64_t *out) {
    char *p=*cursor, *end;
    unsigned long long value;
    while (isspace((unsigned char)*p)) ++p;
    /* scanf unsigned conversions accept negatives and wrap oversized values. */
    if (!isdigit((unsigned char)*p)) return 0;
    errno=0; value=strtoull(p,&end,10);
    if (errno==ERANGE || end==p || (*end && !isspace((unsigned char)*end)) || value>max) return 0;
    *cursor=end; *out=(uint64_t)value; return 1;
}
static int double_field(char **cursor, double *out, double limit) {
    char *p=*cursor, *end;
    double value;
    while (isspace((unsigned char)*p)) ++p;
    errno=0; value=strtod(p,&end);
    if (errno==ERANGE || end==p || (*end && !isspace((unsigned char)*end)) ||
        !isfinite(value) || fabs(value)>limit) return 0;
    *cursor=end; *out=value; return 1;
}

int bd_parse(const char *data, size_t len, bd_frame *out) {
    char b[BD_MAX_PACKET];
    char *cursor;
    bd_frame f;
    uint64_t integer;
    unsigned i, fields;
    if (!data || !out || len == 0 || len >= sizeof b || memchr(data, 0, len)) return 0;
    memcpy(b, data, len); b[len] = 0;
    memset(&f, 0, sizeof f);
    if (len<5 || !isspace((unsigned char)b[4])) return 0;
    #ifdef PERF_LAB
    if (!memcmp(b,"NBG7",4)) fields=BD_COUNT;
    else if (!memcmp(b,"NBG6",4)) fields=22;
    else if (!memcmp(b,"NBG5",4)) fields=11;
    else if (!memcmp(b,"NBG4",4)) fields=9;
    else
#endif
    if (!memcmp(b,"NBG3",4)) fields=8;
    else if (!memcmp(b,"NBG2",4)) fields=5;
    else return 0;
    cursor=b+4;
    if (!unsigned_field(&cursor,UINT32_MAX,&integer)) return 0;
    f.seq=(uint32_t)integer;
    if (!unsigned_field(&cursor,1,&integer)) return 0;
    f.demo=(unsigned)integer;
    if (!unsigned_field(&cursor,(1u<<fields)-1,&integer)) return 0;
    f.valid=(unsigned)integer;
    if (!unsigned_field(&cursor,UINT64_MAX,&f.sent_ms)) return 0;
    for (i = 0; i < fields; ++i) {
        double limit=20000;
#ifdef PERF_LAB
        if (i==BD_ABS_LOAD) limit=25700;
#endif
        if (!double_field(&cursor,&f.value[i],limit) || !unsigned_field(&cursor,999999,&integer)) return 0;
        f.age_ms[i]=(unsigned)integer;
    }
    while (isspace((unsigned char)*cursor)) ++cursor;
    if (*cursor) return 0;
    *out=f;
    return 1;
}

int bd_is_fresh(const bd_frame *f, unsigned i, uint64_t elapsed) {
    unsigned limit = i==BD_IAT || i==BD_TRANS || i==BD_COOLANT ? BD_TEMP_STALE_MS
                   : BD_STALE_MS;
    #ifdef PERF_LAB
    if (i == BD_ETHANOL) limit = 6500;
    if (i >= BD_OIL_TEMP) limit = BD_TEMP_STALE_MS;
#endif
    return i < BD_COUNT && (f->valid & (1u << i)) &&
        elapsed < limit && f->age_ms[i] < limit - elapsed;
}

enum signal_unit { NUMBER, PRESSURE_PSI, PRESSURE_BAR, PRESSURE_KPA, DEGREES,
                   TEMPERATURE, PERCENT, VOLTS, RPM, SPEED, MASS_FLOW, FUEL_FLOW };
static const struct { const char *name; double lo, hi; enum signal_unit unit; } signals[] = {
#define SIGNAL(id, name, lo, hi, unit) {name, lo, hi, unit},
#include "boost_signals.def"
#undef SIGNAL
};

const char *bd_name(unsigned i) { return i < BD_COUNT ? signals[i].name : ""; }

double bd_display_value(unsigned i, double x, int metric) {
    if (i >= BD_COUNT) return x;
    switch (signals[i].unit) {
    case PRESSURE_PSI: return metric ? x / 14.5037738 : x;
    case PRESSURE_BAR: return metric ? x : x * 14.5037738;
    case PRESSURE_KPA: return metric ? x : x * .145037738;
    case TEMPERATURE: return metric ? x : x * 1.8 + 32;
    case SPEED: return metric ? x : x / 1.609344;
    default: return x;
    }
}

const char *bd_unit(unsigned i, int metric) {
    if (i >= BD_COUNT) return "";
    switch (signals[i].unit) {
    case PRESSURE_PSI: case PRESSURE_BAR: return metric ? "bar" : "psi";
    case PRESSURE_KPA: return metric ? "kPa" : "psi";
    case DEGREES: return "deg";
    case TEMPERATURE: return metric ? "\260C" : "\260F";
    case PERCENT: return "%";
    case VOLTS: return "V";
    case RPM: return "rpm";
    case SPEED: return metric ? "km/h" : "mph";
    case MASS_FLOW: return "g/s";
    case FUEL_FLOW: return "L/h";
    default: return "";
    }
}

int bd_bar(unsigned i, double x) {
    if (i >= BD_COUNT || !isfinite(x)) return 0;
    double y = (x - signals[i].lo) * 1000 / (signals[i].hi - signals[i].lo);
    if (y < 0) y = 0;
    if (y > 1000) y = 1000;
    return (int)(y + .5);
}

#endif /* Evo Y / upstream implementation */
