#include "hud_trace.h"
#include <assert.h>
#include <string.h>
static void readback(FILE* f,char* out,size_t n){fflush(f);rewind(f);size_t k=fread(out,1,n-1,f);out[k]=0;fclose(f);}
int main(){
    char result[1024];
    const uint8_t text[]={0,2,0,'S','E','C','R','E','T'};
    FILE* file=tmpfile();assert(file);hud_trace_record(file,100,0x101,0x2001,0x210,0xd,text,sizeof text);readback(file,result,sizeof result);
    assert(strstr(result,"payload=0002 [content redacted]")&&!strstr(result,"534543524554"));
    const uint8_t position[]={0,5,0xff,0xf4,0,8,0,0,0,0,0,0};
    file=tmpfile();assert(file);hud_trace_record(file,101,0x101,0x2001,0xc06,0xd,position,sizeof position);readback(file,result,sizeof result);
    assert(strstr(result,"handle=5 coX=-12 coY=8 ctX=0 ctY=0 angle=0"));
    uint8_t large[200];memset(large,0xaa,sizeof large);
    file=tmpfile();assert(file);hud_trace_record(file,102,0x101,0x2001,0x21d,0xd,large,sizeof large);readback(file,result,sizeof result);
    assert(strstr(result,"[truncated]")&&strlen(result)<300);
    puts("HUD trace: bounded records, signed position decoding and text redaction passed.");
}
