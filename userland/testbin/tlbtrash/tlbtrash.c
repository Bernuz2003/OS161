#include <stdio.h>
#include <err.h>
#include <stdint.h>

#ifndef PAGES
#define PAGES  256   /* 256 pagine = 1MB: > NUM_TLB(64) */
#endif
#define PGSZ 4096

static unsigned char arena[PAGES * PGSZ];

int main(void) {
    printf("[tlbthrash] tocco %d pagine (stride 4KB)...\n", PAGES);
    for (int pass=0; pass<3; pass++) {
        for (int i=0; i<PAGES; i++) {
            arena[(size_t)i*PGSZ] ^= (unsigned char)(pass + i);
        }
        printf("[tlbthrash] pass %d ok\n", pass);
    }
    printf("[tlbthrash] completato senza crash (TLB RR/Free/Replace test)\n");
    return 0;
}
