#include "boost_cds.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

int main() {
    assert(BD_OIL_TEMP!=BD_ETHANOL && BD_COUNT==31);
    assert(bd_cds_slot(BD_RPM)==0 && bd_cds_slot(BD_SPEED)==1);
    assert(bd_cds_slot(BD_COOLANT)==2 && bd_cds_slot(BD_OIL_TEMP)==3);
    assert(bd_cds_slot(BD_ACCELERATOR)==5 && bd_cds_slot(BD_AMBIENT)==8);
    assert(bd_cds_slot(BD_FUEL_LEVEL)==-1); // CDS fuel is litres, not percent.
    unsigned choices=(1u<<BD_RPM)|(1u<<BD_OIL_TEMP)|(1u<<BD_BOOST)|
        (1u<<BD_TRANS)|(1u<<BD_ETHANOL)|(1u<<BD_BATTERY);
    assert(bd_diagnostic_mask(choices)==((1u<<BD_BOOST)|(1u<<BD_BATTERY)));
    double value;const char* unit;
    assert(bd_evo_reading(BD_COOLANT,100,2,1,value,unit) && value==212);
    assert(!strcmp(unit,"\260F"));
    assert(bd_evo_reading(BD_SPEED,160.9344,2,1,value,unit) && value==160.9344);
    assert(!strcmp(unit,"km/h"));
    assert(bd_evo_reading(BD_SPEED,160.9344,1,2,value,unit) && fabs(value-100)<1e-8);
    assert(!strcmp(unit,"mph"));
    assert(!bd_evo_reading(BD_SPEED,20,1,0,value,unit));
    assert(!bd_evo_reading(BD_IAT,25,0,1,value,unit));
    assert(bd_evo_reading(BD_BOOST,14.5037738,0,0,value,unit) && fabs(value-1)<1e-8);
    assert(!strcmp(unit,"bar"));
    bd_frame frame,parsed;char packet[BD_MAX_PACKET];
    memset(&frame,0,sizeof frame);frame.valid=(1u<<BD_OIL_TEMP)|(1u<<BD_BATTERY);
    frame.value[BD_OIL_TEMP]=89;frame.value[BD_BATTERY]=14.2;
    int length=bd_encode(&frame,packet,sizeof packet);
    assert(length>0 && !memcmp(packet,"NBG7",4));
    assert(bd_parse(packet,length,&parsed) && parsed.value[BD_OIL_TEMP]==89);
    const char legacy[]="NBG5 1 0 256 1000 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 89 0";
    assert(!bd_parse(legacy,sizeof legacy-1,&parsed));
    puts("Configurable BoostDeck: mixed Evo units, CDS routing and NBG7 isolation passed.");
}
