#ifndef EVO_HUD_TRACE_H
#define EVO_HUD_TRACE_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
static inline int hud_trace_s16(const uint8_t* p){unsigned n=((unsigned)p[0]<<8)|p[1];return n&32768?(int)n-65536:(int)n;}
static inline void hud_trace_record(FILE* out,uint64_t now,uint16_t source,uint16_t target,
    unsigned fkt,unsigned op,const uint8_t* p,size_t n){
    fprintf(out,"HUD_TRACE t=%llu src=%04X dst=%04X fkt=%03X op=%X len=%lu payload=",
        (unsigned long long)now,source,target,fkt,op,(unsigned long)n);
    size_t shown=n<64?n:64;
    // Do not record route text or bitmap contents. Instance handle suffices
    // to identify those calls during an initialization/ownership audit.
    bool redact=fkt==0x210||fkt==0xc05;
    if(redact&&shown>2)shown=2;
    for(size_t i=0;i<shown;++i)fprintf(out,"%02X",p[i]);
    if(redact)fputs(" [content redacted]",out);
    else if(shown<n)fputs(" [truncated]",out);
    if(fkt==0xc06&&n==12)fprintf(out," handle=%u coX=%d coY=%d ctX=%d ctY=%d angle=%d",
        ((unsigned)p[0]<<8)|p[1],hud_trace_s16(p+2),hud_trace_s16(p+4),
        hud_trace_s16(p+6),hud_trace_s16(p+8),hud_trace_s16(p+10));
    fputc('\n',out);
}
#endif
