#define _POSIX_C_SOURCE 200809L
#include "gauge_core.h"
#include "diagnostic_lease.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static volatile sig_atomic_t running = 1;
static void stop(int sig) { (void)sig; running = 0; }

static int ready(int fd, short events, uint64_t deadline) {
    struct pollfd p;
    int rc;
    p.fd = fd; p.events = events; p.revents = 0;
    while (running) {
        uint64_t now = bd_mono_ms();
        if (now >= deadline) return 0;
        rc = poll(&p, 1, (int)(deadline - now));
        if (rc < 0 && errno == EINTR) continue;
        return rc > 0 && (p.revents & events);
    }
    return 0;
}
static int transfer(int fd, unsigned char *buf, size_t n, int sending, uint64_t deadline) {
    while (n && running) {
        ssize_t k;
        if (!ready(fd, sending ? POLLOUT : POLLIN, deadline)) return 0;
        k = sending ? send(fd, buf, n, 0) : recv(fd, buf, n, 0);
        if (k < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (k <= 0) return 0;
        buf += k; n -= (size_t)k;
    }
    return n == 0;
}
static int connect_gw(const char *host, unsigned port) {
    struct sockaddr_in a;
    int fd, error = 0, no_delay = 1;
    socklen_t len = sizeof error;
    memset(&a, 0, sizeof a); a.sin_family = AF_INET; a.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, host, &a.sin_addr) != 1) return -1;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof no_delay);
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) { close(fd); return -1; }
    if (connect(fd, (struct sockaddr *)&a, sizeof a) < 0 && errno != EINPROGRESS) {
        close(fd); return -1;
    }
    if (!ready(fd, POLLOUT, bd_mono_ms() + 1000)) { close(fd); errno=ETIMEDOUT; return -1; }
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)) {
        int saved=errno;close(fd);errno=saved;return -1;
    }
    if(error){close(fd);errno=error;return -1;}
    return fd;
}

/* The fixed callers below supply the UDS service, target, and response prefix. */
static int exchange(int fd, unsigned ecu, const unsigned char *uds, unsigned uds_size,
                    const unsigned char *prefix, unsigned prefix_size,
                    unsigned char *reply, unsigned reply_size, unsigned timeout_ms,
                    unsigned *negative) {
    unsigned char request[48] = {0,0,0,0,0,1,0xf4,0};
    unsigned char header[6], body[4096];
    uint64_t deadline = bd_mono_ms() + timeout_ms;
    if (uds_size > sizeof request - 8) return -1;
    request[3] = (unsigned char)(uds_size+2);
    request[7] = (unsigned char)ecu;
    memcpy(request+8,uds,uds_size);
    *negative=0;
    if (!transfer(fd,request,8+uds_size,1,deadline)) return -1;
    while (running) {
        uint32_t len;
        unsigned ctrl;
        if (!transfer(fd,header,6,0,deadline)) return -1;
        len=((uint32_t)header[0]<<24)|((uint32_t)header[1]<<16)|((uint32_t)header[2]<<8)|header[3];
        ctrl=((unsigned)header[4]<<8)|header[5];
        if (len>sizeof body || !transfer(fd,body,len,0,deadline)) return -1;
        if (ctrl==0x12) {
            unsigned char alive[8]={0,0,0,2,0,0x13,0,0xf4};
            if (!transfer(fd,alive,8,1,deadline)) return -1;
            continue;
        }
        if (ctrl!=1 || len<3 || body[0]!=ecu || body[1]!=0xf4) continue;
        if (len==5 && body[2]==0x7f && body[3]==uds[0]) {
            if (body[4]==0x78) continue;
            *negative=body[4];
            return -2;
        }
        if (len-2<prefix_size || memcmp(body+2,prefix,prefix_size)) continue;
        if (len-2>reply_size) return -1;
        memcpy(reply,body+2,len-2);
        return (int)(len-2);
    }
    return -1;
}

static unsigned number(const char *s, unsigned lo, unsigned hi) {
    char *end; unsigned long n = strtoul(s, &end, 10);
    if (!*s || *end || n < lo || n > hi) { fprintf(stderr, "Invalid number: %s\n", s); exit(2); }
    return (unsigned)n;
}

#if defined(BM3_ONLY) || defined(XDRIVE_ONLY)
#include <ctype.h>
#include "controls.c"
#elif defined(DIAGNOSTIC_PROBE_ONLY)
int main(int argc,char **argv) {
    unsigned port=6801,negative,i,j,count;int lock,fd=-1,n,signals=0,arg;
    const unsigned char requests[12][3]={{1,0x0c,0},{1,5,0},{0x22,0x43,0},{0x22,0x56,0xd7},
        {1,0x23,0},{1,0x5c,0},{1,0x42,0},{1,4,0},{0x22,0x42,1},{0x22,0x42,5},{1,0x0b,0},{1,0x33,0}};
    const char *labels[12]={"RPM","coolant","BMW-coolant","BMW-rail","OBD-rail","oil-temperature",
        "DME-voltage","engine-load","BMW-ambient","BMW-charge","OBD-MAP","OBD-baro"};
    const unsigned char basic[3][2]={{1,0x0c},{1,0x0d},{1,5}};
    const char *basic_labels[3]={"RPM","speed","coolant"};
    if(argc<3||strcmp(argv[1],"--host"))return 2;
    for(arg=3;arg<argc;arg++) {
        if(!strcmp(argv[arg],"--port")&&arg+1<argc)port=number(argv[++arg],1,65535);
        else if(!strcmp(argv[arg],"--signals"))signals=1;
        else return 2;
    }
    count=signals?12:3;
    signal(SIGINT,stop);signal(SIGTERM,stop);signal(SIGPIPE,SIG_IGN);
    lock=reserve_gateway();
    if(lock<0){puts("Diagnostics busy: exit gauge apps and close other diagnostic clients.");return 1;}
    for(i=0;i<count&&running;i++) {
        const unsigned char *request=signals?requests[i]:basic[i];
        unsigned size=request[0]==0x22?3:2;
        unsigned char prefix[3],reply[64];
        if(fd<0)fd=connect_gw(argv[2],port);
        if(fd<0){printf("CONNECT FAILED %s:%u: %s\n",argv[2],port,strerror(errno));close(lock);return 1;}
        if(!i)printf("TCP CONNECTED %s:%u\n",argv[2],port);
        memcpy(prefix,request,size);prefix[0]=(unsigned char)(request[0]+0x40);
        n=exchange(fd,0x12,request,size,prefix,size,reply,sizeof reply,700,&negative);
        printf("READ %s request=",signals?labels[i]:basic_labels[i]);
        for(j=0;j<size;j++)printf("%02X",request[j]);
        printf(" result=%d",n);
        if(n>0){printf(" response=");for(j=0;j<(unsigned)n;j++)printf("%02X",reply[j]);}
        else if(n==-2)printf(" NRC=%02X",negative);
        puts("");fflush(stdout);
        if(signals){close(fd);fd=-1;}
        else if(n==-1)break;
        poll(0,0,150);
    }
    if(fd>=0)close(fd);
    close(lock);return 0;
}
#elif defined(ENGINE_CODES_ONLY)
static void fault_codes(int fd, int clear) {
    /* Fault, pending, confirmed, failed since clear, or warning requested.
     * Bits 0x10 and 0x40 only report incomplete tests. */
    enum { fault_status = 0xaf };
    const unsigned char read_request[]={0x19,0x02,fault_status}, read_reply[]={0x59,0x02};
    const unsigned char clear_request[]={0x14,0xff,0xff,0xff}, clear_reply[]={0x54};
    unsigned char reply[4094];
    unsigned negative;
    unsigned supported_fault_status;
    int n, i, count=0, shown=0;
    if (fd<0) { puts("Diagnostic gateway unavailable"); return; }
    if (clear) {
        n=exchange(fd,0x12,clear_request,4,clear_reply,1,reply,sizeof reply,8000,&negative);
        // Retry a clear only after a session-related refusal and a positive session reply.
        if (n==-2 && (negative==0x22 || negative==0x31 || negative==0x7e || negative==0x7f)) {
            const unsigned char extended[]={0x10,0x03}, extended_reply[]={0x50,0x03};
            const unsigned char normal[]={0x10,0x01}, normal_reply[]={0x50,0x01};
            unsigned session_nrc;
            if (exchange(fd,0x12,extended,2,extended_reply,2,reply,sizeof reply,2000,&session_nrc)>=2) {
                n=exchange(fd,0x12,clear_request,4,clear_reply,1,reply,sizeof reply,8000,&negative);
                exchange(fd,0x12,normal,2,normal_reply,2,reply,sizeof reply,2000,&session_nrc);
            }
        }
        if (n!=1) {
            if (n==-2 && (negative==0x22 || negative==0x83 || negative==0x88))
                printf("Clear refused (NRC %02X). Try ignition on, engine off.\n",negative);
            else if (n==-2) printf("Clear rejected (NRC %02X)\n",negative);
            else puts("Clear result unknown: read codes again");
            return;
        }
    }
    n=exchange(fd,0x12,read_request,3,read_reply,2,reply,sizeof reply,1800,&negative);
    if (n==-2) { printf("Read rejected (NRC %02X)\n",negative); return; }
    if (n<3 || (n-3)%4!=0) { puts(clear ? "Clear acknowledged; recheck unavailable" : "No valid code response"); return; }
    supported_fault_status=reply[2] & fault_status;
    for (i=3;i<n;i+=4)
        if (reply[i+3] & supported_fault_status) ++count;
    printf("Engine: %d %s%s\n",count,
           count==1 ? "code" : "codes",clear ? " after clear" : "");
    for (i=3;i<n && shown<128;i+=4) {
        if (!(reply[i+3] & supported_fault_status)) continue;
        printf("%02X%02X%02X  status %02X\n",reply[i],reply[i+1],reply[i+2],reply[i+3]);
        ++shown;
    }
    if (count>128) puts("More codes returned; first 128 shown");
}

int main(int argc, char **argv) {
    const char *host=0;
    unsigned port=6801;
    int mode=0, lock, gw, i;
    for (i=1;i<argc;++i) {
        if (!strcmp(argv[i],"--host") && i+1<argc && !host) host=argv[++i];
        else if (!strcmp(argv[i],"--port") && i+1<argc) port=number(argv[++i],1,65535);
        else if (!strcmp(argv[i],"--codes") && !mode) mode=1;
        else if (!strcmp(argv[i],"--clear") && !mode) mode=2;
        else break;
    }
    if (i!=argc || !host || !mode) {
        fprintf(stderr,"Usage: %s --host IPv4 [--port 6801] (--codes | --clear)\n",argv[0]);
        return 2;
    }
    signal(SIGINT,stop); signal(SIGTERM,stop); signal(SIGPIPE,SIG_IGN);
    lock=reserve_gateway();
    if (mode==2) {
        const uint64_t deadline=bd_mono_ms()+650;
        while (running && lock<0 && errno==EADDRINUSE && bd_mono_ms()<deadline) {
            poll(0,0,25);
            lock=reserve_gateway();
        }
    }
    if (lock<0) {
        puts(errno==EADDRINUSE ? "Diagnostics busy. Wait a moment and try again."
                               : "Could not reserve the diagnostics connection");
        return 0;
    }
    gw=connect_gw(host,port);
    if(gw<0) printf("Gateway %s:%u unavailable (%s)\n",host,port,strerror(errno));
    else fault_codes(gw,mode==2);
    if (gw>=0) close(gw);
    close(lock);
    return 0;
}
#elif defined(MHD_MAPS_ONLY)
int main(int argc, char **argv) {
    const char *host=0;
    unsigned port=6801, function=0, value=0, negative;
    int fd, lock, n, i;
    unsigned char request[]={0x2f,0,0}, prefix[]={0x6f,0,0}, reply[16];
    const char *label = 0;
#ifdef PERF_LAB
    const char *flap[] = {"Exhaust OEM", "Exhaust open", "Exhaust closed"};
#endif
    for (i=1;i<argc;++i) {
        if (!strcmp(argv[i],"--host") && i+1<argc && !host) host=argv[++i];
        else if (!strcmp(argv[i],"--port") && i+1<argc) port=number(argv[++i],1,65535);
        else if (!strcmp(argv[i],"--map") && i+1<argc && !function) {
            function=1; value=number(argv[++i],1,4)-1;
        }
#ifdef PERF_LAB
        else if (!strcmp(argv[i],"--flap") && i+1<argc && !function) {
            function=2; value=number(argv[++i],0,2); label=flap[value];
        } else if (!strcmp(argv[i],"--cooling") && i+1<argc && !function) {
            function=3; value=number(argv[++i],0,1); label=value?"Max Cooling on":"Max Cooling off";
        }
#endif
        else return 2;
    }
    if (!host || !function) return 2;
    signal(SIGINT,stop); signal(SIGTERM,stop); signal(SIGPIPE,SIG_IGN);
    lock=reserve_gateway();
    if (lock<0) { puts("Diagnostics busy. Wait a moment and try again."); return 0; }
    fd=connect_gw(host,port);
    if (fd<0) {
        puts("Diagnostic gateway unavailable");
        close(lock);
        return 0;
    }
    /* MHD+ live controls documented in idrive7-apps at 9f0f68a.
       The matching echo acknowledges the request. */
#ifdef PERF_LAB
    if (function==3 && value) {
        const unsigned char rpm[]={0x01,0x0c}, rpm_reply[]={0x41,0x0c};
        n=exchange(fd,0x12,rpm,sizeof rpm,rpm_reply,sizeof rpm_reply,reply,sizeof reply,500,&negative);
        if (n!=4 || (((unsigned)reply[2]<<8)|reply[3])<=2000 ||
            (((unsigned)reply[2]<<8)|reply[3])>40000) {
            puts("Start the engine first. Max Cooling ON needs live RPM.");
            close(fd);
            close(lock);
            return 0;
        }
    }
#endif
    request[1]=prefix[1]=(unsigned char)function;
    request[2]=prefix[2]=(unsigned char)value;
    n=exchange(fd,0x12,request,sizeof request,prefix,sizeof prefix,reply,sizeof reply,1600,&negative);
    if (label) {
        if (n>=3) printf("Accepted by DME: %s\n",label);
        else if (n==-2) printf("%s refused by DME (NRC %02X)\n",label,negative);
        else printf("%s: no matching DME reply\n",label);
    } else if (n>=3) printf("Confirmed: Map %u\n",value+1);
    else if (n==-2) printf("Map %u refused by DME (NRC %02X)\n",value+1,negative);
    else puts("Map result unknown. Check Current Map in MHD.");
    close(fd);
    close(lock);
    return 0;
}
#elif defined(SHIFT_DECK_ONLY)
#include "drive_frame.h"

static int read_live_pid(int fd, unsigned char pid, int width, int *value) {
    unsigned char req[]={0x01,pid}, prefix[]={0x41,pid}, reply[4];
    unsigned negative;
    int n=exchange(fd,0x12,req,sizeof req,prefix,sizeof prefix,reply,sizeof reply,250,&negative);
    *value=-1;
    if (n==width+2) *value=width==1 ? reply[2] : ((unsigned)reply[2]*256+reply[3])/4;
    return n!=-1;
}

int main(int argc, char **argv) {
    const char *host=0;
    unsigned port=6801, udp_port=DRIVE_PORT, cycles=0, count=0;
    pid_t parent=getppid();
    struct sockaddr_in dest;
    int fd=-1, sock, lock=-1, i;
    uint64_t lease_until=0, retry_at=0;
#ifdef NATIVE_FAST
    uint64_t speed_due=0;
    int last_speed=-1;
#endif
    for (i=1;i<argc;++i) {
        if (!strcmp(argv[i],"--host") && i+1<argc && !host) host=argv[++i];
        else if (!strcmp(argv[i],"--port") && i+1<argc) port=number(argv[++i],1,65535);
        else if (!strcmp(argv[i],"--udp-port") && i+1<argc) udp_port=number(argv[++i],1,65535);
        else if (!strcmp(argv[i],"--cycles") && i+1<argc) cycles=number(argv[++i],1,1000);
        else return 2;
    }
    if (!host || parent<=1) return 2;
    signal(SIGINT,stop); signal(SIGTERM,stop); signal(SIGPIPE,SIG_IGN);
    sock=socket(AF_INET,SOCK_DGRAM,0);
    if (sock<0) return 1;
    memset(&dest,0,sizeof dest);
    dest.sin_family=AF_INET;
    dest.sin_port=htons((uint16_t)udp_port);
    inet_pton(AF_INET,"127.0.0.1",&dest.sin_addr);
    while (running && getppid()==parent && (!cycles || count<cycles)) {
        struct drive_frame f;
        int rpm=-1,kph=-1;
        uint64_t start=bd_mono_ms(), elapsed;
        if (fd<0 && start>=retry_at) {
            lock=reserve_gateway();
            if (lock>=0) {
                fd=connect_gw(host,port);
                if (fd<0) {
                    release_gateway(&fd,&lock);
                    retry_at=bd_mono_ms()+DIAGNOSTIC_RETRY_MS;
                } else lease_until=bd_mono_ms()+DIAGNOSTIC_LEASE_MS;
            }
        }
        if (fd>=0) {
            #ifdef NATIVE_FAST
            int ok=read_live_pid(fd,0x0c,2,&rpm);
            if (ok && start>=speed_due) {
                ok=read_live_pid(fd,0x0d,1,&last_speed);
                speed_due=start+65;
            }
            kph=last_speed;
#else
            int ok=read_live_pid(fd,0x0c,2,&rpm) && read_live_pid(fd,0x0d,1,&kph);
#endif
            if (!ok) {
                release_gateway(&fd,&lock);
                retry_at=bd_mono_ms()+DIAGNOSTIC_RETRY_MS;
            } else {
                f.sampled_ms=bd_mono_ms();
                f.rpm=rpm;
                f.kph=kph;
                if (sendto(sock,&f,sizeof f,0,(struct sockaddr *)&dest,sizeof dest)!=(ssize_t)sizeof f) break;
                if (gateway_should_yield(lock,f.sampled_ms,lease_until)) {
                    release_gateway(&fd,&lock);
                    retry_at=f.sampled_ms+DIAGNOSTIC_YIELD_MS;
                }
            }
        }
        ++count;
        elapsed=bd_mono_ms()-start;
#ifdef NATIVE_FAST
        if (running) poll(0,0,fd<0 ? 25 : elapsed<20 ? (int)(20-elapsed) : 2);
#else
        if (running) poll(0,0,elapsed<70 ? (int)(70-elapsed) : 10);
#endif
    }
    release_gateway(&fd,&lock);
    close(sock);
    return 0;
}
#else
static int trace_reads = 0;
static void pause_ms(int ms) { while (running && poll(0, 0, ms) < 0 && errno == EINTR) {} }

static int read_did(int fd, unsigned ecu, unsigned did, double *value) {
    unsigned char request[]={0x22,(unsigned char)(did>>8),(unsigned char)did};
    unsigned char prefix[]={0x62,request[1],request[2]};
    unsigned char reply[4094];
    unsigned negative, i;
    int n=exchange(fd,ecu,request,3,prefix,3,reply,sizeof reply,500,&negative);
    int decoded;
    if (n==-2) {
        fprintf(stderr,"ECU %02X DID %04X NRC %02X\n",ecu,did,negative);
        return 0;
    }
    if (n<0) return -1;
    decoded=bd_decode_did(did,reply+3,(size_t)n-3,value);
    if (trace_reads) {
        fprintf(stderr,"RAW mono_ms=%llu ecu=%02X did=%04X payload=",
                (unsigned long long)bd_mono_ms(),ecu,did);
        for (i=3;i<(unsigned)n;++i) fprintf(stderr,"%02X",reply[i]);
        if (decoded) fprintf(stderr," decoded=%.6f\n",*value);
        else fprintf(stderr," unavailable%s\n",did==0x4a49 && n==5 &&
                     reply[3]==255 && reply[4]==255 ?
                     " reason=ambiguous-FFFF" : " reason=invalid-or-out-of-range");
    }
    return decoded;
}

#if defined(PERF_LAB) && defined(BD_STOCK_DME)
static int read_obd(int fd,unsigned pid,double *value) {
    unsigned char request[]={1,(unsigned char)pid},prefix[]={0x41,(unsigned char)pid},reply[64];
    unsigned negative;
    int n=exchange(fd,0x12,request,2,prefix,2,reply,sizeof reply,500,&negative);
    if(n==-2)return 0;
    if(n<0)return -1;
    return bd_decode_obd(pid,reply+2,(size_t)n-2,value);
}
#endif

static int send_frame(int sock, struct sockaddr_in *dest, bd_frame *f, uint64_t *observed) {
    char packet[BD_MAX_PACKET];
    uint64_t now = bd_mono_ms();
    unsigned i;
    int len;
    f->sent_ms=now;
    for (i = 0; i < BD_COUNT; ++i) {
        uint64_t age = now - observed[i];
        f->age_ms[i] = age > 999999 ? 999999 : (unsigned)age;
    }
    len = bd_encode(f, packet, sizeof packet);
    return len > 0 && sendto(sock, packet, (size_t)len, 0,
        (struct sockaddr *)dest, sizeof *dest) == len;
}
int main(int argc, char **argv) {
    const char *host = 0;
    unsigned port = 6801, udp_port = BD_UDP_PORT, cycles = 0, count = 0, i;
    int demo = 0, sock, gw = -1, no_trans = 0, session = 0;
    int lock = -1;
    pid_t parent = getppid();
    struct sockaddr_in dest;
    bd_frame f;
    uint64_t observed[BD_COUNT] = {0};
    double ambient = 0;
    uint64_t ambient_time = 0;
    int have_ambient = 0;
#ifdef NATIVE_FAST
    const unsigned tick_ms = 35;
    const unsigned minimum_idle_ms = 3;
#else
    const unsigned tick_ms = 150;
    const unsigned minimum_idle_ms = 10;
#endif
    /* Each slow signal has its own deadline. Stagger temperature reads
     * after the first scan so they do not delay the fast gauges together. */
    const unsigned dids[] = {0x4201,0x4205,0x5889,0x4a49,0x580f,0xda12,0x56d7,0x586f,0x4300};
    const unsigned periods[] = {1000,0,0,0,2000,2000,0,0,2000};
    const unsigned first_delay[] = {900,0,0,0,500,1150,0,0,1700};
    uint64_t due[sizeof dids/sizeof dids[0]] = {0};
    uint64_t failed_until[sizeof dids/sizeof dids[0]] = {0};
    uint64_t lease_until=0, retry_at=0;
    int scanned=0;
#ifdef PERF_LAB
#ifdef BD_STOCK_DME
    uint64_t next_oil_temperature=0;
    int use_obd_rail=0;
#else
    int ethanol=0;
    uint64_t next_ethanol=0;
#endif
#endif
    for (i = 1; i < (unsigned)argc; ++i) {
        if (!strcmp(argv[i], "--demo")) demo = 1;
#if defined(PERF_LAB) && !defined(BD_STOCK_DME)
        else if (!strcmp(argv[i], "--ethanol")) ethanol = 1;
#endif
        else if (!strcmp(argv[i], "--session")) session = 1;
        else if (!strcmp(argv[i], "--trace")) trace_reads = 1;
        else if (!strcmp(argv[i], "--no-trans")) no_trans = 1;
        else if (!strcmp(argv[i], "--host") && i+1 < (unsigned)argc) host = argv[++i];
        else if (!strcmp(argv[i], "--port") && i+1 < (unsigned)argc) port = number(argv[++i],1,65535);
        else if (!strcmp(argv[i], "--udp-port") && i+1 < (unsigned)argc) udp_port = number(argv[++i],1,65535);
        else if (!strcmp(argv[i], "--cycles") && i+1 < (unsigned)argc) cycles = number(argv[++i],1,1000000);
        else { fprintf(stderr, "Usage: %s (--host IPv4 | --demo) [--port 6801] [--udp-port 42860] [--cycles N] [--no-trans] [--trace] [--session]\n", argv[0]); return 2; }
    }
    if ((!host && !demo) || (host && demo)) { fprintf(stderr, "Choose --host or --demo, never both.\n"); return 2; }
    signal(SIGINT, stop); signal(SIGTERM, stop); signal(SIGPIPE, SIG_IGN);
    if (session && (parent <= 1 || demo)) return 2;
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("UDP socket"); return 1; }
    memset(&dest,0,sizeof dest); dest.sin_family=AF_INET; dest.sin_port=htons((uint16_t)udp_port);
    inet_pton(AF_INET,"127.0.0.1",&dest.sin_addr); /* local process IPC only */
    memset(&f,0,sizeof f); f.demo=(unsigned)demo;
    fprintf(stderr, "%s: 2016 340i candidate profile; validate against ISTA/logger.\n",
        demo ? "DEMO / NO VEHICLE CONNECTION" : "READ ONLY / HSFZ");
    while (running && (!cycles || count < cycles) && (!session || getppid() == parent)) {
        uint64_t start = bd_mono_ms();
        f.seq++;
        if (demo) {
            double t = count * (tick_ms / 1000.0);
            f.value[0]=12+8*sin(t); f.value[1]=13.2+.6*sin(t);
            f.value[2]=8+3*sin(t); f.value[3]=44+2*sin(t); f.value[4]=88;
            f.value[BD_RAIL]=95+22*sin(t); f.value[BD_OIL]=2.5+.5*sin(t);
            f.value[BD_COOLANT]=86+3*sin(t);
#ifdef PERF_LAB
#ifdef BD_STOCK_DME
            f.value[BD_OIL_TEMP]=95;
#else
            f.value[BD_ETHANOL]=38;
#endif
#endif
            f.valid=(1u<<BD_COUNT)-1;
            for (i=0;i<BD_COUNT;++i) observed[i]=bd_mono_ms();
        } else {
            int full_scan = session ? !scanned : gw < 0;
            if (gw < 0 && start >= retry_at) {
                if (session) lock=reserve_gateway();
                if (!session || lock>=0) {
                    gw=connect_gw(host,port);
                    if (gw<0 && session) {
                        release_gateway(&gw,&lock);
                        retry_at=bd_mono_ms()+DIAGNOSTIC_RETRY_MS;
                    } else if (session) lease_until=bd_mono_ms()+DIAGNOSTIC_LEASE_MS;
                }
            }
            for (i=0; gw >= 0 && i < sizeof dids / sizeof dids[0] && running; ++i) {
                double v = 0;
                int rc;
                if (no_trans && dids[i] == 0xda12) continue;
                if (session && start < failed_until[i]) continue;
                if (!full_scan && periods[i] && start < due[i]) continue;
                if (full_scan) due[i]=start+first_delay[i];
                else if (periods[i]) {
                    due[i]+=periods[i];
                    if (due[i]<=start) due[i]=start+periods[i];
                }
                if (i != 0) f.valid &= ~(1u << (i-1));
#if defined(PERF_LAB) && defined(BD_STOCK_DME)
                if(dids[i]==0x4300)rc=read_obd(gw,5,&v);
                else if(dids[i]==0x56d7&&use_obd_rail)rc=read_obd(gw,0x23,&v);
                else {
                    rc=read_did(gw,dids[i]==0xda12?0x18:0x12,dids[i],&v);
                    if(dids[i]==0x56d7&&rc==0){rc=read_obd(gw,0x23,&v);if(rc>0)use_obd_rail=1;}
                }
#else
                rc = read_did(gw, dids[i] == 0xda12 ? 0x18 : 0x12, dids[i], &v);
#endif
                if (rc < 0) {
                    release_gateway(&gw,&lock);
                    if (session) {
                        failed_until[i]=bd_mono_ms()+2000;
                        retry_at=bd_mono_ms()+DIAGNOSTIC_RETRY_MS;
                    } else {
                        f.valid=0;
                        have_ambient=0;
                    }
                    break;
                }
                if (i == 0) {
                    have_ambient=rc != 0;
                    if (have_ambient) { ambient=v; ambient_time=bd_mono_ms(); }
                    continue;
                }
                if (!rc) continue;
                if (i == 1) {
                    if (!have_ambient || bd_mono_ms()-ambient_time > 1800) continue;
                    v = (v-ambient) * .145037738; /* kPa absolute - measured ambient -> gauge psi */
                }
                if (i == 2) v *= 14.7; /* measured lambda -> gasoline-equivalent AFR */
                f.value[i-1]=v; observed[i-1]=bd_mono_ms(); f.valid |= 1u << (i-1);
            }
#ifdef PERF_LAB
#ifdef BD_STOCK_DME
            if(gw>=0&&start>=next_oil_temperature&&running) {
                double temperature=0;
                int rc=read_obd(gw,0x5c,&temperature);
                f.valid&=~(1u<<BD_OIL_TEMP);
                if(rc>0){f.value[BD_OIL_TEMP]=temperature;observed[BD_OIL_TEMP]=bd_mono_ms();f.valid|=1u<<BD_OIL_TEMP;}
                next_oil_temperature=bd_mono_ms()+2000;
                if(rc<0){release_gateway(&gw,&lock);retry_at=bd_mono_ms()+DIAGNOSTIC_RETRY_MS;}
            }
#else
            if (gw>=0 && ethanol && start>=next_ethanol && running) {
                /* Published MHD+ MG1 monitor layout. This is the tune's active
                 * ethanol percentage, distinct from the raw ECA sensor. */
                const unsigned char query[]={0x2c,0x88,0x01}, prefix[]={0x6c,0x88};
                unsigned char reply[38];
                unsigned negative;
                const int n=exchange(gw,0x12,query,sizeof query,prefix,sizeof prefix,
                                     reply,sizeof reply,180,&negative);
                f.valid &= ~(1u<<BD_ETHANOL);
                if (n==38 && reply[17]<=100) {
                    observed[BD_ETHANOL]=bd_mono_ms();
                    f.value[BD_ETHANOL]=reply[17];
                    f.valid |= 1u<<BD_ETHANOL;
                    next_ethanol=observed[BD_ETHANOL]+5000;
                } else {
                    next_ethanol=bd_mono_ms()+30000;
                    if (n<0 && n!=-2) {
                        release_gateway(&gw,&lock);
                        retry_at=bd_mono_ms()+DIAGNOSTIC_RETRY_MS;
                    }
                }
            }
#endif
#endif
            if (gw>=0) {
                scanned=1;
                if (session && gateway_should_yield(lock,bd_mono_ms(),lease_until)) {
                    release_gateway(&gw,&lock);
                    retry_at=bd_mono_ms()+DIAGNOSTIC_YIELD_MS;
                }
            }
        }
        if (!send_frame(sock,&dest,&f,observed)) { perror("send frame"); running=0; }
        ++count;
        {
            uint64_t elapsed = bd_mono_ms()-start;
            unsigned interval = session && gw<0 ? 55 : tick_ms;
            unsigned idle_ms = elapsed < interval ? (unsigned)(interval-elapsed) : 0;
            if (!demo && idle_ms < minimum_idle_ms) idle_ms = minimum_idle_ms;
            if (idle_ms) pause_ms((int)idle_ms);
        }
    }
    release_gateway(&gw,&lock);
    close(sock);
    return 0;
}
#endif
