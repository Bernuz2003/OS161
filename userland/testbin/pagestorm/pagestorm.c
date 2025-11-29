/* pagestorm.c
 *
 * OBIETTIVO: Generare un alto carico di Page Faults e accessi in memoria.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <err.h>

#define PGSZ 4096
#ifndef PAGES
#define PAGES  1024 /* 4MB di array */
#endif

/* Array globale: risiede in BSS */
static unsigned char arena[PAGES * PGSZ];

int main(int argc, char **argv) {
    int loops = (argc > 1) ? atoi(argv[1]) : 1;
    printf("[pagestorm] PAGES=%d, loops=%d\n", PAGES, loops);
    
    /*
     * Ciclo di scrittura intensiva.
     * Tocca ogni singola pagina dell'array.
     */
    for (int l=0; l<loops; l++) {
        for (int i=0; i<PAGES; i++) {
            /* XOR, richiede lettura + scrittura */
            arena[(size_t)i*PGSZ] ^= (unsigned char)(l+i);
        }
        printf("[pagestorm] loop %d ok\n", l);
    }
    return 0;
}