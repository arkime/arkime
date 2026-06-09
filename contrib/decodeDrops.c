/* Use this tool to decode the /tmp/<node>.tcp.drops.[46] files */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char *argv[]) 
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <drop-file>\n", argv[0]);
        exit(1);
    }

    FILE *fp = fopen(argv[1], "r");

    if (!fp) {
        printf("Couldn't open %s\n", argv[1]);
        exit (0);
    }

    int ver, cnt, i;
    char isIp4;

    if (fread(&ver, 4, 1, fp) != 1) {
        fprintf(stderr, "Failed to read version\n");
        exit(1);
    }
    printf ("Version: %d\n", ver);
    if (ver == 1) {
        if (fread(&cnt, 4, 1, fp) != 1) {
            fprintf(stderr, "Failed to read count\n");
            exit(1);
        }
        printf ("Count: %d\n", cnt);
        if (cnt < 0 || cnt > 10000000) {
            fprintf(stderr, "Implausible count %d; aborting\n", cnt);
            exit(1);
        }
        
        unsigned short port;
        unsigned char  key[16];
        unsigned int   expire;
        unsigned short flags;
        
        for (i = 0; i < cnt; i++) {
            if (fread(&port, 2, 1, fp) != 1 ||
                fread(key, 4, 1, fp) != 1 ||
                fread(&expire, 4, 1, fp) != 1 ||
                fread(&flags, 2, 1, fp) != 1) {
                fprintf(stderr, "Truncated record at index %d\n", i);
                exit(1);
            }

            time_t texpire = expire;
            printf("%24.24s %d.%d.%d.%d:%d\n", ctime(&texpire), key[0], key[1], key[2], key[3], htons(port));
        }
    } else if (ver == 2) {
        if (fread(&isIp4, 1, 1, fp) != 1 ||
            fread(&cnt, 4, 1, fp) != 1) {
            fprintf(stderr, "Failed to read header\n");
            exit(1);
        }
        printf ("isIp4: %d\n", isIp4);
        printf ("Count: %d\n", cnt);
        if (cnt < 0 || cnt > 10000000) {
            fprintf(stderr, "Implausible count %d; aborting\n", cnt);
            exit(1);
        }
        
        unsigned short port;
        unsigned char  key[16];
        unsigned int   last;
        unsigned int   goodFor;
        unsigned short flags;
        
        for (i = 0; i < cnt; i++) {
            if (fread(&port, 2, 1, fp) != 1 ||
                fread(key, (isIp4?4:16), 1, fp) != 1 ||
                fread(&last, 4, 1, fp) != 1 ||
                fread(&goodFor, 4, 1, fp) != 1 ||
                fread(&flags, 2, 1, fp) != 1) {
                fprintf(stderr, "Truncated record at index %d\n", i);
                exit(1);
            }

            time_t texpire = last + goodFor;
            printf("%24.24s %d.%d.%d.%d:%d\n", ctime(&texpire), key[0], key[1], key[2], key[3], htons(port));
        }
    }
}
