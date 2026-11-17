#ifndef EVO_HUD_RPM_GRAPH_H
#define EVO_HUD_RPM_GRAPH_H
#include "cluster/hud_terminal.hpp"
// Experimental windshield profile, based on packaged hud-shape-qnx:
// draw() 0x1047c8 alternates image handles 5/6; uses x=0..116.
// A straight line stays inside its tach-band footprint. Colour 4 is a
// terminal palette role, not RGB; its appearance needs a windshield test.
namespace evo_hud {
enum { kRpmMaximum=8000,kGraphWidth=116,kGraphY=30 };
inline int rpmWidth(int rpm){if(rpm<=0)return 0;if(rpm>=kRpmMaximum)return kGraphWidth;return (rpm*kGraphWidth+kRpmMaximum/2)/kRpmMaximum;}
inline std::vector<nbt::hudterm::Point> strip(int width,int top,int height){
    std::vector<nbt::hudterm::Point> p(4);
    p[0].x=0;p[0].y=(int16_t)top;p[1].x=(int16_t)width;p[1].y=(int16_t)top;
    p[2].x=(int16_t)width;p[2].y=(int16_t)(top+height);p[3].x=0;p[3].y=(int16_t)(top+height);return p;
}
inline nbt::hudterm::Batch rpmGraph(int rpm,unsigned front,bool first){
    using namespace nbt::hudterm;
    unsigned back=front==5?6:5;
    Batch b("HUD RPM line",true);
    b.add(kRemoveAllItems,removeAllItems((uint16_t)back));
    // Match the packaged windshield renderer's filled-polygon path. Its
    // support for ImageSetLineColor/Width or two-point polylines is unproven.
    b.add(kSetFillColor,setFillColor((uint16_t)back,4));
    b.add(kAddPolygon,addPolygon((uint16_t)back,0,kPolygon,strip(kGraphWidth,kGraphY+2,1)));
    int width=rpmWidth(rpm);
    if(width){
        b.add(kAddPolygon,addPolygon((uint16_t)back,1,kPolygon,strip(width,kGraphY,5)));
    }
    if(first)b.add(kTextareaSetText,setText(2,""));
    b.add(kSwitchVisibility,switchVisibility((uint16_t)(back==5?6:5),(uint16_t)back));
    if(first)b.add(kSetVisibility,setVisibility(1,true));
    return b;
}
inline nbt::hudterm::Batch clearRpm(){
    using namespace nbt::hudterm;
    Batch b("clear HUD RPM",true,kSlotAll);
    b.add(kRemoveAllItems,removeAllItems(5));b.add(kRemoveAllItems,removeAllItems(6));
    b.add(kTextareaSetText,setText(2,""));return b;
}
}
#endif
