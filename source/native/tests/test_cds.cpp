#include <assert.h>
#include <stdio.h>
#include <string>
#include <map>
#include "gauge_core.h"
static uint64_t now_ms=100;
extern "C" uint64_t bd_mono_ms(void){return now_ms;}
// Minimal host CDS service double: deliver raw JSON to the actual adapter.
namespace bmw {
template<class T> class BasicString {
    std::basic_string<T> data_;
public: explicit BasicString(const T* p):data_(p){} const T* c_str()const{return data_.c_str();}
};
template<class R,class A> struct Delegate {
    void* m_obj;R(*m_exec)(void*,A);
    template<class C,R(C::*M)(A)> static R execImpl(void* p,A a){return (static_cast<C*>(p)->*M)(a);}
};
class PluginApp {
public:
    typedef Delegate<bool,const BasicString<unsigned short>&> D;
    std::map<std::string,D> handlers;
    unsigned adds,removes,gets;
    PluginApp():adds(0),removes(0),gets(0){}
    void cdsAddPropertyChangedEventHandler(BasicString<char>& p,unsigned,const D& d){handlers[p.c_str()]=d;++adds;}
    void cdsRemovePropertyChangedEventHandler(BasicString<char>& p){handlers.erase(p.c_str());++removes;}
    void cdsGetPropertyAsync(BasicString<char>&,const D&){++gets;}
    void emit(const char* key,const char* json){
        std::basic_string<unsigned short> s;for(;*json;++json)s+=static_cast<unsigned short>(*json);
        BasicString<unsigned short> value(s.c_str());D d=handlers[key];assert(d.m_exec);d.m_exec(d.m_obj,value);
    }
};
}
#include "cds_telemetry.h"
int main(){
    assert(fabs(cds_speed(100,2)-62.1371192237)<1e-8);
    assert(cds_speed(100,1)==100&&cds_temperature(0,2)==32&&cds_temperature(100,2)==212);
    assert(fabs(cds_fuel(4.54609,2)-1)<1e-8&&fabs(cds_fuel(3.785411784,3)-1)<1e-8);
    double a=99,b=99;
    assert(cds_decode(0,"{\"RPMSpeed\":703}",a,b)&&a==703);
    assert(cds_decode(1," { \"speedActual\" : 0 } ",a,b)&&a==0);
    assert(cds_decode(2,"{\"temperature\":{\"engine\":78,\"oil\":75}}",a,b)&&a==78&&b==75);
    assert(cds_decode(2,"{\"temperature\":{\"oil\":-10,\"engine\":20}}",a,b)&&a==20&&b==-10);
    const char* bad[]={"{\"error\":400}","{\"RPMSpeed\":65535}","{\"RPMSpeed\":-1}",
        "{\"RPMSpeed\":NaN}","{\"RPMSpeed\":null}","{\"RPMSpeed\":01}",
        "{\"RPMSpeed\":1.}","{\"RPMSpeed\":1e}","{\"RPMSpeed\":1}junk",
        "{\"RPMSpeed\":1,\"RPMSpeed\":2}","{\"RPMSpeed\":1"};
    for(unsigned i=0;i<sizeof bad/sizeof bad[0];++i)assert(!cds_decode(0,bad[i],a,b));
    assert(!cds_decode(2,"{\"temperature\":{\"engine\":78,\"engine\":75}}",a,b));
    assert(!cds_decode(2,"{\"temperature\":{\"engine\":78,\"oil\":255}}",a,b));
    bmw::PluginApp app;CdsTelemetry cds(app);assert(cds.ready());cds.start(7);
    cds.tick(now_ms);assert(app.adds==0);now_ms=1100;cds.tick(now_ms);assert(app.adds==3&&app.gets==3);
    app.emit("engine.RPMSpeed","{\"RPMSpeed\":703}");assert(cds.get(0,now_ms,a)&&a==703);
    app.emit("driving.speedActual","{\"speedActual\":0}");assert(cds.get(1,now_ms,a)&&a==0);
    app.emit("engine.temperature","{\"temperature\":{\"engine\":78,\"oil\":75}}");
    assert(cds.get(2,now_ms,a)&&a==78);assert(cds.get(3,now_ms,a)&&a==75);
    now_ms=2100;cds.tick(now_ms);assert(app.adds==3&&app.gets==6);
    now_ms=4600;assert(!cds.get(0,now_ms,a)&&!cds.get(1,now_ms,a));
    app.emit("driving.speedActual","{\"speedActual\":0}");assert(cds.get(1,now_ms,a)&&a==0);
    app.emit("engine.RPMSpeed","{\"error\":400}");assert(!cds.get(0,now_ms,a));
    cds.stop();assert(app.handlers.empty()&&!cds.get(1,now_ms,a));
    cds.start(3);assert(!cds.get(1,now_ms,a));now_ms+=1000;cds.tick(now_ms);
    assert(app.handlers.size()==2);app.emit("engine.RPMSpeed","{\"RPMSpeed\":800}");
    assert(cds.get(0,now_ms,a)&&a==800);cds.stop();cds.stop();assert(app.removes==5);
    cds.start(1023);now_ms+=1000;cds.tick(now_ms);assert(app.handlers.size()==10);
    app.emit("engine.torque","{\"torque\":250}");assert(cds.get(4,now_ms,a)&&a==250);
    app.emit("driving.acceleratorPedal","{\"acceleratorPedal\":{\"position\":30,\"ecoPosition\":0}}");
    assert(cds.get(5,now_ms,a)&&a==30);
    app.emit("driving.gear","{\"gear\":3}");assert(cds.get(6,now_ms,a)&&a==3);
    app.emit("sensors.fuel","{\"fuel\":{\"range\":522,\"reserve\":0,\"tanklevel\":39}}");
    assert(cds.get(7,now_ms,a)&&a==39);
    app.emit("sensors.temperatureExterior","{\"temperatureExterior\":14.5}");assert(cds.get(8,now_ms,a)&&a==14.5);
    app.emit("vehicle.unitSpeed","{\"unitSpeed\":1}");assert(cds.get(9,now_ms,a)&&a==1);
    app.emit("vehicle.units","{\"units\":{\"temperature\":1,\"fuel\":1,\"sportTorque\":1}}");
    assert(cds.get(10,now_ms,a)&&a==1);assert(cds.get(11,now_ms,a)&&a==1);
    app.emit("vehicle.unitSpeed","{\"unitSpeed\":2}");assert(cds.get(9,now_ms,a)&&a==2);
    app.emit("vehicle.units","{\"units\":{\"temperature\":2,\"fuel\":3}}");
    assert(cds.get(10,now_ms,a)&&a==2);assert(cds.get(11,now_ms,a)&&a==3);
    app.emit("vehicle.units","{\"units\":{\"temperature\":255,\"fuel\":2}}");
    assert(!cds.get(10,now_ms,a));assert(cds.get(11,now_ms,a)&&a==2);
    app.emit("vehicle.units","{\"units\":{\"temperature\":1,\"temperature\":2}}");
    assert(!cds.get(10,now_ms,a)&&!cds.get(11,now_ms,a));
    app.emit("driving.acceleratorPedal","{\"acceleratorPedal\":{\"position\":255}}");assert(!cds.get(5,now_ms,a));
    app.emit("driving.gear","{\"gear\":255}");assert(!cds.get(6,now_ms,a));
    app.emit("sensors.fuel","{\"fuel\":{\"tanklevel\":255}}");assert(!cds.get(7,now_ms,a));
    app.emit("engine.torque","{\"error\":401}");assert(!cds.get(4,now_ms,a));
    now_ms+=3500;assert(!cds.get(8,now_ms,a)&&!cds.get(9,now_ms,a));
    cds.stop();assert(app.handlers.empty());
    cds.start(1025);now_ms+=1000;cds.tick(now_ms);
    app.emit("navigation.guidanceStatus","{\"guidanceStatus\":0}");assert(cds.get(12,now_ms,a)&&a==0);
    app.emit("navigation.guidanceStatus","{\"guidanceStatus\":1}");assert(cds.get(12,now_ms,a)&&a==1);
    app.emit("navigation.guidanceStatus","{\"error\":400}");assert(!cds.get(12,now_ms,a));
    cds.stop();
    puts("CDS: captured JSON, invalid/sentinel data, per-channel expiry, constant refresh and reopen passed.");
}
