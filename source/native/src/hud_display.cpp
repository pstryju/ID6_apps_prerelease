#include "cluster/hud_terminal.hpp"
#include "gauge_core.h"
#include "hud_feed.h"
extern "C" {
#include "most/dsi_abi.h"
#include "most/dsi_client.h"
}
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>

using namespace nbt;
namespace {
volatile sig_atomic_t running=1;
pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
hudterm::Core* core=0;
uint16_t self_address=0,host_address=0;
bool link_up=false;
bool dsi_exited=false;
void stop(int){if(running)alarm(15);running=0;}
void disconnected(void*){pthread_mutex_lock(&lock);link_up=false;core->setLink(false,0,(uint32_t)bd_mono_ms());pthread_mutex_unlock(&lock);}
void client(void*,uint32_t,uint16_t address){
    pthread_mutex_lock(&lock);self_address=address;link_up=false;core->setLink(false,0,(uint32_t)bd_mono_ms());pthread_mutex_unlock(&lock);
    dsi_register_target();
}
void up(void*,uint16_t address){
    pthread_mutex_lock(&lock);link_up=address==0x0101;core->setLink(link_up,self_address,(uint32_t)bd_mono_ms());pthread_mutex_unlock(&lock);
}
void registry(void*,const uint16_t* pairs,int n){
    unsigned count=0;uint16_t host=0;
    for(int i=0;i<n;++i)if(pairs[2*i+1]==0x8200){++count;host=pairs[2*i];}
    if(count!=1||host!=0x0101)host=0; // exact packaged HUD profile; no guessed cluster
    pthread_mutex_lock(&lock);host_address=host;core->setHost(host,(uint32_t)bd_mono_ms());pthread_mutex_unlock(&lock);
}
void down(void*){pthread_mutex_lock(&lock);host_address=0;core->setHost(0,(uint32_t)bd_mono_ms());pthread_mutex_unlock(&lock);}
void message(void*,uint32_t tr,uint32_t kind,uint32_t flags,uint16_t source,uint16_t target,
    uint8_t fb,uint8_t inst,uint16_t fkt,uint8_t op,uint32_t tag,const uint8_t* p,size_t n){
    if(fb!=0x82||inst!=0||!(kind==1||kind==2))return;
    most::View v;v.txType=tr;v.varTag=kind;v.varVal=flags;v.src=source;v.dst=target;
    v.fblock=fb;v.inst=inst;v.fkt=fkt;v.op=op;v.hdrTag=tag;v.data=p;v.len=n;v.rxMs=(uint32_t)bd_mono_ms();
    if(most::classify(v)==most::kTxStatus||most::classify(v)==most::kSegFirst||most::classify(v)==most::kSegMiddle||most::classify(v)==most::kSegLast)return;
    pthread_mutex_lock(&lock);core->onMessage(v,v.rxMs);pthread_mutex_unlock(&lock);
}
void* dsiThread(void*){
    dsi_handlers_t h;memset(&h,0,sizeof h);h.disconnected=disconnected;h.most_client=client;
    h.registry=registry;h.target_up=up;h.target_down=down;h.message=message;
    dsi_client_run("MOSTSubSystem",&h,0);
    pthread_mutex_lock(&lock);dsi_exited=true;core->setLink(false,0,(uint32_t)bd_mono_ms());pthread_mutex_unlock(&lock);return 0;
}
int listener(unsigned port){
    int fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0)return -1;
    sockaddr_in a;memset(&a,0,sizeof a);a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(port);
    if(fcntl(fd,F_SETFL,O_NONBLOCK)<0||bind(fd,reinterpret_cast<sockaddr*>(&a),sizeof a)<0){close(fd);return -1;}return fd;
}
void pump(){
    most::Msg m;bool have;
    pthread_mutex_lock(&lock);core->tick((uint32_t)bd_mono_ms());have=core->next((uint32_t)bd_mono_ms(),m);pthread_mutex_unlock(&lock);
    if(have&&dsi_send_fn(m.fblock,m.inst,m.fkt,m.opType,m.data.empty()?0:&m.data[0],m.data.size())!=0){
        pthread_mutex_lock(&lock);core->sendFailed((uint32_t)bd_mono_ms());pthread_mutex_unlock(&lock);
    }
}
void clear(bool explicit_stop=false){
    pthread_mutex_lock(&lock);
    if(explicit_stop&&core->status()==hudterm::kFailed)core->reset((uint32_t)bd_mono_ms());
    if(core->status()==hudterm::kReady){
        hudterm::Batch b("clear HUD gauges",true,hudterm::kSlotAll);
        b.add(hudterm::kSetVisibility,hudterm::setVisibility(1,false));
        b.add(hudterm::kTextareaSetText,hudterm::setText(2,""));b.add(hudterm::kTextareaSetText,hudterm::setText(3,""));
        b.add(hudterm::kTextareaSetText,hudterm::setText(4,""));
        core->submit(b,(uint32_t)bd_mono_ms());
    }
    pthread_mutex_unlock(&lock);
}
}
int main(int argc,char** argv){
    if(argc!=2||strcmp(argv[1],"--session"))return 2;
    pid_t parent=getppid();if(parent<=1)return 2;
    signal(SIGINT,stop);signal(SIGTERM,stop);signal(SIGPIPE,SIG_IGN);signal(SIGALRM,SIG_DFL);
    hudterm::Config cfg;for(int i=1;i<=6;++i)cfg.handles.allow(i);cfg.navFrame=1;cfg.maxBatch=12;cfg.maxInFlight=3;
    core=new hudterm::Core(cfg);
    int feed_fd=listener(HUD_FEED_PORT),boost_fd=listener(HUD_BOOST_PORT);
    if(feed_fd<0||boost_fd<0||most_dsi_bind()!=0)return 1;
    dsi_client_set_name("hud_gauges");dsi_client_set_target(0x8200,0x82,0);
    pthread_t thread;alarm(15);if(pthread_create(&thread,0,dsiThread,0)!=0)return 1;
    hud_feed feed;memset(&feed,0,sizeof feed);bd_frame boost;memset(&boost,0,sizeof boost);
    uint64_t boost_received=0,next_frame=0,next_status=0;bool drawn=false;
    unsigned last_epoch=0;
    while(running&&getppid()==parent){
        alarm(15);uint64_t now=bd_mono_ms();
        pthread_mutex_lock(&lock);bool exited=dsi_exited;pthread_mutex_unlock(&lock);
        if(exited)break;
        for(unsigned i=0;i<8;++i){hud_feed next;ssize_t n=recv(feed_fd,&next,sizeof next,0);if(n<=0)break;if(n==sizeof next&&hud_feed_valid(next,now))feed=next;}
        for(unsigned i=0;i<8;++i){char p[BD_MAX_PACKET];ssize_t n=recv(boost_fd,p,sizeof p,0);if(n<=0)break;bd_frame next;
            if(bd_parse(p,(size_t)n,&next)&&next.sent_ms&&next.sent_ms<=now&&next.sent_ms>=boost_received){boost=next;boost_received=next.sent_ms;}}
        bool data_ok=hud_feed_valid(feed,now)&&feed.nav_idle&&feed.rpm>=0;
        pthread_mutex_lock(&lock);hudterm::Status state=core->status();unsigned epoch=core->epoch();pthread_mutex_unlock(&lock);
        if(epoch!=last_epoch){drawn=false;last_epoch=epoch;}
        if(!data_ok&&drawn){clear();drawn=false;}
        if(data_ok&&state==hudterm::kReady&&now>=next_frame){
            char rpm[32],pressure[32];snprintf(rpm,sizeof rpm,"%d RPM",feed.rpm);
            bool valid=boost_received&&bd_is_fresh(&boost,BD_BOOST,now-boost_received);
            if(valid)snprintf(pressure,sizeof pressure,"%.2f",boost.value[BD_BOOST]/14.5037738);else strcpy(pressure,"--");
            hudterm::Batch b("HUD RPM and boost",true);
            b.add(hudterm::kTextareaSetText,hudterm::setText(2,rpm));b.add(hudterm::kTextareaSetText,hudterm::setText(3,pressure));
            b.add(hudterm::kTextareaSetText,hudterm::setText(4,"bar"));b.add(hudterm::kSetVisibility,hudterm::setVisibility(1,true));
            pthread_mutex_lock(&lock);core->submit(b,(uint32_t)now);pthread_mutex_unlock(&lock);drawn=true;next_frame=now+250;
        }
        pump();
        if(now>=next_status){
            hud_status s;memset(&s,0,sizeof s);memcpy(s.magic,"HUS1",4);
            pthread_mutex_lock(&lock);s.state=core->status();s.host=host_address;pthread_mutex_unlock(&lock);
            if(s.state==hudterm::kReady&&!data_ok)s.state=4;
            sockaddr_in d;memset(&d,0,sizeof d);d.sin_family=AF_INET;d.sin_addr.s_addr=htonl(INADDR_LOOPBACK);d.sin_port=htons(HUD_STATUS_PORT);
            sendto(feed_fd,&s,sizeof s,0,reinterpret_cast<sockaddr*>(&d),sizeof d);next_status=now+1000;
        }
        poll(0,0,20);
    }
    alarm(15);clear(true);uint64_t until=bd_mono_ms()+1000;while(bd_mono_ms()<until){pump();poll(0,0,20);}
    dsi_unregister_target();dsi_unregister_client();dsi_client_stop();pthread_join(thread,0);
    close(feed_fd);close(boost_fd);alarm(0);return 0;
}
