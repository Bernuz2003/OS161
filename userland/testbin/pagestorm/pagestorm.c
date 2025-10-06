#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <err.h>

#define PGSZ 4096
#ifndef PAGES
#define PAGES  1024
#endif
static unsigned char arena[PAGES * PGSZ];

int main(int argc, char **argv) {
    int loops = (argc > 1) ? atoi(argv[1]) : 1;
    printf("[pagestorm] PAGES=%d, loops=%d\n", PAGES, loops);
    for (int l=0; l<loops; l++) {
        for (int i=0; i<PAGES; i++) {
            arena[(size_t)i*PGSZ] ^= (unsigned char)(l+i);
        }
        printf("[pagestorm] loop %d ok\n", l);
    }
    return 0;
}
