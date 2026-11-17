#include "cluster/hud_terminal.hpp"
#include "hud_feed.h"
#include "hud_lifecycle.h"
#include "hud_rpm_graph.h"
#include <assert.h>
#include <stdio.h>
using namespace nbt;
static hudterm::Config config(){hudterm::Config c;for(int i=1;i<=6;++i)c.handles.allow(i);c.navFrame=1;return c;}
static most::View view(uint16_t src,uint16_t dst,uint16_t fkt,uint8_t op,const std::vector<uint8_t>& p){
    most::View v;memset(&v,0,sizeof v);v.varTag=2;v.src=src;v.dst=dst;v.fblock=0x82;v.inst=0;v.fkt=fkt;v.op=op;v.data=p.empty()?0:&p[0];v.len=p.size();return v;
}
int main(){
    assert(evo_hud::rpmWidth(-1)==0&&evo_hud::rpmWidth(0)==0);
    assert(evo_hud::rpmWidth(4000)==58&&evo_hud::rpmWidth(8000)==116&&evo_hud::rpmWidth(10000)==116);
    int previous=0;for(int rpm=0;rpm<=10000;++rpm){int width=evo_hud::rpmWidth(rpm);assert(width>=previous&&width<=116);previous=width;}
    hudterm::Config graphics=config();graphics.maxInFlight=1;
    for(unsigned front=5;front<=6;++front){
        unsigned back=front==5?6:5;hudterm::Batch graph=evo_hud::rpmGraph(4000,front,true);
        assert(graph.calls.size()<=graphics.maxBatch);bool swapped=false,fill=false;
        for(size_t i=0;i<graph.calls.size();++i){
            const hudterm::Call& c=graph.calls[i];assert(hudterm::callOk(c,graphics.handles));
            assert(c.handle()==back||c.handle()==1||c.handle()==2||(c.fkt==hudterm::kSwitchVisibility&&c.handle()==front));
            if(c.fkt==hudterm::kAddPolygon){
                assert(!swapped&&c.payload[5]==8&&c.payload[4]==hudterm::kPolygon);
                assert(hudterm::rdBe16(&c.payload[12])==30);
                assert(hudterm::rdBe16(&c.payload[10])==58);fill=true;
            }
            if(c.fkt==hudterm::kSwitchVisibility){assert(fill);assert(c.handle()==front&&hudterm::rdBe16(&c.payload[2])==back);swapped=true;}
            if(c.fkt==hudterm::kSetVisibility)assert(c.handle()==1&&c.payload[2]==1);
        }
        assert(swapped&&fill);
        hudterm::Core renderer(graphics);renderer.setLink(true,0x2001,1);renderer.setHost(0x0101,1);
        assert(renderer.submit(graph,2)==hudterm::kQueued);most::Msg packet;unsigned calls=0;
        while(renderer.next(3+calls,packet)){
            most::Msg blocked;assert(!renderer.next(3+calls,blocked)); // wait for each acknowledgment
            most::View ack=view(0x0101,0x2001,packet.fkt,0x0d,packet.data);renderer.onMessage(ack,4+calls);++calls;
        }
        assert(calls==graph.calls.size()&&!renderer.busy());
        hudterm::Batch cleanup=evo_hud::clearRpm();
        for(size_t i=0;i<cleanup.calls.size();++i){assert(hudterm::callOk(cleanup.calls[i],graphics.handles));assert(cleanup.calls[i].fkt!=hudterm::kSetVisibility&&cleanup.calls[i].fkt!=hudterm::kSwitchVisibility);}
        assert(renderer.submit(graph,100)==hudterm::kQueued);assert(renderer.next(100,packet));
        renderer.setExternalIdle(false,101);assert(!renderer.next(101,packet)&&!renderer.busy());
    }
    for(int rpm=0;rpm<=8000;rpm+=50){
        std::vector<hudterm::Point> shape=evo_hud::rpmShape(rpm);int twiceArea=0;
        for(size_t i=0;i<shape.size();++i){const hudterm::Point& a=shape[i];const hudterm::Point& b=shape[(i+1)%shape.size()];
            assert(a.x>=0&&a.x<=116&&a.y>=30&&a.y<=35);twiceArea+=a.x*b.y-b.x*a.y;}
        assert(twiceArea==2*(116+4*evo_hud::rpmWidth(rpm)));
    }
    // Steady-state frame: two ordered construction messages in parallel,
    // then a swap barrier. With 120 ms replies, finish in two RTTs, not five.
    hudterm::Config fast=config();fast.maxInFlight=3;hudterm::Core pipeline(fast);
    pipeline.setLink(true,0x2001,1);pipeline.setHost(0x0101,1);
    hudterm::Batch update=evo_hud::rpmGraph(4000,6,false,false);assert(update.calls.size()==3);
    assert(pipeline.submit(update,2)==hudterm::kQueued);
    most::Msg erase,polygon,swap,blocked;
    assert(pipeline.next(2,erase)&&pipeline.next(2,polygon)&&!pipeline.next(2,swap));
    most::View reply=view(0x0101,0x2001,polygon.fkt,0x0d,polygon.data);pipeline.onMessage(reply,122);
    assert(!pipeline.next(122,swap)); // even out-of-order replies cannot expose an unfinished image
    reply=view(0x0101,0x2001,erase.fkt,0x0d,erase.data);pipeline.onMessage(reply,122);
    assert(pipeline.next(122,swap)&&swap.fkt==hudterm::kSwitchVisibility);
    reply=view(0x0101,0x2001,swap.fkt,0x0d,swap.data);pipeline.onMessage(reply,242);
    assert(!pipeline.busy());
    assert(pipeline.submit(update,300)==hudterm::kQueued);
    assert(pipeline.next(300,erase)&&pipeline.next(300,polygon));
    pipeline.setExternalIdle(false,301);assert(!pipeline.next(301,blocked)&&!pipeline.busy());
    hudterm::Core refused(graphics);most::Msg command;
    refused.setLink(true,0x2001,1);refused.setHost(0x0101,1);
    assert(refused.submit(evo_hud::rpmGraph(1000,6,true),2)==hudterm::kQueued&&refused.next(2,command));
    most::View error=view(0x0101,0x2001,command.fkt,0x09,command.data);refused.onMessage(error,3);
    assert(refused.status()==hudterm::kFailed&&strstr(refused.failureReason(),"220 handle 5"));
    assert(!refused.next(4,command)); // never continue blindly after a rejected graphics command
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
