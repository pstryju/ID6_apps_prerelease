/* Evo Y uses its validated nine-channel protocol and CDS layout. */
#if defined(BD_NATIVE_CDS) || defined(BD_STOCK_DME)
#include "evo_y_plugin.inc"
#else
// Native NBT EVO RA plugin. Models are touched only on the HMI dispatcher.
// Network diagnostics run in the separate collector process, never in HmiMain3D.
#include "plugin_factory.h"
#include "gauge_core.h"
#include "dial_scales.h"
#include "diagnostic_worker.h"
#include "diagnostic_gateway.h"
#include "gauge_preferences.h"
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
const char kCollector[] = "/var/opt/hmi/data/ID5/Plugins/BoostDeck/boostdeck-collector-qnx";
const char kPreferences[] = "/var/opt/hmi/data/ID5/BoostDeck.prefs";
const unsigned kBarCount = 9;
const unsigned kVerticalSlot = 8;
const unsigned kVerticalFirst = 56100;
const unsigned kVerticalSteps = 20;
const char kAssignments[] = "/var/opt/hmi/data/ID5/BoostDeck.signals";

}

class BoostDeck : public bmw::PluginApp {
    int fd_;
    unsigned poll_generation_;
    bd_frame frame_;
    uint64_t received_;
    pid_t collector_;
    bool stopping_;
    bool gauges_visible_;
    unsigned color_;
    bool metric_;
    uint64_t retry_at_;
    unsigned char signals_[2][kBarCount];
    bool diesel_;
    unsigned editing_;
    void loadAssignments();
    void updateAssignments();
    bool saveAssignments();
    void assignmentsChanged();
    void startVisibleCollector();
    void selectColor(unsigned color);
    char labels_[kBarCount][128];
    unsigned images_[kBarCount];
    void postPoll();
public:
    BoostDeck(bmw::IDispatcher& d, bmw::BasicString<char> name, unsigned maj, unsigned mn,
              bmw::RHMIInterfaceVersion rhmi, const bmw::Bundle& cfg, const bmw::Bundle& res)
        : bmw::PluginApp(d,name,maj,mn,rhmi,cfg,res), fd_(-1), poll_generation_(0),
          received_(0), collector_(-1), stopping_(false), gauges_visible_(false),
          color_(0),
          metric_(false),
          retry_at_(0), diesel_(false), editing_(kBarCount) {
        resetDisplay();
    }
    virtual ~BoostDeck() { shutdown(); }
    virtual bool onInit();
    virtual bool onFinalize() { shutdown(); return true; }
    virtual bool invokeDataUpdate(unsigned key, const bmw::Parcel& data);
    bool gaugeVisibility(const bmw::Parcel&);
    template <unsigned Color> bool changeColor(const bmw::Parcel&) {
        saveGaugePreferences(kPreferences, Color, metric_);
        selectColor(Color);
        return true;
    }
    template <bool Metric> bool changeUnits(const bmw::Parcel&) {
        metric_=Metric;
        saveGaugePreferences(kPreferences, color_, metric_);
        refresh();
        return true;
    }
    template <unsigned Slot> bool editGauge(const bmw::Parcel& event) {
        bool show;
        if (event.readBool(show)) {
            if (show) editing_=Slot;
            else if (editing_==Slot) editing_=kBarCount;
        }
        return true;
    }
    template <unsigned Signal> bool changeGauge(const bmw::Parcel&) {
        if (editing_<kBarCount && signals_[diesel_][editing_]!=Signal) {
            unsigned char& selected=signals_[diesel_][editing_];
            const unsigned char before=selected;
            selected=Signal;
            if (saveAssignments()) assignmentsChanged();
            else {
                selected=before;
                updateModel(340u+editing_,bmw::BasicString<char>("Could not save gauge selection"));
            }
        }
        return true;
    }
    template <bool Diesel> bool changeProfile(const bmw::Parcel&) {
        if (diesel_!=Diesel) {
            diesel_=Diesel;
            if (saveAssignments()) assignmentsChanged();
            else {
                diesel_=!Diesel;
                updateModel(360u,bmw::BasicString<char>("Could not save engine profile"));
            }
        }
        return true;
    }
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
    loadAssignments();
    struct sockaddr_in a;
    fd_=socket(AF_INET,SOCK_DGRAM,0);
    memset(&a,0,sizeof a); a.sin_family=AF_INET; a.sin_port=htons(BD_UDP_PORT);
    a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if (fd_>=0 && (fcntl(fd_,F_SETFL,O_NONBLOCK)<0 || fcntl(fd_,F_SETFD,FD_CLOEXEC)<0 ||
        bind(fd_,reinterpret_cast<struct sockaddr*>(&a),sizeof a)<0)) {
        close(fd_); fd_=-1;
    }
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
    action.m_exec=&Action::execImpl<BoostDeck,&BoostDeck::changeUnits<false> >;
    addHandler(49u,action);
    action.m_exec=&Action::execImpl<BoostDeck,&BoostDeck::changeUnits<true> >;
    addHandler(59u,action);
    action.m_exec=&Action::execImpl<BoostDeck,&BoostDeck::changeProfile<false> >;
    addHandler(502u,action);
    action.m_exec=&Action::execImpl<BoostDeck,&BoostDeck::changeProfile<true> >;
    addHandler(503u,action);
    static const Handler editors[] = {
#define EDIT(slot) &Action::execImpl<BoostDeck,&BoostDeck::editGauge<slot> >
        EDIT(0), EDIT(1), EDIT(2), EDIT(3), EDIT(4), EDIT(5),
        EDIT(6), EDIT(7), EDIT(8)
#undef EDIT
    };
    for (unsigned i=0;i<kBarCount;++i) {
        action.m_exec=editors[i];
        addHandler(510u+i,11u,action);
    }
    static const Handler changes[] = {
#define SIGNAL(id, name, lo, hi, unit) &Action::execImpl<BoostDeck,&BoostDeck::changeGauge<BD_##id> >,
#include "boost_signals.def"
#undef SIGNAL
    };
    for (unsigned i=0;i<BD_COUNT;++i) {
        action.m_exec=changes[i];
        addHandler(600u+i,action);
    }
    loadGaugePreferences(kPreferences, sizeof kColorNames / sizeof kColorNames[0], color_, metric_);
    selectColor(color_);
    updateAssignments();
    if (fd_>=0) {
        ++poll_generation_;
        postPoll();
    }
    return true;
}

bool BoostDeck::gaugeVisibility(const bmw::Parcel& event) {
    bool visible;
    if (!event.readBool(visible) || gauges_visible_==visible) return true;
    gauges_visible_=visible;
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
    if (!gauges_visible_ || collector_>0 || fd_<0 ||
        bd_mono_ms()<retry_at_) return;
    const DiagnosticGateway gateway("/var/opt/hmi/data/ID5/Plugins/" "BoostDeck" "/gateway-ip.txt");
    unsigned mask=0;
    for (unsigned i=0;i<kBarCount;++i) mask |= 1u << signals_[diesel_][i];
    char selected[16];
    snprintf(selected,sizeof selected,"%u",mask);
    const char *args[]={kCollector,"--host",gateway.host(),"--session","--signals",selected,
                       0,0,0};
    unsigned next=6;
    if (mask & (1u<<BD_ETHANOL)) args[next++]="--ethanol";
    if (diesel_) args[next]="--diesel";
    collector_=launch(args,-1);
    retry_at_=bd_mono_ms()+2000;
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
    if (fd_<0 || generation!=poll_generation_) return true;
    poll();
    if (fd_>=0 && generation==poll_generation_) postPoll();
    return true;
}

void BoostDeck::poll() {
    reapCollector();
    startVisibleCollector();
    // A bounded non-blocking drain: at most 8 * 512 bytes per UI tick.
    char data[BD_MAX_PACKET];
    for (unsigned n=0;fd_>=0 && n<8;++n) {
        ssize_t k=recv(fd_,data,sizeof data,0);
        if (k<=0) break;
        bd_frame next;
        if (gauges_visible_ && !stopping_ && bd_parse(data,static_cast<size_t>(k),&next) &&
            next.sent_ms && next.sent_ms<=bd_mono_ms() &&
            (!received_ || next.sent_ms>=received_)) {
            frame_=next; received_=next.sent_ms; // queued UDP does not reset sample age
        }
    }
    refresh();
}

void BoostDeck::loadAssignments() {
    static const unsigned char dieselDefaults[kBarCount]={
        BD_BOOST, BD_LOAD, BD_RPM, BD_IAT, BD_TRANS, BD_RAIL,
        BD_MAF, BD_COOLANT, BD_MAP
    };
    diesel_=false;
    for (unsigned i=0;i<kBarCount;++i) signals_[0][i]=static_cast<unsigned char>(i);
    memcpy(signals_[1],dieselDefaults,sizeof dieselDefaults);
    FILE* file=fopen(kAssignments,"rb");
    if (!file) return;
    const unsigned previousSlots=11;
    unsigned char data[3+2*previousSlots];
    const size_t length=fread(data,1,sizeof data,file);
    const bool complete=fgetc(file)==EOF && !ferror(file);
    fclose(file);
    if (!complete || length<3 || data[0]!='B' || data[2]>1) return;
    unsigned stride=0;
    if (data[1]==2 && length==3+sizeof signals_) stride=kBarCount;
    if (data[1]==1 && length==3+2*previousSlots) stride=previousSlots;
    if (!stride) return;
    for (unsigned profile=0;profile<2;++profile)
        for (unsigned i=0;i<kBarCount;++i)
            if (data[3+profile*stride+i]>=BD_COUNT) return;
    diesel_=data[2]!=0;
    for (unsigned profile=0;profile<2;++profile)
        memcpy(signals_[profile],data+3+profile*stride,kBarCount);
}

void BoostDeck::updateAssignments() {
    for (unsigned i=0;i<kBarCount;++i) {
        char name[64];
        snprintf(name,sizeof name,"Selected: %s",bd_name(signals_[diesel_][i]));
        updateModel(340u+i,bmw::BasicString<char>(name));
        const unsigned signal=signals_[diesel_][i];
        const char* caption=signal==BD_ETHANOL && i==kVerticalSlot ? "Active E" :
                            signal==BD_TRANS ? "Transmission" : bd_name(signal);
        updateModel(370u+i,bmw::BasicString<char>(caption));
    }
    updateModel(360u,bmw::BasicString<char>(diesel_ ? "Selected: Diesel" : "Selected: Gasoline"));
    refresh();
}

bool BoostDeck::saveAssignments() {
    unsigned char data[3+sizeof signals_]={'B',2,static_cast<unsigned char>(diesel_)};
    memcpy(data+3,signals_,sizeof signals_);
    return savePersistentBytes(kAssignments,data,sizeof data);
}

void BoostDeck::assignmentsChanged() {
    stopCollector();
    resetDisplay();
    retry_at_=0;
    updateAssignments();
}

void BoostDeck::refresh() {
    uint64_t elapsed=received_ ? bd_mono_ms()-received_ : static_cast<uint64_t>(BD_STALE_MS);
    for (unsigned i=0;i<kBarCount;++i) {
        const unsigned signal=signals_[diesel_][i];
        char label[128];
        const bool vertical=i==kVerticalSlot;
        const unsigned steps=vertical ? kVerticalSteps : kDialSteps;
        unsigned image=vertical ? kVerticalFirst : kBarFirst;
        if (received_ && bd_is_fresh(&frame_,signal,elapsed)) {
            const double value=frame_.value[signal];
            const char *unit=bd_unit(signal,metric_);
            double reading=bd_display_value(signal,value,metric_);
            if (signal==BD_BOOST && fabs(reading)<0.05) reading=0;
            const int decimals=signal==BD_BATTERY || signal==BD_LAMBDA_TARGET ? 2 :
                signal==BD_RPM || signal==BD_RAIL || signal==BD_ETHANOL ? 0 : 1;
            snprintf(label,sizeof label,"%.*f%s%s",decimals,reading,
                     *unit && *unit!='%' ? " " : "",unit);
            const unsigned step=(bd_bar(signal,value)*steps+500)/1000;
            if (step) {
                const unsigned palette=!vertical && color_==kMulticolor ? kMulticolor+i : color_;
                image+=palette*steps+step;
            }
        } else {
            strcpy(label,"--");
        }
        if (image!=images_[i]) {
            updateModel(320u+i,bmw::ImageID(image));
            images_[i]=image;
        }
        if (strcmp(label,labels_[i])) {
            updateModel(300u+i,bmw::BasicString<char>(label));
            strcpy(labels_[i],label);
        }
    }
}

extern "C" {
PluginAppInfo pluginAppInfo = {1, "BoostDeck", "owner-development", 1,
                              create_app<BoostDeck>, destroy_app};
}

#endif /* Evo Y / upstream implementation */
