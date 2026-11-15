/* Self-contained SHA-256 and bounded bundle receiver for the restricted QNX shell. */
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>

typedef struct { uint32_t h[8]; uint64_t bytes; unsigned used; unsigned char block[64]; } evo_sha;
static uint32_t evo_ror(uint32_t n,unsigned k) { return (n>>k)|(n<<(32-k)); }
static void evo_block(evo_sha *s,const unsigned char *p) {
    static const uint32_t k[64]={
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    uint32_t w[64],a,b,c,d,e,f,g,h,t,u; unsigned i;
    for(i=0;i<16;i++) w[i]=((uint32_t)p[4*i]<<24)|((uint32_t)p[4*i+1]<<16)|((uint32_t)p[4*i+2]<<8)|p[4*i+3];
    for(i=16;i<64;i++) w[i]=w[i-16]+(evo_ror(w[i-15],7)^evo_ror(w[i-15],18)^(w[i-15]>>3))+w[i-7]+(evo_ror(w[i-2],17)^evo_ror(w[i-2],19)^(w[i-2]>>10));
    a=s->h[0];b=s->h[1];c=s->h[2];d=s->h[3];e=s->h[4];f=s->h[5];g=s->h[6];h=s->h[7];
    for(i=0;i<64;i++) {
        t=h+(evo_ror(e,6)^evo_ror(e,11)^evo_ror(e,25))+((e&f)^((~e)&g))+k[i]+w[i];
        u=(evo_ror(a,2)^evo_ror(a,13)^evo_ror(a,22))+((a&b)^(a&c)^(b&c));
        h=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+u;
    }
    s->h[0]+=a;s->h[1]+=b;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;s->h[5]+=f;s->h[6]+=g;s->h[7]+=h;
}
static void evo_init(evo_sha *s) {
    static const uint32_t initial[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    memset(s,0,sizeof *s);memcpy(s->h,initial,sizeof initial);
}
static void evo_update(evo_sha *s,const unsigned char *p,size_t n) {
    s->bytes+=n;
    while(n) { unsigned take=64-s->used;if(take>n)take=(unsigned)n;memcpy(s->block+s->used,p,take);s->used+=take;p+=take;n-=take;if(s->used==64){evo_block(s,s->block);s->used=0;} }
}
static void evo_final(evo_sha *s,char out[65]) {
    uint64_t bits=s->bytes*8;unsigned i;unsigned char pad=0x80,zero=0;
    evo_update(s,&pad,1);while(s->used!=56)evo_update(s,&zero,1);
    for(i=0;i<8;i++){pad=(unsigned char)(bits>>(56-8*i));evo_update(s,&pad,1);}
    for(i=0;i<8;i++)snprintf(out+8*i,9,"%08lx",(unsigned long)s->h[i]);
    out[64]=0;
}
static int evo_verify(const char *path,const char *expected) {
    FILE *f;struct stat st;evo_sha s;unsigned char buf[16384];size_t n;char hash[65];
    if(strlen(expected)!=64 || strspn(expected,"0123456789abcdef")!=64)return 1;
    if(lstat(path,&st)||!S_ISREG(st.st_mode))return 1;
    f=fopen(path,"rb");if(!f)return 1;evo_init(&s);
    while((n=fread(buf,1,sizeof buf,f))!=0)evo_update(&s,buf,n);
    if(ferror(f)){fclose(f);return 1;}fclose(f);evo_final(&s,hash);
    if(strcmp(hash,expected)){fprintf(stderr,"SHA256 mismatch: %s\n",path);return 1;}
    printf("SHA256 OK: %s\n",path);fflush(stdout);return 0;
}
static int evo_relative(const char *p) {
    const char *part=p;const char *q;
    if(!*p||strlen(p)>240)return 0;
    for(q=p;;q++) {
        char c=*q;
        if(!c||c=='/') {
            size_t n=(size_t)(q-part);if(!n||(n==1&&part[0]=='.')||(n==2&&part[0]=='.'&&part[1]=='.'))return 0;
            if(!c)return 1;
            part=q+1;
        } else if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='_'||c=='-'))return 0;
    }
}
static int evo_receive(const char *root) {
    char line[512],relative[256],expected[65],path[1024];struct stat st;unsigned count=0;
    unsigned long total=0;
    if(root[0]!='/'||lstat(root,&st)||!S_ISDIR(st.st_mode))return 1;
    while(fgets(line,sizeof line,stdin)) {
        unsigned long length,left;int end=0,fd;FILE *out;char *q;unsigned char buf[16384];evo_sha s;char actual[65];
        if(!strcmp(line,"END\n")) {
            int trailing=fgetc(stdin);
            if(trailing!=EOF){fprintf(stderr,"Unexpected bundle trailing byte: %02x\n",(unsigned)trailing);return 1;}
            puts("BUNDLE_COMPLETE");fflush(stdout);return 0;
        }
        if(sscanf(line,"FILE %lu %64s %255s%n",&length,expected,relative,&end)!=3||strcmp(line+end,"\n")||
           length>32ul*1024*1024||++count>128||total+length>128ul*1024*1024||!evo_relative(relative)||
           strlen(expected)!=64||strspn(expected,"0123456789abcdef")!=64)return 1;
        total+=length;
        if(snprintf(path,sizeof path,"%s/%s",root,relative)>=(int)sizeof path)return 1;
        for(q=path+strlen(root)+1;*q;q++)if(*q=='/') {
            *q=0;
            if(lstat(path,&st)) {if(errno!=ENOENT||mkdir(path,0700))return 1;}
            else if(!S_ISDIR(st.st_mode))return 1;
            *q='/';
        }
        fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0)return 1;
        out=fdopen(fd,"wb");if(!out){close(fd);return 1;}
        evo_init(&s);left=length;
        while(left) {
            size_t want=left>sizeof buf?sizeof buf:(size_t)left,n=fread(buf,1,want,stdin);
            if(n!=want||fwrite(buf,1,n,out)!=n){fclose(out);return 1;}
            evo_update(&s,buf,n);left-=(unsigned long)n;
        }
        if(fclose(out))return 1;
        evo_final(&s,actual);
        if(strcmp(actual,expected)){fprintf(stderr,"Bundle hash mismatch: %s\n",relative);return 1;}
        printf("RECEIVED %lu B SHA256 OK: %s\n",length,relative);fflush(stdout);
    }
    fputs("Bundle missing END marker\n",stderr);return 1;
}
