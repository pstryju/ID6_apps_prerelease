#include "gauge_core.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    /* De-identified N20 live replies: 410582, 415C81, 41230410. */
    const unsigned char coolant[]={0x82},oil[]={0x81},rail[]={4,0x10};
    bd_frame frame,parsed;char packet[BD_MAX_PACKET];int n;double value=0;
    assert(bd_decode_obd(5,coolant,1,&value)&&value==90);
    assert(bd_decode_obd(0x5c,oil,1,&value)&&value==89);
    assert(bd_decode_obd(0x23,rail,2,&value)&&fabs(value-104)<.000001);
    assert(!bd_decode_obd(0x23,rail,1,&value));
    assert(!bd_decode_obd(0x5c,rail,2,&value));
    assert(!bd_decode_obd(0x99,oil,1,&value));
    assert(bd_display_value(BD_OIL_TEMP,100,0)==212);
    assert(bd_display_value(BD_OIL_TEMP,100,1)==100);
    assert(!strcmp(bd_unit(BD_OIL_TEMP,1),"C"));
    assert(bd_bar(BD_OIL_TEMP,75)==500);
    assert(bd_bar(BD_OIL_TEMP,200)==1000);
    memset(&frame,0,sizeof frame);frame.valid=1u<<BD_OIL_TEMP;frame.value[BD_OIL_TEMP]=89;
    frame.sent_ms=1000;
    n=bd_encode(&frame,packet,sizeof packet);
    assert(n>0&&!memcmp(packet,"NBG5",4));
    assert(bd_parse(packet,(size_t)n,&parsed)&&parsed.value[BD_OIL_TEMP]==89);
    assert(bd_is_fresh(&parsed,BD_OIL_TEMP,3499));
    assert(!bd_is_fresh(&parsed,BD_OIL_TEMP,3500));
    packet[3]='4';assert(!bd_parse(packet,(size_t)n,&parsed));
    puts("Stock core: captured OBD values, oil units, staleness and NBG5 isolation passed.");
    return 0;
}
