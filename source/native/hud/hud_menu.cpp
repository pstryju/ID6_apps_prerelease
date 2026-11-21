// SPDX-License-Identifier: MIT
#include "plugin_factory.h"
#include "diagnostic_worker.h"
#include "diagnostic_gateway.h"
#include <sys/wait.h>
#include <stdlib.h>
#ifdef HUD_CDS_SOURCE
#include "cds_telemetry.h"
#include "hud_feed.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

namespace {
const unsigned kPoll = 0x48554454u;
const char kWorker[] = "/var/opt/hmi/data/ID5/Plugins/HudTest/hud-shape-qnx";
const char kPreference[] = "/var/opt/hmi/data/ID5/Plugins/HudTest/hud-enabled";

bool restoreEnabled() {
    FILE* file = fopen(kPreference, "r");
    if (!file) return false;
    const bool enabled = fgetc(file) == '1' && fgetc(file) == '\n' && fgetc(file) == EOF;
    fclose(file);
    return enabled;
}

bool saveEnabled(bool enabled) {
    char temporary[] = "/var/opt/hmi/data/ID5/Plugins/HudTest/hud-enabled.XXXXXX";
    int fd = mkstemp(temporary);
    if (fd < 0) return false;
    const bool written = write(fd, enabled ? "1\n" : "0\n", 2) == 2 && fsync(fd) == 0;
    const bool closed = close(fd) == 0;
    if (written && closed && rename(temporary, kPreference) == 0) return true;
    unlink(temporary);
    return false;
}
}

class HudTest : public bmw::PluginApp {
#ifdef HUD_CDS_SOURCE
    CdsTelemetry cds_;
    int feed_;
    void sendFeed() {
        if(!enabled_||feed_<0) return;
        const uint64_t now=bd_mono_ms();cds_.tick(now);
        hud_feed sample;memset(&sample,0,sizeof sample);memcpy(sample.magic,"HUD1",4);
        double rpm=0;sample.sent_ms=now;sample.rpm=cds_.get(0,now,rpm)?static_cast<int>(rpm+.5):-1;
        // Factory HUD ownership remains exclusively the upstream terminal's job.
        sample.nav_idle=0;
        sockaddr_in address;memset(&address,0,sizeof address);
        address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        address.sin_port=htons(HUD_FEED_PORT);
        sendto(feed_,&sample,sizeof sample,0,reinterpret_cast<sockaddr*>(&address),sizeof address);
    }
#endif
    pid_t child_;
    bool enabled_;
    bool polling_;
    unsigned retries_;
    void status(const char* text) { updateModel(50u, bmw::BasicString<char>(text)); }

    void poll(unsigned delay =
#ifdef HUD_CDS_SOURCE
              100
#else
              200
#endif
              ) {
        if (polling_) return;
        polling_ = true;
        bmw::Parcel data;
        data.writeUInt32(kPoll);
        post(bmw::Message(0x40000003u, data), delay);
    }

    void startWorker() {
#ifdef HUD_CDS_SOURCE
        const char* args[] = {kWorker, "--draw-live", "127.0.0.1", 0};
#else
        const DiagnosticGateway gateway("/var/opt/hmi/data/ID5/Plugins/HudTest/gateway-ip.txt");
        const char* args[] = {kWorker, "--draw-live", gateway.host(), 0};
#endif
        child_ = launch(args, -1);
        if (child_ > 0) {
            status("HUD gauges enabled. Uncheck to turn off.");
            poll();
        } else retry("Could not start HUD gauges.");
    }

    void retry(const char* message) {
        if (enabled_ && retries_) {
            --retries_;
            poll(1500);
        }
        status(message);
    }

    bool show(const bmw::Parcel&) {
        const bool next = !enabled_;
        if (!saveEnabled(next)) {
            updateModel(51u, enabled_);
            status("Could not save the HUD setting.");
            return true;
        }
        enabled_ = next;
#ifdef HUD_CDS_SOURCE
        if(enabled_) cds_.start(1u); else cds_.stop();
#endif
        updateModel(51u, enabled_);
        retries_ = enabled_ ? 3 : 0;
        if (enabled_) {
            if (child_ > 0) poll();
            else startWorker();
        } else if (child_ > 0) {
            kill(child_, SIGTERM);
            status("Clearing HUD gauges.");
        } else status("HUD gauges off.");
        return true;
    }

public:
    HudTest(bmw::IDispatcher& d, bmw::BasicString<char> name, unsigned maj, unsigned min,
            bmw::RHMIInterfaceVersion version, const bmw::Bundle& cfg, const bmw::Bundle& res)
        : PluginApp(d, name, maj, min, version, cfg, res),
#ifdef HUD_CDS_SOURCE
          cds_(*this), feed_(-1),
#endif
          child_(-1), enabled_(false),
          polling_(false), retries_(0) {}
    virtual ~HudTest() {
        if (child_ > 0) kill(child_, SIGTERM);
#ifdef HUD_CDS_SOURCE
        cds_.stop();if(feed_>=0) close(feed_);
#endif
    }
    virtual bool onInit() {
#ifdef HUD_CDS_SOURCE
        if(!cds_.ready()) return false;
        feed_=socket(AF_INET,SOCK_DGRAM,0);
        if(feed_<0) return false;
        if(fcntl(feed_,F_SETFL,O_NONBLOCK)<0||fcntl(feed_,F_SETFD,FD_CLOEXEC)<0) {close(feed_);feed_=-1;return false;}
#endif
        typedef bmw::Delegate<bool, const bmw::Parcel&> Action;
        Action action;
        action.m_obj = this;
        action.m_exec = &Action::execImpl<HudTest, &HudTest::show>;
        addHandler(21u, action);
        enabled_ = restoreEnabled();
#ifdef HUD_CDS_SOURCE
        if(enabled_) cds_.start(1u);
#endif
        updateModel(51u, enabled_);
        if (enabled_) {
            retries_ = 3;
            status("Starting saved HUD gauges.");
            poll(1800);
        } else status("Check to show HUD gauges and remember it at startup.");
        return true;
    }
    virtual bool invokeDataUpdate(unsigned key, const bmw::Parcel& data) {
        if (key != kPoll) return PluginApp::invokeDataUpdate(key, data);
        polling_ = false;
#ifdef HUD_CDS_SOURCE
        sendFeed();
#endif
        if (child_ <= 0) {
            if (enabled_) startWorker();
            return true;
        }
        int outcome = 0;
        const pid_t result = waitpid(child_, &outcome, WNOHANG);
        if (result == 0 || (result < 0 && errno == EINTR)) {
            poll();
            return true;
        }
        child_ = -1;
        const int code = result > 0 && WIFEXITED(outcome) ? WEXITSTATUS(outcome) : -1;
        if (!enabled_) status("HUD gauges off.");
        else if (code == 0 || code == 1 || code == 2)
            retry("Waiting for the supported factory MOST HUD.");
        else if (code == 4) status("HUD rejected the overlay. This cluster layout may be unsupported.");
        else if (code == 6) retry(
#ifdef HUD_CDS_SOURCE
            "No fresh CDS RPM. Check ignition."
#else
            "No HUD speed/RPM diagnostics. Check the gateway and ignition."
#endif
            );
        else status(
#ifdef HUD_CDS_SOURCE
            "HUD gauges stopped. Check HUDTerminal and navigation."
#else
            "HUD gauges stopped. Check ENET and navigation."
#endif
            );
        return true;
    }
};

extern "C" {
PluginAppInfo pluginAppInfo = {1, "HudTest", "owner-development", 1,
                              create_app<HudTest>, destroy_app};
}
