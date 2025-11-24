/* pagestorm.c
 *
 * OBIETTIVO: Generare un alto carico di Page Faults e accessi in memoria.
 *
 * COMPORTAMENTO ATTESO:
 * - SU 512KB/4MB RAM: RISCHIO CRASH. 
 * Allocare 4MB (PAGES=1024) su 4MB di RAM fisica porta alla saturazione completa.
 * Il kernel potrebbe non avere spazio per allocare le strutture di gestione (Page Tables).
 * - SU 8MB+ RAM: SUCCESSO.
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
     * Se la memoria fisica è piena, questo ciclo costringe il sistema a fare 
     * continuo swap-out e swap-in (thrashing) se l'algoritmo di rimpiazzo non è buono,
     * o semplicemente a riempire lo swapfile.
     */
    for (int l=0; l<loops; l++) {
        for (int i=0; i<PAGES; i++) {
            /* Scrittura XOR: richiede lettura + scrittura (RMW) */
            arena[(size_t)i*PGSZ] ^= (unsigned char)(l+i);
        }
        printf("[pagestorm] loop %d ok\n", l);
    }
    return 0;
}