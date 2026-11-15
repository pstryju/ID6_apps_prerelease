#define _POSIX_C_SOURCE 200809L
// Read-only N20 gauge worker. MOST only: no HSFZ connect or ECU write fallback.
#include "most/most_link.h"
#include "most/dsi_client.h"
#include "diagnostic_lease.h"
#include "gauge_core.h"
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
int main(int argc,char** argv){
    // Payloads and formulas were observed on this N20. OBD rail replaces
    // unsupported 2256D7. No EGS, tune services or temperature diagnostics.
    static const unsigned dids[]={0x4201,0x4205,0x5889,0x4a49,0x580f,0x586f,0};
    static const unsigned slots[]={BD_COUNT,BD_BOOST,BD_AFR,BD_TIMING,BD_IAT,BD_OIL,BD_RAIL};
    static const unsigned periods[]={800,80,300,300,2000,500,500};
    uint64_t due[7]={0},observed[BD_COUNT]={0},ambient_at=0;
    unsigned udp_port=BD_UDP_PORT,cycles=0,iteration=0,i;int session=0,trace=0,sock,exit_code=0;
    pid_t parent=getppid();struct sockaddr_in dest;bd_frame frame;double ambient=0;int have_ambient=0;
    for(i=1;i<(unsigned)argc;++i){
        if(!strcmp(argv[i],"--session"))session=1;
        else if(!strcmp(argv[i],"--trace"))trace=1;
        else if(!strcmp(argv[i],"--udp-port")&&i+1<(unsigned)argc&&argument_number(argv[i+1],1,65535,&udp_port))++i;
        else if(!strcmp(argv[i],"--cycles")&&i+1<(unsigned)argc&&argument_number(argv[i+1],1,100000,&cycles))++i;
        else {fputs("Usage: boostdeck-most-qnx [--session] [--trace] [--cycles N] [--udp-port N]\n",stderr);return 2;}
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
    fputs("BoostDeck MOST connected; read-only N20 profile\n",stderr);
    while(running&&(!cycles||iteration<cycles)&&(!session||getppid()==parent)){
        alarm(30);
        for(i=0;i<7&&running;++i){
#ifdef BD_HUD_SOURCE
            if(i>=2)continue; // the HUD needs boost only, not a second full engine scan
#endif
            uint64_t now=bd_mono_ms();unsigned slot=slots[i];int lock;
            unsigned char request[]={0x22,(unsigned char)(dids[i]>>8),(unsigned char)dids[i]},reply[64];
            size_t n=0;enum most_rc rc;double v=0;int ok=0;
            if(now<due[i])continue;
            lock=reserve_gateway();if(lock<0)continue; // ages continue while another tester owns the lease
            if(!dids[i]){request[0]=1;request[1]=0x23;}
            rc=most_request_wait(0x12,request,dids[i]?3:2,reply,sizeof reply,&n,2000);
            close(lock);
            now=bd_mono_ms();due[i]=now+periods[i];
            if(rc==MOST_CLOSED){exit_code=1;running=0;break;}
            if(rc==MOST_REPLY){
                if(dids[i]&&n>=3&&reply[0]==0x62&&reply[1]==request[1]&&reply[2]==request[2])
                    ok=bd_decode_did(dids[i],reply+3,n-3,&v);
                else if(!dids[i]&&n>=2&&reply[0]==0x41&&reply[1]==0x23)
                    ok=bd_decode_obd(0x23,reply+2,n-2,&v);
            }
            if(trace){unsigned j;fprintf(stderr,"MOST read %04X rc=%d raw=",dids[i]?dids[i]:0x0123,(int)rc);
                for(j=0;j<n;++j){fprintf(stderr,"%02X",reply[j]);}
                fprintf(stderr," valid=%d value=%.6f\n",ok,v);}
            if(slot==BD_COUNT){have_ambient=ok;if(ok){ambient=v;ambient_at=now;}}
            else {
                frame.valid&=~(1u<<slot);
                if(slot==BD_BOOST){ok=ok&&have_ambient&&now>=ambient_at&&now-ambient_at<1800;v=(v-ambient)*.145037738;}
                if(slot==BD_AFR)v*=14.7;
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
