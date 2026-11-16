#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "most/dsi_abi.h"
#include "fake_dme.h"
static void report(void){
    fprintf(stderr,"FAKE_MOST_CLOSE target=%d client=%d free=%d\n",fake_dsi_req_count(5),fake_dsi_req_count(0x10),fake_free_calls);
    fprintf(stderr,"FAKE_MOST_REQUESTS ");
    {const char* requests[]={"224201","224205","225889","224A49","22580F","22586F","0123","2256D7","22DA12","22F150","22F187","22F189","14","2F","10"};unsigned i;
     for(i=0;i<sizeof requests/sizeof requests[0];++i)fprintf(stderr,"%s=%d ",requests[i],fake_dme_count(requests[i]));}
    fputc('\n',stderr);
}
int most_fake_bind(void){
    const char* mode=getenv("BOOST_MOST_FAKE");
    static const unsigned char ambient[]={0x62,0x89},charge[]={0x31,0x4d},afr[]={0x10,0x17},timing[]={0xff,0xec},iat[]={0x6c},oil[]={0x09,0xd5},rail[]={0x41,0x23,4,2};
    if(!mode||!strcmp(mode,"missing"))return -1;
    fake_dsi_reset();atexit(report);
    fake_dme_start(!strcmp(mode,"no-target")?FAKE_NO_TARGET:FAKE_UP);
    fake_dme_plain(0x4201,ambient,sizeof ambient);fake_dme_plain(0x4205,charge,sizeof charge);
    fake_dme_plain(0x5889,afr,sizeof afr);fake_dme_plain(0x4a49,timing,sizeof timing);
    fake_dme_plain(0x580f,iat,sizeof iat);fake_dme_plain(0x586f,oil,sizeof oil);
    fake_dme_raw("0123",rail,sizeof rail);
    if(!strcmp(mode,"auto-n20")||!strcmp(mode,"auto-conflict"))fake_dme_plain(0xf150,(const unsigned char*)"N20B20O0",8);
    if(!strcmp(mode,"auto-b48"))fake_dme_plain(0xf150,(const unsigned char*)"B48B20",6);
    if(!strcmp(mode,"auto-conflict"))fake_dme_plain(0xf187,(const unsigned char*)"N13B16",6);
    if(!strcmp(mode,"reject-ambient"))fake_dme_refuse("224201",0x31,0);
    if(!strcmp(mode,"reject-rail"))fake_dme_refuse("0123",0x31,0);
    if(!strcmp(mode,"drop"))fake_dme_drop_link_after(5);
    if(!strcmp(mode,"slow"))fake_dme_hold_ms(600);
    return 0;
}
