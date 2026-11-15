#include "plugin_factory.h"
#include "diagnostic_worker.h"
#include "cds_telemetry.h"
#include "hud_feed.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <stdio.h>

namespace {const unsigned kHudPoll=0x48554450;const char* const kHudDir="/var/opt/hmi/data/ID5/Plugins/HudTest/";}
class HudTest:public bmw::PluginApp {
    CdsTelemetry cds_;int socket_;pid_t display_,boost_;bool enabled_;unsigned generation_;uint64_t retry_at_;
    void label(const char* s){updateModel(50,bmw::BasicString<char>(s));}
    void reap(pid_t& child){if(child>0){pid_t rc=waitpid(child,0,WNOHANG);if(rc==child||(rc<0&&errno==ECHILD))child=-1;}}
    void stopChildren(){reap(display_);reap(boost_);if(display_>0)kill(display_,SIGTERM);if(boost_>0)kill(boost_,SIGTERM);}
    void shutdown(){enabled_=false;++generation_;cds_.stop();stopChildren();if(socket_>=0){close(socket_);socket_=-1;}}
    void postPoll(){bmw::Parcel p;p.writeUInt32(kHudPoll);p.writeUInt32(generation_);post(bmw::Message(0x40000003u,p),100);}
public:
    HudTest(bmw::IDispatcher& d,bmw::BasicString<char> name,unsigned maj,unsigned min,bmw::RHMIInterfaceVersion v,const bmw::Bundle& c,const bmw::Bundle& r)
      :PluginApp(d,name,maj,min,v,c,r),cds_(*this),socket_(-1),display_(-1),boost_(-1),enabled_(false),generation_(0),retry_at_(0){}
    ~HudTest(){shutdown();}
    virtual bool onFinalize(){shutdown();return true;}
    virtual bool onInit(){
        shutdown();if(!cds_.ready())return false;
        socket_=socket(AF_INET,SOCK_DGRAM,0);if(socket_<0)return false;
        sockaddr_in a;memset(&a,0,sizeof a);a.sin_family=AF_INET;a.sin_port=htons(HUD_STATUS_PORT);a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        if(fcntl(socket_,F_SETFL,O_NONBLOCK)<0||fcntl(socket_,F_SETFD,FD_CLOEXEC)<0||bind(socket_,reinterpret_cast<sockaddr*>(&a),sizeof a)<0){shutdown();return false;}
        bmw::Delegate<bool,const bmw::Parcel&> action;action.m_obj=this;action.m_exec=&bmw::Delegate<bool,const bmw::Parcel&>::execImpl<HudTest,&HudTest::toggle>;
        addHandler(21,action);updateModel(51,false);label("HUD disabled. Test profile: factory HUDTerminal 82/00 at 0101.");postPoll();return true;
    }
    bool toggle(const bmw::Parcel& p){
        bool on;if(!p.readBool(on))return true;
        enabled_=on;updateModel(51,on);
        if(on){cds_.start(1025u);retry_at_=0;label("Starting HUD; waiting for CDS and unique HUDTerminal...");}
        else{cds_.stop();stopChildren();label("HUD disabled; clearing overlay.");}return true;
    }
    virtual bool invokeDataUpdate(unsigned key,const bmw::Parcel& p){
        if(key!=kHudPoll)return PluginApp::invokeDataUpdate(key,p);
        unsigned g;if(!p.readUInt32(g))return false;if(g!=generation_||socket_<0)return true;
        reap(display_);reap(boost_);uint64_t now=bd_mono_ms();cds_.tick(now);
        if(enabled_&&now>=retry_at_){
            char path[160];
            if(display_<=0){snprintf(path,sizeof path,"%shud-display-qnx",kHudDir);const char* args[]={path,"--session",0};display_=launch(args,-1);}
            if(boost_<=0){snprintf(path,sizeof path,"%shud-boost-most-qnx",kHudDir);const char* args[]={path,"--session","--udp-port","42863",0};boost_=launch(args,-1);}
            retry_at_=now+10000;
        }
        if(enabled_){
            hud_feed f;memset(&f,0,sizeof f);memcpy(f.magic,"HUD1",4);f.sent_ms=now;double rpm=0,nav=0;
            f.rpm=cds_.get(0,now,rpm)?static_cast<int>(rpm+.5):-1;f.nav_idle=cds_.get(12,now,nav)&&nav==0;
            sockaddr_in dest;memset(&dest,0,sizeof dest);dest.sin_family=AF_INET;dest.sin_addr.s_addr=htonl(INADDR_LOOPBACK);dest.sin_port=htons(HUD_FEED_PORT);
            sendto(socket_,&f,sizeof f,0,reinterpret_cast<sockaddr*>(&dest),sizeof dest);
        }
        for(unsigned i=0;i<8;++i){hud_status s;ssize_t n=recv(socket_,&s,sizeof s,0);if(n<=0)break;
            if(enabled_&&n==sizeof s&&!memcmp(s.magic,"HUS1",4)){
                label(s.state==1?"HUD active: RPM from CDS, boost in bar from MOST.":
                    s.state==2?"HUD yielded to factory navigation.":s.state==3?"HUD profile/error: overlay stopped.":
                    s.state==4?"Waiting for fresh CDS / navigation state.":"HUDTerminal missing or unsupported (requires unique 82/00 at 0101).");
            }
        }
        postPoll();return true;
    }
};
extern "C" {PluginAppInfo pluginAppInfo={1,"HudTest","owner-development",1,create_app<HudTest>,destroy_app};}
