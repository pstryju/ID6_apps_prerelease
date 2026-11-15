#ifndef EVO_HUD_FEED_H
#define EVO_HUD_FEED_H
#include <stdint.h>
#include <string.h>
enum { HUD_FEED_PORT=42862,HUD_BOOST_PORT=42863,HUD_STATUS_PORT=42864 };
struct hud_feed {char magic[4];uint64_t sent_ms;int rpm,nav_idle;};
struct hud_status {char magic[4];int state,host;};
inline bool hud_feed_valid(const hud_feed& f,uint64_t now){
    return !memcmp(f.magic,"HUD1",4)&&f.sent_ms&&f.sent_ms<=now&&now-f.sent_ms<1000&&
        f.rpm>=-1&&f.rpm<=10000&&(f.nav_idle==0||f.nav_idle==1);
}
#endif
