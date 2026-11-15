#ifndef BD_CDS_VALUES_H
#define BD_CDS_VALUES_H
#include <stdlib.h>
#include <string.h>
#include <math.h>
// Small, strict parser for the observed numeric CDS objects. Unknown keys,
// duplicate keys, error objects, truncation and non-JSON numbers fail closed.
class CdsJson {
    const char* p_;
public:
    explicit CdsJson(const char* p):p_(p){}
    void ws(){while(*p_==' '||*p_=='\t'||*p_=='\r'||*p_=='\n')++p_;}
    bool token(char c){ws();if(*p_!=c)return false;++p_;return true;}
    bool key(char* out,unsigned size){
        if(!token('"'))return false;
        unsigned n=0;
        while(*p_&&*p_!='"'){
            if(*p_<' '||*p_=='\\'||n+1>=size)return false;
            out[n++]=*p_++;
        }
        out[n]=0;return token('"')&&token(':');
    }
    bool number(double& out){
        ws();const char* start=p_;
        if(*p_=='-')++p_;
        if(*p_=='0')++p_;
        else {if(*p_<'1'||*p_>'9')return false;while(*p_>='0'&&*p_<='9')++p_;}
        if(*p_=='.'){++p_;if(*p_<'0'||*p_>'9')return false;while(*p_>='0'&&*p_<='9')++p_;}
        if(*p_=='e'||*p_=='E'){
            ++p_;if(*p_=='+'||*p_=='-')++p_;
            if(*p_<'0'||*p_>'9')return false;
            while(*p_>='0'&&*p_<'9'+1)++p_;
        }
        char* end=0;out=strtod(start,&end);
        return end==p_&&out==out&&out>=-1e9&&out<=1e9;
    }
    bool done(){ws();return !*p_;}
};
struct CdsNumbers {
    char keys[16][32];double values[16];unsigned count;
    bool find(const char* name,double& value)const{
        for(unsigned i=0;i<count;++i)if(!strcmp(keys[i],name)){value=values[i];return true;}
        return false;
    }
};
// Read a bounded numeric object, optionally under a single named envelope.
inline bool cds_numbers(const char* text,const char* envelope,CdsNumbers& out){
    CdsJson j(text);CdsNumbers next;next.count=0;char key[32];
    if(!j.token('{'))return false;
    if(envelope&&(!j.key(key,sizeof key)||strcmp(key,envelope)||!j.token('{')))return false;
    do {
        if(next.count==16||!j.key(key,sizeof key))return false;
        double unused; if(next.find(key,unused))return false;
        strcpy(next.keys[next.count],key);
        if(!j.number(next.values[next.count]))return false;
        ++next.count;
    }while(j.token(','));
    if(!j.token('}')||(envelope&&!j.token('}'))||!j.done())return false;
    out=next;return true;
}
// Canonical CDS: km/h, degrees C and litres. Enums from CDSValues.kt.
inline double cds_speed(double kph,unsigned unit){return unit==2?kph/1.609344:kph;}
inline double cds_temperature(double c,unsigned unit){return unit==2?c*1.8+32:c;}
inline double cds_fuel(double litres,unsigned unit){return unit==2?litres/4.54609:unit==3?litres/3.785411784:litres;}
// kind 0 RPM, 1 km/h, 2 engine/oil degrees C. Outputs commit atomically.
inline bool cds_decode(unsigned kind,const char* text,double& first,double& second){
    CdsJson j(text);char key[32];double a=0,b=0;
    if(!j.token('{')||!j.key(key,sizeof key))return false;
    if(kind<2){
        if(strcmp(key,kind==0?"RPMSpeed":"speedActual")||!j.number(a))return false;
        if(a<0||a>(kind==0?10000:350))return false;
    }else if(kind==2){
        if(strcmp(key,"temperature")||!j.token('{'))return false;
        unsigned seen=0;
        for(unsigned i=0;i<2;++i){
            if(i&&!j.token(','))return false;
            double x;
            if(!j.key(key,sizeof key)||!j.number(x))return false;
            unsigned bit=!strcmp(key,"engine")?1u:!strcmp(key,"oil")?2u:0u;
            if(!bit||(seen&bit)||x< -40||x>180)return false;
            if(bit==1)a=x;else b=x;
            seen|=bit;
        }
        if(seen!=3||!j.token('}'))return false;
    }else return false;
    if(!j.token('}')||!j.done())return false;
    first=a;second=b;return true;
}
#endif
