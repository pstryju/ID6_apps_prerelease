// Data-source replacement only. hud_shape.cpp remains the upstream renderer.
#include "hud_sample.h"
#include "hud_feed.h"
#include "gauge_core.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>

// hud_shape.cpp's original signal handler also clears this diagnostic flag.
// No diagnostic transport is linked into the CDS-only worker.
extern "C" { volatile sig_atomic_t diag_running=1; }

extern "C" void hud_reader_init(hud_reader* reader) {
    memset(reader,0,sizeof *reader);
    reader->fd=reader->lock=-1;
}
extern "C" void hud_reader_close(hud_reader* reader) {
    if(reader->fd>=0) close(reader->fd);
    hud_reader_init(reader);
}
extern "C" int hud_reader_poll(hud_reader* reader,const char*,hud_sample* sample,int requirePark) {
    // This feed contains RPM, not a verified stationary/Park indication.
    if(requirePark) return -1;
    if(reader->fd<0) {
        const uint64_t now=bd_mono_ms();
        if(now<reader->retry_at) return 0;
        reader->retry_at=now+1000;
        int fd=socket(AF_INET,SOCK_DGRAM,0);
        if(fd<0) return 0;
        sockaddr_in address;memset(&address,0,sizeof address);
        address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        address.sin_port=htons(HUD_FEED_PORT);
        if(fcntl(fd,F_SETFL,O_NONBLOCK)<0||fcntl(fd,F_SETFD,FD_CLOEXEC)<0||
           bind(fd,reinterpret_cast<sockaddr*>(&address),sizeof address)<0) {close(fd);return 0;}
        reader->fd=fd;
    }
    int result=0;
    for(unsigned i=0;i<8;++i) {
        hud_feed feed;
        const ssize_t length=recv(reader->fd,&feed,sizeof feed,MSG_TRUNC);
        if(length<0) break;
        if(length!=sizeof feed||!hud_feed_valid(feed,bd_mono_ms())) continue;
        if(feed.rpm<0) {result=-1;continue;}
        memset(sample,0,sizeof *sample);
        sample->rpm=static_cast<unsigned>(feed.rpm);
        sample->rpm_ms=feed.sent_ms;
        sample->gear='-';
        // No boost is invented or read through HSFZ. The unchanged UI shows --.
        result=1;
    }
    return result;
}
extern "C" int hud_read(const char* host,hud_sample* sample,int requirePark) {
    hud_reader reader;hud_reader_init(&reader);
    const int result=hud_reader_poll(&reader,host,sample,requirePark);
    hud_reader_close(&reader);return result==1;
}
