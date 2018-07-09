/* Use this tool to decode the /tmp/<node>.tcp.drops.[46] files */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char *argv[]) 
{
    FILE *fp = fopen(argv[1], "r");

    if (!fp) {
        printf("Couldn't open %s\n", argv[1]);
        exit (0);
    }

    int ver, cnt, i;
    char isIp4;

    fread(&ver, 4, 1, fp);
    printf ("Version: %d\n", ver);
    if (ver == 1) {
        fread(&cnt, 4, 1, fp);
        printf ("Count: %d\n", cnt);
        
        unsigned short port;
        unsigned char  key[16];
        unsigned int   expire;
        unsigned short flags;
        
        for (i = 0; i < cnt; i++) {
            fread(&port, 2, 1, fp);
            fread(key, 4, 1, fp);
            fread(&expire, 4, 1, fp);
            fread(&flags, 2, 1, fp);

            time_t texpire = expire;
            printf("%24.24s %d.%d.%d.%d:%d\n", ctime(&texpire), key[0], key[1], key[2], key[3], htons(port));
        }
    } else if (ver == 2) {
        fread(&isIp4, 1, 1, fp);
        fread(&cnt, 4, 1, fp);
        printf ("isIp4: %d\n", isIp4);
        printf ("Count: %d\n", cnt);
        
        unsigned short port;
        unsigned char  key[16];
        unsigned int   last;
        unsigned int   goodFor;
        unsigned short flags;
        
        for (i = 0; i < cnt; i++) {
            fread(&port, 2, 1, fp);
            fread(key, (isIp4?4:16), 1, fp);
            fread(&last, 4, 1, fp);
            fread(&goodFor, 4, 1, fp);
            fread(&flags, 2, 1, fp);

            time_t texpire = last + goodFor;
            printf("%24.24s %d.%d.%d.%d:%d\n", ctime(&texpire), key[0], key[1], key[2], key[3], htons(port));
        }
    }
}
