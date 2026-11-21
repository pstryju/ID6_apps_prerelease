#include "hud_sample.h"
#include "hud_feed.h"
#include "gauge_core.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>

int main() {
    hud_reader reader;hud_reader_init(&reader);hud_sample sample;
    assert(hud_reader_poll(&reader,"169.254.20.21",&sample,0)==0);
    int fd=socket(AF_INET,SOCK_DGRAM,0);assert(fd>=0);
    sockaddr_in address;memset(&address,0,sizeof address);
    address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    address.sin_port=htons(HUD_FEED_PORT);
    hud_feed feed;memset(&feed,0,sizeof feed);memcpy(feed.magic,"HUD1",4);
    feed.sent_ms=bd_mono_ms();feed.rpm=703;
    assert(sendto(fd,&feed,sizeof feed,0,reinterpret_cast<sockaddr*>(&address),sizeof address)==sizeof feed);
    assert(hud_reader_poll(&reader,"unused",&sample,0)==1);
    assert(sample.rpm==703 && sample.rpm_ms==feed.sent_ms && !sample.have_boost);
    assert(hud_reader_poll(&reader,"unused",&sample,0)==0); // no fabricated freshness
    feed.sent_ms=bd_mono_ms()-1100;
    sendto(fd,&feed,sizeof feed,0,reinterpret_cast<sockaddr*>(&address),sizeof address);
    assert(hud_reader_poll(&reader,"unused",&sample,0)==0);
    feed.sent_ms=bd_mono_ms();feed.rpm=-1;
    sendto(fd,&feed,sizeof feed,0,reinterpret_cast<sockaddr*>(&address),sizeof address);
    assert(hud_reader_poll(&reader,"unused",&sample,0)==-1);
    feed.rpm=3500;feed.sent_ms=bd_mono_ms();
    sendto(fd,&feed,sizeof feed,0,reinterpret_cast<sockaddr*>(&address),sizeof address);
    assert(hud_reader_poll(&reader,"unused",&sample,1)==-1); // RPM alone never proves Park
    assert(hud_reader_poll(&reader,"unused",&sample,0)==1 && sample.rpm==3500);
    char oversized[sizeof feed+1];memcpy(oversized,&feed,sizeof feed);
    sendto(fd,oversized,sizeof oversized,0,reinterpret_cast<sockaddr*>(&address),sizeof address);
    assert(hud_reader_poll(&reader,"unused",&sample,0)==0);
    close(fd);hud_reader_close(&reader);assert(reader.fd==-1);
    puts("HUD CDS reader: RPM, missing/expired data, no boost and no guessed Park passed.");
}
