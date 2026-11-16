#include "cluster/hud_terminal.hpp"
#include "hud_feed.h"
#include "hud_lifecycle.h"
#include <assert.h>
#include <stdio.h>
using namespace nbt;
static hudterm::Config config(){hudterm::Config c;c.handles.allow(1);c.handles.allow(2);c.handles.allow(3);c.handles.allow(4);c.navFrame=1;return c;}
static most::View view(uint16_t src,uint16_t dst,uint16_t fkt,uint8_t op,const std::vector<uint8_t>& p){
    most::View v;memset(&v,0,sizeof v);v.varTag=2;v.src=src;v.dst=dst;v.fblock=0x82;v.inst=0;v.fkt=fkt;v.op=op;v.data=p.empty()?0:&p[0];v.len=p.size();return v;
}
int main(){
    HudLifecycle life;
    life.request(true);assert(life.canRun());
    life.request(false);life.reaped(true);assert(!life.canRun());
    life.request(true);assert(!life.canRun()); // old renderer is still clearing/exiting
    life.reaped(false);assert(life.canRun());
    life.request(false);life.request(true);life.request(false);life.reaped(false);assert(!life.canRun());
    hud_feed f;memset(&f,0,sizeof f);memcpy(f.magic,"HUD1",4);f.sent_ms=100;f.rpm=703;f.nav_idle=1;
    assert(hud_feed_valid(f,100)&&!hud_feed_valid(f,1100)&&!hud_feed_valid(f,99));
    f.rpm=65535;assert(!hud_feed_valid(f,100));f.rpm=703;f.nav_idle=2;assert(!hud_feed_valid(f,100));
    hudterm::Core core(config());most::Msg out;
    assert(!core.next(1,out));core.setLink(true,0x2001,1);core.setHost(0x0101,1);assert(core.status()==hudterm::kReady);
    hudterm::Batch b("text",true);b.add(hudterm::kTextareaSetText,hudterm::setText(2,"703 RPM"));
    assert(core.submit(b,2)==hudterm::kQueued);assert(core.next(2,out));
    assert(out.dst==0x0101&&out.fblock==0x82&&out.inst==0&&out.fkt==0x210);
    most::View answer=view(0x0101,0x2001,out.fkt,0x0d,out.data);core.onMessage(answer,3);assert(!core.busy());
    std::vector<uint8_t> on=hudterm::setVisibility(1,true);
    most::View nav=view(0x0101,0x2002,0x204,0x0d,on);core.onMessage(nav,4);
    assert(core.status()==hudterm::kNavActive);assert(core.submit(b,5)==hudterm::kNotReady);assert(!core.next(5,out));
    std::vector<uint8_t> off=hudterm::setVisibility(1,false);nav=view(0x0101,0x2002,0x204,0x0d,off);core.onMessage(nav,10);
    core.tick(5011);assert(core.status()==hudterm::kReady);
    hudterm::Batch bad("bad");bad.add(hudterm::kTextareaSetText,hudterm::setText(99,"bad"));assert(core.submit(bad,5012)==hudterm::kRefused);
    assert(core.submit(b,5013)==hudterm::kQueued);assert(core.next(5013,out));core.tick(7014);assert(core.status()==hudterm::kFailed);
    assert(!core.next(7015,out));
    core.setExternalIdle(false,7015);core.reset(7016);
    assert(core.status()==hudterm::kNavActive&&!core.next(7016,out));
    core.setExternalIdle(true,7016);
    core.reset(7016);core.setHost(0,7016);assert(core.status()==hudterm::kOffline);assert(!core.next(7017,out));
    hudterm::Core gated(config());gated.setExternalIdle(false,1);
    gated.setLink(true,0x2001,1);gated.setHost(0x0101,1);
    assert(gated.status()==hudterm::kNavActive&&!gated.next(2,out));
    gated.setExternalIdle(true,2);assert(gated.status()==hudterm::kReady);
    hudterm::Batch frame("text then visibility",true);
    frame.add(hudterm::kTextareaSetText,hudterm::setText(2,"703 RPM"));
    frame.add(hudterm::kSetVisibility,hudterm::setVisibility(1,true));
    assert(gated.submit(frame,3)==hudterm::kQueued);assert(gated.next(3,out));
    // CDS can announce guidance before the terminal's own MOST messages.
    // Drop both unsent visibility and pending frames; no hide/clear on yield.
    assert(gated.submit(frame,3)==hudterm::kQueued);
    gated.setExternalIdle(false,4);assert(!gated.busy()&&!gated.next(4,out));
    answer=view(0x0101,0x2001,out.fkt,0x0d,out.data);gated.onMessage(answer,5);
    gated.tick(10000);assert(gated.status()==hudterm::kNavActive&&!gated.next(10000,out));
    // A raw route still owns the terminal even if CDS briefly reports idle.
    nav=view(0x0101,0x2002,0x204,0x0d,on);gated.onMessage(nav,10001);
    gated.setExternalIdle(true,10002);assert(gated.status()==hudterm::kNavActive);
    nav=view(0x0101,0x2002,0x204,0x0d,off);gated.onMessage(nav,10003);
    gated.tick(15004);assert(gated.status()==hudterm::kReady&&!gated.next(15004,out));
    assert(gated.submit(b,15005)==hudterm::kQueued);assert(gated.next(15005,out));
    puts("HUD: fresh feed, terminal gating, text payload, reply, nav yield/resume and timeout passed.");
}
