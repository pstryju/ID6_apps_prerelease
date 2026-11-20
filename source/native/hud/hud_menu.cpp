// SPDX-License-Identifier: MIT
#include "plugin_factory.h"
#include "diagnostic_worker.h"
#include "diagnostic_gateway.h"
#include <sys/wait.h>
#include <stdlib.h>

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
    pid_t child_;
    bool enabled_;
    bool polling_;
    unsigned retries_;
    void status(const char* text) { updateModel(50u, bmw::BasicString<char>(text)); }

    void poll(unsigned delay = 200) {
        if (polling_) return;
        polling_ = true;
        bmw::Parcel data;
        data.writeUInt32(kPoll);
        post(bmw::Message(0x40000003u, data), delay);
    }

    void startWorker() {
        const DiagnosticGateway gateway("/var/opt/hmi/data/ID5/Plugins/HudTest/gateway-ip.txt");
        const char* args[] = {kWorker, "--draw-live", gateway.host(), 0};
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
        : PluginApp(d, name, maj, min, version, cfg, res), child_(-1), enabled_(false),
          polling_(false), retries_(0) {}
    virtual ~HudTest() { if (child_ > 0) kill(child_, SIGTERM); }
    virtual bool onInit() {
        typedef bmw::Delegate<bool, const bmw::Parcel&> Action;
        Action action;
        action.m_obj = this;
        action.m_exec = &Action::execImpl<HudTest, &HudTest::show>;
        addHandler(21u, action);
        enabled_ = restoreEnabled();
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
        else if (code == 6) retry("No HUD speed/RPM diagnostics. Check the gateway and ignition.");
        else status("HUD gauges stopped. Check ENET and navigation.");
        return true;
    }
};

extern "C" {
PluginAppInfo pluginAppInfo = {1, "HudTest", "owner-development", 1,
                              create_app<HudTest>, destroy_app};
}
