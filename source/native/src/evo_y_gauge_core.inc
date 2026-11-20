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
static const bd_signal obd_signals[]={
#include "profiles/obd.inc"
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
int bd_decode_obd(unsigned pid,const unsigned char *data,size_t len,double *out){
    return decode_signal(obd_signals,sizeof obd_signals/sizeof obd_signals[0],pid,data,len,out);
}

int bd_encode(const bd_frame *f, char *out, size_t len) {
    int n;
    size_t used;
    unsigned i;
    if (!f || !out || !len) return -1;
    #if defined(PERF_LAB) && defined(BD_STOCK_DME)
    const char *version = "NBG5";
#elif defined(PERF_LAB)
    const char *version = "NBG4";
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
static int double_field(char **cursor, double *out) {
    char *p=*cursor, *end;
    double value;
    while (isspace((unsigned char)*p)) ++p;
    errno=0; value=strtod(p,&end);
    if (errno==ERANGE || end==p || (*end && !isspace((unsigned char)*end)) ||
        !isfinite(value) || fabs(value)>10000) return 0;
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
    #if defined(PERF_LAB) && defined(BD_STOCK_DME)
    if (!memcmp(b,"NBG5",4)) fields=BD_COUNT;
    else
#elif defined(PERF_LAB)
    if (!memcmp(b,"NBG4",4)) fields=BD_COUNT;
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
        if (!double_field(&cursor,&f.value[i]) || !unsigned_field(&cursor,999999,&integer)) return 0;
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
    #if defined(PERF_LAB) && defined(BD_STOCK_DME)
    if (i == BD_OIL_TEMP) limit = BD_TEMP_STALE_MS;
#elif defined(PERF_LAB)
    if (i == BD_ETHANOL) limit = 6500;
#endif
    return i < BD_COUNT && (f->valid & (1u << i)) &&
        elapsed < limit && f->age_ms[i] < limit - elapsed;
}

double bd_display_value(unsigned i, double x, int metric) {
#if defined(PERF_LAB) && defined(BD_STOCK_DME)
    if(i==BD_OIL_TEMP&&!metric)return x*1.8+32;
#endif
    if (i == BD_BOOST && metric) return x / 14.5037738;
    if ((i == BD_IAT || i == BD_TRANS || i == BD_COOLANT) && !metric) return x * 1.8 + 32;
    if ((i == BD_RAIL || i == BD_OIL) && !metric) return x * 14.5037738;
    return x;
}
const char *bd_unit(unsigned i, int metric) {
    if (i == BD_BOOST || i == BD_RAIL || i == BD_OIL) return metric ? "bar" : "psi";
    if (i == BD_AFR) return "AFR (gas eq.)";
    if (i == BD_TIMING) return "deg BTDC (cyl 1)";
#if defined(PERF_LAB) && !defined(BD_STOCK_DME)
    if (i == BD_ETHANOL) return "%";
#endif
    return metric ? "C" : "F";
}
int bd_bar(unsigned i, double x) {
    static const double lo[BD_COUNT] = {0, 7, -30, -20, -20, 0, 0, -20};
    static const double hi[BD_COUNT] = {45, 25, 50, 100, 160, 300, 8, 130};
    double y;
    if (i >= BD_COUNT || !isfinite(x)) return 0;
#if defined(PERF_LAB) && defined(BD_STOCK_DME)
    if(i==BD_OIL_TEMP)return x<=0?0:x>=150?1000:(int)(x*1000./150.+.5);
#elif defined(PERF_LAB)
    if (i == BD_ETHANOL) return x < 0 ? 0 : x >= 100 ? 1000 : (int)(x * 10 + .5);
#endif
    y = (x - lo[i]) * 1000 / (hi[i] - lo[i]);
    if (y < 0) y = 0;
    if (y > 1000) y = 1000;
    return (int)(y + .5);
}
