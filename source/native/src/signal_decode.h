#ifndef BD_SIGNAL_DECODE_H
#define BD_SIGNAL_DECODE_H
#include <stddef.h>
#include <math.h>
/* Payload only, after the positive service and identifier. Canonical units
 * belong to the mapping, never to the transport. */
typedef struct {
    unsigned service, identifier, width, signed_value;
    int unavailable; /* -1: no sentinel */
    double scale, offset, minimum, maximum;
} bd_signal;
static inline int bd_signal_decode(const bd_signal *s,const unsigned char *p,size_t n,double *out){
    unsigned raw;int value;double result;
    if(!s||!p||!out||n!=s->width||(n!=1&&n!=2))return 0;
    raw=n==1?p[0]:((unsigned)p[0]<<8)|p[1];
    if(s->unavailable>=0&&raw==(unsigned)s->unavailable)return 0;
    value=(int)raw;
    if(s->signed_value&&(raw&(n==1?128u:32768u)))value-=n==1?256:65536;
    result=value*s->scale+s->offset;
    if(!isfinite(result)||result<s->minimum||result>s->maximum)return 0;
    *out=result;return 1;
}
#endif
