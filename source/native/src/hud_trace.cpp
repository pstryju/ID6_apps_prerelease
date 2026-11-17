// Observe terminal traffic only. No HUD method, DME request or frame write.
// Quarantined after a target SIGSEGV and reported head-unit restart.
#error "HUD observer quarantined: resolve registration and runtime lifecycle before rebuilding"
#include "gauge_core.h"
#include "hud_trace.h"
extern "C" {
#include "most/dsi_abi.h"
#include "most/dsi_client.h"
}
#include <pthread.h>
#include <signal.h>
#include <poll.h>
#include <unistd.h>
#include <string.h>
namespace {
volatile sig_atomic_t running=1;
pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
bool exited=false;unsigned records=0;
void stop(int){if(running)alarm(15);running=0;}
void client(void*,uint32_t,uint16_t address){
    printf("HUD_TRACE CLIENT %04X; registering observation target only\n",address);
    dsi_register_target();
}
void message(void*,uint32_t tr,uint32_t kind,uint32_t flags,uint16_t src,uint16_t dst,
    uint8_t fb,uint8_t inst,uint16_t fkt,uint8_t op,uint32_t,const uint8_t* p,size_t n){
    if(fb!=0x82||inst!=0||(kind!=1&&kind!=2))return;
    if(records>=4096){running=0;return;}
    ++records;
    printf("HUD_META tr=%lu kind=%lu flags=%08lX\n",(unsigned long)tr,(unsigned long)kind,(unsigned long)flags);
    hud_trace_record(stdout,bd_mono_ms(),src,dst,fkt,op,p,n);
}
void* observe(void*){
    dsi_handlers_t h;memset(&h,0,sizeof h);h.most_client=client;h.message=message;
    dsi_client_run("MOSTSubSystem",&h,0);
    pthread_mutex_lock(&mutex);exited=true;pthread_mutex_unlock(&mutex);return 0;
}
}
int main(int argc,char**){
    if(argc!=1)return 2;
    setvbuf(stdout,0,_IOLBF,4096);
    signal(SIGINT,stop);signal(SIGTERM,stop);signal(SIGPIPE,SIG_IGN);signal(SIGALRM,SIG_DFL);
    if(most_dsi_bind()!=0){puts("HUD_TRACE no DSI library");return 1;}
    dsi_client_set_name("hud_audit");dsi_client_set_target(0x8200,0x82,0);
    puts("HUD_TRACE START 45s / 4096 records; overlay must be OFF; no terminal methods sent");
    pthread_t thread;alarm(60);
    if(pthread_create(&thread,0,observe,0)!=0)return 1;
    uint64_t deadline=bd_mono_ms()+45000;
    while(running&&bd_mono_ms()<deadline){
        pthread_mutex_lock(&mutex);bool done=exited;pthread_mutex_unlock(&mutex);
        if(done)break;poll(0,0,50);
    }
    alarm(15);dsi_unregister_target();dsi_unregister_client();dsi_client_stop();pthread_join(thread,0);alarm(0);
    printf("HUD_TRACE COMPLETE records=%u%s\n",records,records>=4096?" LIMIT_REACHED":"");
    return records?0:1;
}
