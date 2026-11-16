#define _POSIX_C_SOURCE 200809L
#include "most/most_link.h"
#include "most/dsi_client.h"
#include "diagnostic_lease.h"
#include "gauge_core.h"
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc,char** argv){
    unsigned dids[]={0x4201,0x4205,0x5889,0x4a49,0x580f,0x586f,0x56d7};unsigned i,j;
    int exhaust_info=argc==2&&!strcmp(argv[1],"--exhaust-info");
    int engine_info=argc==2&&!strcmp(argv[1],"--engine-info");
    unsigned count=sizeof dids/sizeof dids[0];
    if(argc!=1&&!exhaust_info&&!engine_info)return 2;
    if(exhaust_info){dids[0]=0xf150;dids[1]=0xf187;dids[2]=0xf189;dids[3]=0x60fd;count=4;}
    if(engine_info){dids[0]=0xf150;dids[1]=0xf187;dids[2]=0xf189;count=3;}
    int lock=reserve_gateway();enum most_open_rc opened;
    if(lock<0){puts("MOST_PROBE lease busy; close diagnostic apps");return 1;}
    setvbuf(stdout,0,_IONBF,0);setvbuf(stderr,0,_IONBF,0);
    alarm(25);dsi_client_set_name("evo_most_probe");most_log_level(2);
    puts(engine_info?"MOST_PROBE engine identification ONLY: 22 F150/F187/F189; no writes or VIN":
        exhaust_info?"MOST_PROBE exhaust identification ONLY: 22 F150/F187/F189/60FD; no actuation":
        "MOST_PROBE START read-only known DME gauge DIDs and OBD rail; total limit 25s");
    opened=most_open(2000);printf("MOST_PROBE OPEN %d\n",(int)opened);
    if(opened==MOST_UP)for(i=0;i<count;++i){
        unsigned char req[]={0x22,(unsigned char)(dids[i]>>8),(unsigned char)dids[i]},reply[64];
        size_t n=0;double value=0;enum most_rc rc;
        rc=most_request_wait(0x12,req,3,reply,sizeof reply,&n,2000);
        printf("MOST_PROBE DID %04X rc=%d bytes=",dids[i],(int)rc);
        for(j=0;j<n;++j)printf("%02X",reply[j]);
        if(rc==MOST_REPLY&&n>=3&&reply[0]==0x62&&reply[1]==req[1]&&reply[2]==req[2]&&
           bd_decode_did(dids[i],reply+3,n-3,&value))printf(" decoded=%.6f unit=%s",value,
               dids[i]==0x4201||dids[i]==0x4205?"kPa":dids[i]==0x5889?"lambda":
               dids[i]==0x4a49?"deg":dids[i]==0x580f?"C":"bar");
        puts("");
    }
    if(opened==MOST_UP&&!exhaust_info&&!engine_info){
        unsigned char req[]={1,0x23},reply[64];size_t n=0;double value=0;
        enum most_rc rc=most_request_wait(0x12,req,2,reply,sizeof reply,&n,2000);
        printf("MOST_PROBE OBD 23 rc=%d bytes=",(int)rc);
        for(j=0;j<n;++j)printf("%02X",reply[j]);
        if(rc==MOST_REPLY&&n>=2&&reply[0]==0x41&&reply[1]==0x23&&bd_decode_obd(0x23,reply+2,n-2,&value))printf(" bar=%.6f",value);
        puts("");
    }
    printf("MOST_PROBE CLOSE %d\n",most_close());close(lock);alarm(0);
    puts("MOST_PROBE COMPLETE");return opened==MOST_UP?0:1;
}
