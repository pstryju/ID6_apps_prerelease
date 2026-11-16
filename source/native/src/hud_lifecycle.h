#ifndef EVO_HUD_LIFECYCLE_H
#define EVO_HUD_LIFECYCLE_H
// A fast OFF/ON must finish reaping the previous renderer before starting
// or feeding another session. The plugin keeps ownership of its PID.
struct HudLifecycle {
    bool enabled,stopping;
    HudLifecycle():enabled(false),stopping(false){}
    void request(bool on){if(enabled&&!on)stopping=true;enabled=on;}
    void reaped(bool childAlive){if(!childAlive)stopping=false;}
    bool canRun()const{return enabled&&!stopping;}
};
#endif
