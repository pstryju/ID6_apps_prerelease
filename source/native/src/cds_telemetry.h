#ifndef BD_CDS_TELEMETRY_H
#define BD_CDS_TELEMETRY_H
#include "cds_values.h"
#include "gauge_core.h"
#include <pthread.h>

class CdsTelemetry {
    typedef bmw::Delegate<bool,const bmw::BasicString<unsigned short>&> Callback;
    bmw::PluginApp& app_;
    pthread_mutex_t mutex_;
    bool ready_,active_;
    unsigned mask_;
    uint64_t request_at_,since_;
    double values_[13];
    uint64_t times_[13];
    bool valid_[13];
    static const char* property(unsigned i){
        static const char* names[]={"engine.RPMSpeed","driving.speedActual","engine.temperature",
            "engine.torque","driving.acceleratorPedal","driving.gear","sensors.fuel",
            "sensors.temperatureExterior","vehicle.units","vehicle.unitSpeed","navigation.guidanceStatus"};
        return names[i];
    }
    template<unsigned I> bool receive(const bmw::BasicString<unsigned short>& input){
        char text[512];unsigned n=0;const unsigned short* p=input.c_str();
        if(p)for(;n+1<sizeof text&&p[n];++n){
            if(p[n]>127)break;
            text[n]=static_cast<char>(p[n]);
        }
        text[n]=0;
        double a=0,b=0;bool complete=p&&!p[n];
        bool ok=complete&&I<3&&cds_decode(I,text,a,b);
        CdsNumbers numbers;
        if(I>=3)ok=complete&&cds_numbers(text,I==4?"acceleratorPedal":I==6?"fuel":I==8?"units":0,numbers);
        pthread_mutex_lock(&mutex_);
        if(active_){
            uint64_t now=bd_mono_ms();
            if(I<3){
                valid_[I]=ok;times_[I]=now;
                if(ok){values_[I]=a;if(I==2)values_[3]=b;}
                if(I==2){valid_[3]=ok;times_[3]=now;}
            }else if(I==8){
                const char* keys[]={"temperature","fuel"};
                for(unsigned k=0;k<2;++k){
                    unsigned slot=10+k;double v;
                    valid_[slot]=ok&&numbers.find(keys[k],v)&&v>=1&&v<=(k==0?2:3)&&floor(v)==v;
                    if(valid_[slot]){values_[slot]=v;times_[slot]=now;}
                }
            }else{
                unsigned slot=I==10?12:I==9?9:I+1;
                const char* key=I==3?"torque":I==4?"position":I==5?"gear":I==6?"tanklevel":I==7?"temperatureExterior":I==9?"unitSpeed":"guidanceStatus";
                double v=0;
                bool found=ok&&numbers.find(key,v);
                double lo=I==7?-50:0,hi=I==3?1500:I==4?100:I==5?11:I==6?100:I==7?80:I==10?254:2;
                bool valid=found&&v>=lo&&v<=hi;
                if(I==5)valid=valid&&v>=1&&floor(v)==v;
                if(I==9)valid=valid&&v>=1&&floor(v)==v;
                valid_[slot]=valid;times_[slot]=now;if(valid)values_[slot]=v;
            }
        }        pthread_mutex_unlock(&mutex_);return true;
    }
    Callback callback(unsigned i){
        Callback d;d.m_obj=this;
        if(i==0)d.m_exec=&Callback::execImpl<CdsTelemetry,&CdsTelemetry::receive<0> >;
        else if(i==1)d.m_exec=&Callback::execImpl<CdsTelemetry,&CdsTelemetry::receive<1> >;
        else if(i==2)d.m_exec=&Callback::execImpl<CdsTelemetry,&CdsTelemetry::receive<2> >;
        else if(i==3)d.m_exec=&Callback::execImpl<CdsTelemetry,&CdsTelemetry::receive<3> >;
        else if(i==4)d.m_exec=&Callback::execImpl<CdsTelemetry,&CdsTelemetry::receive<4> >;
        else if(i==5)d.m_exec=&Callback::execImpl<CdsTelemetry,&CdsTelemetry::receive<5> >;
        else if(i==6)d.m_exec=&Callback::execImpl<CdsTelemetry,&CdsTelemetry::receive<6> >;
        else if(i==7)d.m_exec=&Callback::execImpl<CdsTelemetry,&CdsTelemetry::receive<7> >;
        else if(i==8)d.m_exec=&Callback::execImpl<CdsTelemetry,&CdsTelemetry::receive<8> >;
        else if(i==9)d.m_exec=&Callback::execImpl<CdsTelemetry,&CdsTelemetry::receive<9> >;
        else d.m_exec=&Callback::execImpl<CdsTelemetry,&CdsTelemetry::receive<10> >;
        return d;
    }
public:
    explicit CdsTelemetry(bmw::PluginApp& app):app_(app),ready_(false),active_(false),mask_(0),request_at_(0),since_(0){
        memset(values_,0,sizeof values_);memset(times_,0,sizeof times_);memset(valid_,0,sizeof valid_);
        ready_=pthread_mutex_init(&mutex_,0)==0;
    }
    ~CdsTelemetry(){stop();if(ready_)pthread_mutex_destroy(&mutex_);}
    bool ready()const{return ready_;}
    void stop(){
        if(!ready_)return;
        pthread_mutex_lock(&mutex_);bool was=active_;active_=false;memset(valid_,0,sizeof valid_);pthread_mutex_unlock(&mutex_);
        if(was)for(unsigned i=0;i<11;++i)if(mask_&(1u<<i)){
            bmw::BasicString<char> p(property(i));app_.cdsRemovePropertyChangedEventHandler(p);
        }
        mask_=0;
    }
    void start(unsigned mask){
        stop();if(!ready_)return;
        mask_=mask;since_=bd_mono_ms();request_at_=since_+1000;
        pthread_mutex_lock(&mutex_);active_=true;pthread_mutex_unlock(&mutex_);
    }
    // One second delay after screen activation, like the verified probe.
    // Periodic asynchronous reads renew constant values without inventing age.
    void tick(uint64_t now){
        if(!active_||now<request_at_)return;
        bool first=request_at_==since_+1000;
        request_at_=now+1000;
        for(unsigned i=0;i<11;++i)if(mask_&(1u<<i)){
            bmw::BasicString<char> p(property(i));Callback d=callback(i);
            if(first)app_.cdsAddPropertyChangedEventHandler(p,100u,d);
            app_.cdsGetPropertyAsync(p,d);
        }
    }
    bool get(unsigned i,uint64_t now,double& value){
        if(!ready_||i>=13)return false;
        unsigned channel=i;
        pthread_mutex_lock(&mutex_);
        bool ok=active_&&valid_[channel]&&now>=times_[channel]&&now-times_[channel]<3500;
        if(ok)value=values_[i];
        pthread_mutex_unlock(&mutex_);return ok;
    }
private:
    CdsTelemetry(const CdsTelemetry&);
    CdsTelemetry& operator=(const CdsTelemetry&);
};
#endif
