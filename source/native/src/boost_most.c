#define _POSIX_C_SOURCE 200809L
// Read-only profile-driven gauge worker. MOST only; no ECU writes.
#include "most/most_link.h"
#include "most/dsi_client.h"
#include "diagnostic_lease.h"
#include "gauge_core.h"
#include "engine_profiles.h"
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static volatile sig_atomic_t running=1;
static void stop(int sig){(void)sig;if(running)alarm(15);running=0;}
static int publish(int sock,struct sockaddr_in* dest,bd_frame* f,const uint64_t* observed){
    char packet[BD_MAX_PACKET];unsigned i;int n;uint64_t now=bd_mono_ms();
    f->sent_ms=now;++f->seq;
    for(i=0;i<BD_COUNT;++i){uint64_t age=now-observed[i];f->age_ms[i]=age>999999?999999:(unsigned)age;}
    n=bd_encode(f,packet,sizeof packet);
    return n>0&&sendto(sock,packet,(size_t)n,0,(struct sockaddr*)dest,sizeof *dest)==n;
}
static int argument_number(const char* s,unsigned lo,unsigned hi,unsigned* value){
    char* end;unsigned long n;if(!*s)return 0;
    n=strtoul(s,&end,10);if(*end||n<lo||n>hi)return 0;*value=(unsigned)n;return 1;
}
/* Bounded read-only identification. Never print raw identification or VIN.
 * Real DME software often returns only part numbers: then auto stays unknown. */
static const bd_engine_profile* detect_profile(void){
    const unsigned dids[]={0xf150,0xf187,0xf189};unsigned i;int identity=0;
    for(i=0;i<3&&running;++i){
        unsigned char request[]={0x22,(unsigned char)(dids[i]>>8),(unsigned char)dids[i]},reply[128];
        size_t n=0;int match,lock=reserve_gateway();enum most_rc rc;
        if(lock<0)return 0;
        rc=most_request_wait(0x12,request,3,reply,sizeof reply,&n,2000);close(lock);
        if(rc==MOST_CLOSED)return 0;
        if(rc!=MOST_REPLY||n<3||reply[0]!=0x62||reply[1]!=request[1]||reply[2]!=request[2])continue;
        match=bd_engine_identification(reply+3,n-3);
        if(match<0||(match&&identity&&match!=identity))return 0;
        if(match)identity=match;
    }
    return identity?&bd_engine_profiles[identity-1]:0;
}
int main(int argc,char** argv){
    const bd_engine_profile* profile=0;const char* selected=0;char setting[16];
    uint64_t due[BD_PROFILE_MAX_READS]={0},observed[BD_COUNT]={0},ambient_at=0;
    unsigned udp_port=BD_UDP_PORT,cycles=0,iteration=0,i;int session=0,trace=0,sock,exit_code=0;
    pid_t parent=getppid();struct sockaddr_in dest;bd_frame frame;double ambient=0;int have_ambient=0;
    for(i=1;i<(unsigned)argc;++i){
        if(!strcmp(argv[i],"--session"))session=1;
        else if(!strcmp(argv[i],"--trace"))trace=1;
        else if(!strcmp(argv[i],"--profile")&&i+1<(unsigned)argc&&!selected)selected=argv[++i];
        else if(!strcmp(argv[i],"--udp-port")&&i+1<(unsigned)argc&&argument_number(argv[i+1],1,65535,&udp_port))++i;
        else if(!strcmp(argv[i],"--cycles")&&i+1<(unsigned)argc&&argument_number(argv[i+1],1,100000,&cycles))++i;
        else {fputs("Usage: boostdeck-most-qnx [--profile n20|b48|n13|auto] [--session] [--trace] [--cycles N] [--udp-port N]\n",stderr);return 2;}
    }
    if(!selected){if(!bd_engine_profile_setting(argv[0],setting)){fputs("Invalid engine-profile.txt\n",stderr);return 2;}selected=setting;}
    if(strcmp(selected,"auto")){
        profile=bd_engine_profile_find(selected);
        if(!profile){fputs("Unknown engine profile\n",stderr);return 2;}
        if(!profile->count){fprintf(stderr,"Engine profile %s has no validated mapping\n",profile->name);return 3;}
    }
    if(session&&parent<=1)return 2;
    signal(SIGINT,stop);signal(SIGTERM,stop);signal(SIGPIPE,SIG_IGN);signal(SIGALRM,SIG_DFL);
    most_set_running(&running);
#ifdef BD_HUD_SOURCE
    dsi_client_set_name("hud_boost");
#else
    dsi_client_set_name("boostdeck");
#endif
    if(trace)most_log_level(2);
    sock=socket(AF_INET,SOCK_DGRAM,0);if(sock<0)return 1;
    memset(&dest,0,sizeof dest);dest.sin_family=AF_INET;dest.sin_port=htons((uint16_t)udp_port);
    dest.sin_addr.s_addr=htonl(INADDR_LOOPBACK);memset(&frame,0,sizeof frame);
    alarm(30);
    if(most_open(2000)!=MOST_UP){fputs("BoostDeck MOST unavailable\n",stderr);exit_code=1;goto done;}
    if(!profile)profile=detect_profile();
    if(!profile||!profile->count||profile->count>BD_PROFILE_MAX_READS){
        fprintf(stderr,"Engine profile %s: identification unknown/ambiguous or mapping unavailable; no gauge reads\n",profile?profile->name:"unknown");
        exit_code=3;goto done;
    }
    fprintf(stderr,"BoostDeck MOST connected; read-only %s profile\n",profile->name);
    while(running&&(!cycles||iteration<cycles)&&(!session||getppid()==parent)){
        alarm(30);
        for(i=0;i<profile->count&&running;++i){
            const bd_profile_read* read=&profile->reads[i];
            const bd_signal* signal=&read->signal;
#ifdef BD_HUD_SOURCE
            if(read->slot!=BD_COUNT&&read->slot!=BD_BOOST)continue;
#endif
            uint64_t now=bd_mono_ms();unsigned slot=read->slot;int lock;
            unsigned char request[3],reply[64];unsigned request_size;
            size_t n=0;enum most_rc rc;double v=0;int ok=0;
            if(now<due[i])continue;
            lock=reserve_gateway();if(lock<0)continue; // ages continue while another tester owns the lease
            request[0]=(unsigned char)signal->service;
            request_size=signal->service==0x22?3:2;
            request[1]=(unsigned char)(request_size==3?signal->identifier>>8:signal->identifier);
            request[2]=(unsigned char)signal->identifier;
            rc=most_request_wait(0x12,request,request_size,reply,sizeof reply,&n,2000);
            close(lock);
            now=bd_mono_ms();due[i]=now+read->period_ms;
            if(rc==MOST_CLOSED){exit_code=1;running=0;break;}
            if(rc==MOST_REPLY){
                if(n>=request_size&&reply[0]==request[0]+0x40&&reply[1]==request[1]&&
                   (request_size==2||reply[2]==request[2]))
                    ok=bd_signal_decode(signal,reply+request_size,n-request_size,&v);
            }
            if(trace){unsigned j;fprintf(stderr,"MOST read %02X:%04X rc=%d raw=",signal->service,signal->identifier,(int)rc);
                for(j=0;j<n;++j){fprintf(stderr,"%02X",reply[j]);}
                fprintf(stderr," valid=%d value=%.6f\n",ok,v);}
            if(slot==BD_COUNT){have_ambient=ok;if(ok){ambient=v;ambient_at=now;}}
            else {
                frame.valid&=~(1u<<slot);
                if(slot==BD_BOOST){ok=ok&&have_ambient&&now>=ambient_at&&now-ambient_at<1800;v-=ambient;}
                v*=read->multiplier;
                if(ok){frame.value[slot]=v;observed[slot]=now;frame.valid|=1u<<slot;}
            }
            if(!have_ambient||now-ambient_at>=1800)frame.valid&=~(1u<<BD_BOOST);
            publish(sock,&dest,&frame,observed); // a slow channel never delays publication of the last boost
        }
        publish(sock,&dest,&frame,observed);++iteration;
        if(running)poll(0,0,20);
    }
done:
    frame.valid=0;publish(sock,&dest,&frame,observed);
    alarm(15);if(most_close()!=0)exit_code=1;close(sock);alarm(0);return exit_code;
}
