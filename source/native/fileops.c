#define _POSIX_C_SOURCE 200112L
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "fileops_bundle.h"

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "check") == 0)
        return 0;
    if (argc == 3 && strcmp(argv[1], "receive") == 0) return evo_receive(argv[2]);
    if (argc == 4 && strcmp(argv[1], "verify") == 0) return evo_verify(argv[2],argv[3]);
    if (argc == 4 && strcmp(argv[1], "text") == 0) {
        unsigned char value[128];size_t n,start=0,end,i;int extra,error;
        FILE *file=fopen(argv[2],"rb");
        if(!file){perror(argv[2]);return 1;}
        /* Read bytes instead of relying on the target libc's multi-conversion
         * scanf/EOF behavior. Only surrounding ASCII whitespace is ignored. */
        n=fread(value,1,sizeof value,file);extra=fgetc(file);error=ferror(file);fclose(file);
        if(error){fprintf(stderr,"Could not read text: %s\n",argv[2]);return 1;}
        end=n;
        while(start<end&&(value[start]==' '||(value[start]>=9&&value[start]<=13)))++start;
        while(end>start&&(value[end-1]==' '||(value[end-1]>=9&&value[end-1]<=13)))--end;
        if(extra!=EOF||end-start!=strlen(argv[3])||memcmp(value+start,argv[3],end-start)){
            fprintf(stderr,"Text value mismatch: %s; expected '%s'; actual bytes (%lu%s):",
                    argv[2],argv[3],(unsigned long)n,extra==EOF?"":"+");
            for(i=0;i<n&&i<32;++i)fprintf(stderr," %02X",(unsigned)value[i]);
            fputc('\n',stderr);return 1;
        }
        return 0;
    }

    if (argc == 3 && strcmp(argv[1], "unlock") == 0) {
        if (rmdir(argv[2]) == 0)
            return 0;
        perror(argv[2]);
        return 1;
    }

    if (argc == 4 && strcmp(argv[1], "move") == 0) {
        struct stat destination;
        if (lstat(argv[3], &destination) == 0) {
            fprintf(stderr, "Destination already exists: %s\n", argv[3]);
            return 1;
        }
        if (errno != ENOENT) {
            perror(argv[3]);
            return 1;
        }
        if (rename(argv[2], argv[3]) == 0)
            return 0;
        fprintf(stderr, "Could not rename %s to %s: %s\n",
                argv[2], argv[3], strerror(errno));
        return 1;
    }

    fputs("Usage: gaugedeck-fileops check | unlock DIR | move SOURCE DEST | receive DIR | verify FILE SHA256\n", stderr);
    return 2;
}
