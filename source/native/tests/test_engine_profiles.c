#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include "engine_profiles.h"
static int identify(const char *s){return bd_engine_identification((const unsigned char*)s,strlen(s));}
int main(void){
    const bd_engine_profile *p=bd_engine_profile_find("n20");unsigned i;double value=0;
    const unsigned char ambient[]={0x62,0x89},timing[]={0xff,0xec},sentinel[]={0xff,0xff},rail[]={4,2};
    assert(p&&p->count==7&&p->count<=BD_PROFILE_MAX_READS);
    assert(!bd_engine_profile_find("unknown"));
    assert(bd_engine_profile_find("b48")->count==0&&bd_engine_profile_find("n13")->count==0);
    assert(bd_signal_decode(&p->reads[0].signal,ambient,2,&value)&&value==98.53515625);
    assert(bd_signal_decode(&p->reads[3].signal,timing,2,&value)&&value==-2);
    assert(!bd_signal_decode(&p->reads[3].signal,sentinel,2,&value));
    assert(!bd_signal_decode(&p->reads[3].signal,timing,1,&value));
    assert(bd_signal_decode(&p->reads[6].signal,rail,2,&value)&&fabs(value-102.6)<1e-9);
    for(i=0;i<p->count;++i){
        assert(p->reads[i].slot<=BD_COUNT&&p->reads[i].period_ms>0);
        assert(p->reads[i].signal.service==1||p->reads[i].signal.service==0x22);
    }
    assert(identify("N20B20O0")==1&&identify("N20")==1);
    assert(identify("DME B48B20")==2&&identify("N13B16")==3);
    assert(identify("N20B20 / B48B20")==-1);
    assert(identify("part123N20B20")==0&&identify("N200")==0);
    assert(identify("N20wrong")==0&&identify("MEVD17.2.4")==0);
    assert(identify("1234567890")==0&&identify("4 cylinders 2000cc")==0);
    puts("Engine profiles: captured scaling, sentinels, unsupported families and ambiguous identity passed.");
    return 0;
}
