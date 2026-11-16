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
inline std::vector<nbt::hudterm::Point> line(int width){
    std::vector<nbt::hudterm::Point> p(2);p[0].x=0;p[0].y=kGraphY;p[1].x=(int16_t)width;p[1].y=kGraphY;return p;
}
inline nbt::hudterm::Batch rpmGraph(int rpm,unsigned front,bool first){
    using namespace nbt::hudterm;
    unsigned back=front==5?6:5;
    Batch b("HUD RPM line",true);
    b.add(kRemoveAllItems,removeAllItems((uint16_t)back));
    b.add(kSetLineColor,setLineColor((uint16_t)back,4));
    b.add(kSetLineWidth,setLineWidth((uint16_t)back,1));
    b.add(kAddPolygon,addPolygon((uint16_t)back,0,kPolyline,line(kGraphWidth)));
    int width=rpmWidth(rpm);
    if(width){
        b.add(kSetLineWidth,setLineWidth((uint16_t)back,5));
        b.add(kAddPolygon,addPolygon((uint16_t)back,1,kPolyline,line(width)));
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
