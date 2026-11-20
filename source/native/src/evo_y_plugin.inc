// Native NBT EVO RA plugin. Models are touched only on the HMI dispatcher.
// Network diagnostics run in the separate collector process, never in HmiMain3D.
#include "plugin_factory.h"
#include "gauge_core.h"
#include "dial_scales.h"
#include "diagnostic_worker.h"
#include "gateway_config.h"
#ifdef BD_NATIVE_CDS
#include "cds_telemetry.h"
#endif
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#if defined(__arm__)
// C++98 target ABI gates. Host builds have 64-bit pointers and cannot prove these.
typedef char bd_pointer_size[(sizeof(void*)==4) ? 1 : -1];
typedef char bd_parcel_size[(sizeof(bmw::Parcel)==20) ? 1 : -1];
typedef char bd_message_size[(sizeof(bmw::Message)==80 && __alignof__(bmw::Message)==8) ? 1 : -1];
typedef char bd_std_string_size[(sizeof(std::string)==4) ? 1 : -1];
typedef char bd_delegate_size[(sizeof(bmw::Delegate<bool,const bmw::Parcel&>)==8) ? 1 : -1];
#endif

namespace {
// In the installed AppFramework, this message reads a UInt32 key from its
// Parcel and dispatches through the primary invokeDataUpdate virtual.
const unsigned kDataUpdateMessage = 0x40000003u;
const unsigned kPollKey = 0x4244504cu; // BDPL, private to this app
const unsigned kActivePollIntervalMs = 25u;
const unsigned kIdlePollIntervalMs = 250u;
#ifdef BD_BOOST_MOST
const char kCollector[] = "/var/opt/hmi/data/ID5/Plugins/BoostDeck/boostdeck-most-qnx";
#else
const char kCollector[] = "/var/opt/hmi/data/ID5/Plugins/BoostDeck/boostdeck-collector-qnx";
#endif
const unsigned kBarCount = sizeof(kDialScales)/sizeof(kDialScales[0]);
#ifdef PERF_LAB
typedef char dial_count_check[kBarCount + 1 == BD_COUNT ? 1 : -1];
const unsigned kEthanolFirst = 56100u;
const unsigned kEthanolSteps = 20u;
#else
typedef char dial_count_check[kBarCount == BD_COUNT ? 1 : -1];
#endif
}

class BoostDeck : public bmw::PluginApp {
#ifdef BD_NATIVE_CDS
    CdsTelemetry cds_;
#endif
    int fd_;
    unsigned poll_generation_;
    bd_frame frame_;
    uint64_t received_;
    uint64_t visible_since_;
    pid_t collector_;
    bool stopping_;
    bool gauges_visible_;
    unsigned color_;
#ifdef PERF_LAB
    bool metric_;
#endif
    uint64_t retry_at_;
    void startVisibleCollector();
    void selectColor(unsigned color);
    char labels_[BD_COUNT][128];
    unsigned images_[BD_COUNT];
    void postPoll();
public:
    BoostDeck(bmw::IDispatcher& d, bmw::BasicString<char> name, unsigned maj, unsigned mn,
              bmw::RHMIInterfaceVersion rhmi, const bmw::Bundle& cfg, const bmw::Bundle& res)
        : bmw::PluginApp(d,name,maj,mn,rhmi,cfg,res),
#ifdef BD_NATIVE_CDS
          cds_(*this),
#endif
          fd_(-1), poll_generation_(0),
          received_(0), visible_since_(0), collector_(-1), stopping_(false), gauges_visible_(false),
          color_(0),
#ifdef PERF_LAB
          metric_(false),
#endif
          retry_at_(0) {
        resetDisplay();
    }
    virtual ~BoostDeck() { shutdown(); }
    virtual bool onInit();
    virtual bool onFinalize() { shutdown(); return true; }
    virtual bool invokeDataUpdate(unsigned key, const bmw::Parcel& data);
    bool gaugeVisibility(const bmw::Parcel&);
    template <unsigned Color> bool changeColor(const bmw::Parcel&) {
        selectColor(Color);
        return true;
    }
#ifdef PERF_LAB
    template <bool Metric> bool changeUnits(const bmw::Parcel&) {
        metric_=Metric;
        refresh();
        return true;
    }
#endif
    void reapCollector() {
        if (collector_ <= 0) return;
        pid_t ended = waitpid(collector_, 0, WNOHANG);
        if (ended == collector_ || (ended < 0 && errno == ECHILD)) {
            collector_ = -1;
            stopping_ = false;
        }
    }
    void stopCollector() {
        reapCollector();
        if (collector_ > 0 && !stopping_) {
            kill(collector_, SIGTERM);
            stopping_ = true;
        }
    }
    void shutdown() {
        gauges_visible_=false;
#ifdef BD_NATIVE_CDS
        cds_.stop();
#endif
        stopCollector();
        if (fd_>=0) { close(fd_); fd_=-1; }
    }
    void resetDisplay() {
        memset(&frame_,0,sizeof frame_); memset(labels_,0,sizeof labels_);
        memset(images_,0,sizeof images_);
        received_=0;
    }
    void refresh();
    void poll();
};

bool BoostDeck::onInit() {
    shutdown();
    resetDisplay();
#ifdef BD_NATIVE_CDS
    if(!cds_.ready())return false;
#endif
#if !defined(BD_NATIVE_CDS) || defined(BD_BOOST_MOST)
    struct sockaddr_in a;
    fd_=socket(AF_INET,SOCK_DGRAM,0);
    memset(&a,0,sizeof a); a.sin_family=AF_INET; a.sin_port=htons(BD_UDP_PORT);
    a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if (fd_>=0 && (fcntl(fd_,F_SETFL,O_NONBLOCK)<0 || fcntl(fd_,F_SETFD,FD_CLOEXEC)<0 ||
        bind(fd_,reinterpret_cast<struct sockaddr*>(&a),sizeof a)<0)) {
        close(fd_); fd_=-1;
    }
#endif
    bmw::Delegate<bool,const bmw::Parcel&> action;
    action.m_obj=this;
    // BMW sends event 11 with a bool when this HMI state becomes visible or hidden.
    action.m_exec=&bmw::Delegate<bool,const bmw::Parcel&>::execImpl<BoostDeck,&BoostDeck::gaugeVisibility>;
    addHandler(100u,11u,action);
    typedef bmw::Delegate<bool,const bmw::Parcel&> Action;
    typedef bool (*Handler)(void*,const bmw::Parcel&);
    static const Handler colorHandlers[] = {
        &Action::execImpl<BoostDeck,&BoostDeck::changeColor<0> >,
        &Action::execImpl<BoostDeck,&BoostDeck::changeColor<1> >,
        &Action::execImpl<BoostDeck,&BoostDeck::changeColor<2> >,
        &Action::execImpl<BoostDeck,&BoostDeck::changeColor<3> >,
        &Action::execImpl<BoostDeck,&BoostDeck::changeColor<4> >,
        &Action::execImpl<BoostDeck,&BoostDeck::changeColor<5> >,
        &Action::execImpl<BoostDeck,&BoostDeck::changeColor<6> >,
        &Action::execImpl<BoostDeck,&BoostDeck::changeColor<7> >,
        &Action::execImpl<BoostDeck,&BoostDeck::changeColor<kMulticolor> >
    };
    typedef char color_handler_count[(sizeof colorHandlers/sizeof colorHandlers[0] ==
                                      sizeof kColorNames/sizeof kColorNames[0]) ? 1 : -1];
    (void)sizeof(color_handler_count);
    for (unsigned i=0;i<sizeof colorHandlers/sizeof colorHandlers[0];++i) {
        action.m_exec=colorHandlers[i];
        addHandler(40u+i,action);
    }
#if defined(PERF_LAB) && !defined(BD_NATIVE_CDS)
    action.m_exec=&Action::execImpl<BoostDeck,&BoostDeck::changeUnits<false> >;
    addHandler(49u,action);
    action.m_exec=&Action::execImpl<BoostDeck,&BoostDeck::changeUnits<true> >;
    addHandler(59u,action);
#endif
    selectColor(color_);
    bmw::Parcel visible(true);
    for (unsigned i=0;i<kBarCount;++i) setProperty(200u+i,3u,visible);
    refresh();
#ifdef BD_NATIVE_CDS
    if(cds_.ready()) {
#else
    if (fd_>=0) {
#endif
        ++poll_generation_;
        postPoll();
    }
    return true;
}

bool BoostDeck::gaugeVisibility(const bmw::Parcel& event) {
    bool visible;
    if (!event.readBool(visible) || gauges_visible_==visible) return true;
    gauges_visible_=visible;
    visible_since_=visible?bd_mono_ms():0;
#ifdef BD_NATIVE_CDS
#ifdef BD_BOOST_MOST
    if(visible)cds_.start(261u);else cds_.stop(); // RPM, temperatures, Evo units
#else
    if(visible)cds_.start(1023u);else cds_.stop();
#endif
#endif
    if (visible) {
        retry_at_=0;
        startVisibleCollector();
    } else {
        stopCollector();
        received_=0;
        memset(&frame_,0,sizeof frame_);
        refresh();
    }
    return true;
}

void BoostDeck::startVisibleCollector() {
#if !defined(BD_NATIVE_CDS) || defined(BD_BOOST_MOST)
    if (!gauges_visible_ || collector_>0 || fd_<0 ||
        bd_mono_ms()<retry_at_) return;
#ifdef BD_BOOST_MOST
    const char *args[]={kCollector,"--session",0};
#else
    char gateway[16];
    if (!bd_load_gateway(kCollector,gateway)) { retry_at_=bd_mono_ms()+2000; return; }
    const char *args[]={kCollector,"--host",gateway,"--session",
#if defined(PERF_LAB) && !defined(BD_STOCK_DME)
                       "--ethanol",
#endif
                       0};
#endif
    collector_=launch(args,-1);
    retry_at_=bd_mono_ms()+2000;
#endif
}

void BoostDeck::selectColor(unsigned color) {
    color_=color;
    char label[48];
    snprintf(label,sizeof label,"Selected: %s",kColorNames[color_]);
    updateModel(110u,bmw::BasicString<char>(label));
    refresh();
}

void BoostDeck::postPoll() {
    // The framework copies the Message and Parcel into its own queue. Entries
    // carry only data, so clearing the queue needs no code from this module.
    bmw::Parcel data;
    data.writeUInt32(kPollKey);
    data.writeUInt32(poll_generation_);
    bmw::Message message(kDataUpdateMessage, data);
    post(message, collector_ > 0 ? kActivePollIntervalMs : kIdlePollIntervalMs);
}

bool BoostDeck::invokeDataUpdate(unsigned key, const bmw::Parcel& data) {
    if (key!=kPollKey) return bmw::PluginApp::invokeDataUpdate(key,data);
    unsigned generation;
    if (!data.readUInt32(generation)) return false;
#ifdef BD_NATIVE_CDS
    if(generation!=poll_generation_)return true;
#else
    if (fd_<0 || generation!=poll_generation_) return true;
#endif
    poll();
#ifdef BD_NATIVE_CDS
    if(generation==poll_generation_)postPoll();
#else
    if (fd_>=0 && generation==poll_generation_) postPoll();
#endif
    return true;
}

void BoostDeck::poll() {
#ifdef BD_NATIVE_CDS
    cds_.tick(bd_mono_ms());
#endif
#if !defined(BD_NATIVE_CDS) || defined(BD_BOOST_MOST)
    reapCollector();
    startVisibleCollector();
    // A bounded non-blocking drain: at most 8 * 512 bytes per UI tick.
    char data[BD_MAX_PACKET];
    for (unsigned n=0;fd_>=0 && n<8;++n) {
        ssize_t k=recv(fd_,data,sizeof data,0);
        if (k<=0) break;
        bd_frame next;
        if (gauges_visible_ && !stopping_ && bd_parse(data,static_cast<size_t>(k),&next) &&
            next.sent_ms && next.sent_ms>=visible_since_ && next.sent_ms<=bd_mono_ms() &&
            (!received_ || next.sent_ms>=received_)) {
            frame_=next; received_=next.sent_ms; // queued UDP does not reset sample age
        }
    }
#endif
    refresh();
}

void BoostDeck::refresh() {
#ifdef BD_BOOST_MOST
    uint64_t now=bd_mono_ms();
    uint64_t elapsed=received_&&now>=received_?now-received_:static_cast<uint64_t>(BD_STALE_MS);
    double unit=0;bool units=cds_.get(10,now,unit);
    for(unsigned i=0;i<9;++i){
        double raw=i<BD_COUNT?frame_.value[i]:0,reading=0;char label[128];const char* suffix="";
        bool valid=gauges_visible_&&received_&&bd_is_fresh(&frame_,i,elapsed);
        double lo=i<8?kDialScales[i].minimum:0,hi=i<8?kDialScales[i].maximum:150;
        if(i==BD_TRANS){valid=gauges_visible_&&cds_.get(0,now,raw);lo=0;hi=8000;suffix=" RPM";}
        if(i==BD_COOLANT||i==BD_OIL_TEMP){valid=gauges_visible_&&cds_.get(i==BD_COOLANT?2:3,now,raw);}
        reading=raw;
        if(i==BD_BOOST){reading=raw/14.5037738;suffix=" bar";if(fabs(reading)<.005)reading=0;}
        else if(i==BD_RAIL||i==BD_OIL)suffix=" bar";
        else if(i==BD_TIMING)suffix=" deg";
        else if(i==BD_IAT||i==BD_COOLANT||i==BD_OIL_TEMP){
            valid=valid&&units;reading=cds_temperature(raw,static_cast<unsigned>(unit));suffix=unit==2?"\260F":"\260C";
        }
        if(!valid)strcpy(label,"--");
        else snprintf(label,sizeof label,i==BD_BOOST||i==BD_OIL?"%.2f%s":i==BD_TRANS||i==BD_COOLANT||i==BD_OIL_TEMP?"%.0f%s":"%.1f%s",reading,suffix);
        double fraction=(raw-lo)/(hi-lo);if(!valid||fraction<0)fraction=0;if(fraction>1)fraction=1;
        unsigned step=static_cast<unsigned>(fraction*(i==8?kEthanolSteps:kDialSteps)+.5);
        unsigned palette=color_==kMulticolor?kMulticolor+i:color_;
        unsigned image=i==8?kEthanolFirst+(step?color_*kEthanolSteps+step:0):kBarFirst+(step?palette*kDialSteps+step:0);
        if(images_[i]!=image){images_[i]=image;updateModel(60u+i,bmw::ImageID(image));}
        if(strcmp(labels_[i],label)){strcpy(labels_[i],label);updateModel(50u+i,bmw::BasicString<char>(label));}
    }
#elif defined(BD_NATIVE_CDS)
    uint64_t now=bd_mono_ms();cds_.tick(now);
    const unsigned slots[]={0,1,4,5,6,7,8,2,3};
    const double lows[]={0,0,0,0,1,0,-40,0,0};
    const double highs[]={8000,260,500,100,11,60,50,130,150};
    for(unsigned i=0;i<9;++i){
        double raw=0,reading=0,unit=0;const char* suffix="";char label[128];
        bool valid=gauges_visible_&&cds_.get(slots[i],now,raw);reading=raw;
        if(i==1){
            valid=valid&&cds_.get(9,now,unit);
            reading=cds_speed(raw,static_cast<unsigned>(unit));suffix=unit==2?" mph":" km/h";
        }else if(i==5){
            valid=valid&&cds_.get(11,now,unit);
            reading=cds_fuel(raw,static_cast<unsigned>(unit));
            suffix=unit==2?" gal UK":unit==3?" gal US":" L";
        }else if(i>=6){
            valid=valid&&cds_.get(10,now,unit);
            reading=cds_temperature(raw,static_cast<unsigned>(unit));suffix=unit==2?"\260F":"\260C";
        }else if(i==0)suffix=" RPM";
        else if(i==2)suffix=" Nm"; // CDS torque canonical unit; no guessed sportTorque enum.
        else if(i==3)suffix=" %";
        if(!valid)strcpy(label,"--");
        else if(i==4){
            unsigned gear=static_cast<unsigned>(raw);
            if(gear<=3)strcpy(label,gear==1?"N":gear==2?"R":"P");
            else snprintf(label,sizeof label,"D%u",gear-3);
        }else snprintf(label,sizeof label,i==5||i==6?"%.1f%s":"%.0f%s",reading,suffix);
        double fraction=(raw-lows[i])/(highs[i]-lows[i]);
        if(!valid||fraction<0)fraction=0;if(fraction>1)fraction=1;
        unsigned step=static_cast<unsigned>(fraction*(i==8?kEthanolSteps:kDialSteps)+.5);
        unsigned palette=color_==kMulticolor?kMulticolor+i:color_;
        unsigned image=i==8?kEthanolFirst+(step?color_*kEthanolSteps+step:0):
            kBarFirst+(step?palette*kDialSteps+step:0);
        if(image!=images_[i]){updateModel(60u+i,bmw::ImageID(image));images_[i]=image;}
        if(strcmp(label,labels_[i])){updateModel(50u+i,bmw::BasicString<char>(label));strcpy(labels_[i],label);}
    }
#else
    uint64_t elapsed=received_ ? bd_mono_ms()-received_ : static_cast<uint64_t>(BD_STALE_MS);
    for (unsigned i=0;i<kBarCount;++i) {
        char label[128];
        const DialScale& scale = kDialScales[i];
        unsigned image = kBarFirst;
        bool valid=received_ && bd_is_fresh(&frame_,i,elapsed);
        double x=frame_.value[i];
#ifdef BD_NATIVE_CDS
        if(i==BD_COOLANT)valid=cds_.get(2,bd_mono_ms(),x);
#endif
        if (valid) {
            const char *unit_space = *scale.unit ? " " : "";
            double reading = bd_display_value(i,x,0);
            if (i==BD_BOOST && fabs(reading)<0.05) reading=0;
#ifdef PERF_LAB
            if (metric_) {
                const char* units=i==BD_AFR ? "" : i==BD_TIMING ? "deg" :
                    i==BD_IAT || i==BD_TRANS || i==BD_COOLANT ? "\260C" : "bar";
                snprintf(label,sizeof label,"%.1f%s%s",bd_display_value(i,x,1),*units ? " " : "",units);
            } else snprintf(label,sizeof label,i==BD_RAIL ? "%.0f%s%s" : "%.1f%s%s",reading,unit_space,scale.unit);
#else
            const char *prefix = frame_.demo && i==BD_BOOST ? "Demo " : "";
            const char *format = i==BD_BOOST ? "%s%s   %+.1f%s%s"
                                : i==BD_RAIL ? "%s%s   %.0f%s%s"
                                : "%s%s   %.1f%s%s";
            snprintf(label,sizeof label,format,prefix,scale.name,reading,unit_space,scale.unit);
#endif
            double fraction=(x-scale.minimum)/(scale.maximum-scale.minimum);
            if (fraction<0) fraction=0;
            if (fraction>1) fraction=1;
            unsigned step=static_cast<unsigned>(fraction*kDialSteps+.5);
            if (step) {
                const unsigned palette = color_ == kMulticolor ? kMulticolor + i : color_;
                image += palette*kDialSteps+step;
            }
        } else {
#ifdef PERF_LAB
            strcpy(label,"--");
#else
            snprintf(label,sizeof label,"%s   --",scale.name);
#endif
        }
        if (image!=images_[i]) {
            updateModel(60u+i,bmw::ImageID(image));
            images_[i]=image;
        }
        if (strcmp(label,labels_[i])) {
            updateModel(50u+i,bmw::BasicString<char>(label));
            strcpy(labels_[i],label);
        }
    }
#ifdef PERF_LAB
#ifdef BD_STOCK_DME
    double oil=frame_.value[BD_OIL_TEMP];
#ifdef BD_NATIVE_CDS
    const bool valid=cds_.get(3,bd_mono_ms(),oil);
#else
    const bool valid = received_ && bd_is_fresh(&frame_,BD_OIL_TEMP,elapsed);
#endif
    char ethanol[128];
    if (valid) snprintf(ethanol,sizeof ethanol,"%.0f %s",bd_display_value(BD_OIL_TEMP,oil,metric_),metric_?"\260C":"\260F");
    else strcpy(ethanol,"--");
    const unsigned step=valid ? static_cast<unsigned>((bd_bar(BD_OIL_TEMP,oil)*kEthanolSteps+500)/1000) : 0;
#else
    const bool valid = received_ && bd_is_fresh(&frame_,BD_ETHANOL,elapsed) &&
        frame_.value[BD_ETHANOL]>=0 && frame_.value[BD_ETHANOL]<=100;
    char ethanol[128];
    if (valid) snprintf(ethanol,sizeof ethanol,"%.0f%%",frame_.value[BD_ETHANOL]);
    else strcpy(ethanol,"--");
    const unsigned step=valid ? static_cast<unsigned>(frame_.value[BD_ETHANOL]*kEthanolSteps/100.+.5) : 0;
#endif
    const unsigned ethanolImage=kEthanolFirst+(step ? color_*kEthanolSteps+step : 0);
    if (images_[BD_ETHANOL]!=ethanolImage) {
        updateModel(68u,bmw::ImageID(ethanolImage));
        images_[BD_ETHANOL]=ethanolImage;
    }
    if (strcmp(labels_[BD_ETHANOL],ethanol)) {
        updateModel(58u,bmw::BasicString<char>(ethanol));
        strcpy(labels_[BD_ETHANOL],ethanol);
    }
#endif
#endif
}

extern "C" {
PluginAppInfo pluginAppInfo = {1, "BoostDeck", "owner-development", 1,
                              create_app<BoostDeck>, destroy_app};
}
