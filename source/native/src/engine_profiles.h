#ifndef BD_ENGINE_PROFILES_H
#define BD_ENGINE_PROFILES_H
#include "gauge_core.h"
#include "signal_decode.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
enum { BD_PROFILE_MAX_READS=16 };
typedef struct { bd_signal signal; unsigned slot,period_ms; double multiplier; } bd_profile_read;
typedef struct { const char *name; const bd_profile_read *reads; unsigned count; } bd_engine_profile;
static const bd_profile_read bd_n20_reads[]={
#include "profiles/n20.inc"
};
static const bd_engine_profile bd_engine_profiles[]={
    {"n20",bd_n20_reads,sizeof bd_n20_reads/sizeof bd_n20_reads[0]},
    /* Deliberately empty: no captured B48/N13 mappings have been validated. */
    {"b48",0,0},{"n13",0,0}
};
static inline const bd_engine_profile *bd_engine_profile_find(const char *name){
    unsigned i;for(i=0;i<sizeof bd_engine_profiles/sizeof bd_engine_profiles[0];++i)
        if(!strcmp(name,bd_engine_profiles[i].name))return &bd_engine_profiles[i];
    return 0;
}
/* No VIN/model/year guesses. Only explicit engine designations in a DME
 * identification response are candidates. Not a part-number database.
 * 0: unknown, -1: ambiguous, positive: index + 1. */
static inline int bd_engine_identification(const unsigned char *p,size_t n){
    size_t i;unsigned k;int found=0;
    for(i=0;i+3<=n;++i)for(k=0;k<3;++k){
        const char *name=bd_engine_profiles[k].name;
        if(i&&isalnum((unsigned char)p[i-1]))continue;
        if(tolower((unsigned char)p[i])!=name[0]||p[i+1]!=(unsigned char)name[1]||p[i+2]!=(unsigned char)name[2])continue;
        if(i+3<n&&isalnum((unsigned char)p[i+3])&&
           !(i+5<n&&(p[i+3]=='A'||p[i+3]=='B')&&isdigit(p[i+4])&&isdigit(p[i+5])))continue;
        if(found&&found!=(int)k+1)return -1;
        found=(int)k+1;
    }
    return found;
}
/* Optional small file beside the executable; absence preserves this owner's
 * existing explicit N20 configuration. Malformed/present unreadable fails. */
static inline int bd_engine_profile_setting(const char *executable,char out[16]){
    char path[512],text[32];const char *slash=strrchr(executable,'/');size_t prefix=slash?(size_t)(slash-executable)+1:0,n;
    FILE *file;
    if(prefix+sizeof "engine-profile.txt">sizeof path)return 0;
    memcpy(path,executable,prefix);strcpy(path+prefix,"engine-profile.txt");
    file=fopen(path,"rb");
    if(!file){if(errno!=ENOENT)return 0;strcpy(out,"n20");return 1;}
    n=fread(text,1,sizeof text,file);
    if(ferror(file)||n==sizeof text){fclose(file);return 0;}fclose(file);
    while(n&&(text[n-1]=='\n'||text[n-1]=='\r'))--n;
    if(n==0||n>=16||memchr(text,0,n))return 0;
    memcpy(out,text,n);out[n]=0;
    return !strcmp(out,"auto")||bd_engine_profile_find(out)!=0;
}
#endif
