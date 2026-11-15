#ifndef GATEWAY_CONFIG_H
#define GATEWAY_CONFIG_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

/* Per-app configuration beside the worker; the upstream reference is preserved
 * only when no configuration file exists. Malformed files never select a host. */
static inline int bd_load_gateway(const char *worker,char output[16]) {
    char path[768],value[64],extra;
    const char *slash=strrchr(worker,'/');
    FILE *file;struct in_addr address;int fields;
    if(!slash)return 0;
    if(snprintf(path,sizeof path,"%.*s/gateway-ip.txt",(int)(slash-worker),worker)>=(int)sizeof path)return 0;
    file=fopen(path,"r");
    if(!file){if(errno!=ENOENT)return 0;strcpy(output,"169.254.21.3");return 1;}
    fields=fscanf(file,"%63s %c",value,&extra);
    fclose(file);
    if(fields!=1||strlen(value)>15||inet_pton(AF_INET,value,&address)!=1)return 0;
    strcpy(output,value);return 1;
}
#endif
