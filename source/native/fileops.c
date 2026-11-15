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
        char value[128],extra;int count;FILE *file=fopen(argv[2],"r");
        if(!file){perror(argv[2]);return 1;}
        count=fscanf(file,"%127s %c",value,&extra);fclose(file);
        if(count!=1||strcmp(value,argv[3])){fprintf(stderr,"Text value mismatch: %s\n",argv[2]);return 1;}
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
