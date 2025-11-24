/* tlbtrash.c
 *
 * OBIETTIVO: Accedere a più pagine di quante ne stiano nel TLB hardware (64 entry su MIPS).
 *
 * COMPORTAMENTO ATTESO:
 * - SUCCESSO: Il programma completa i 3 passaggi senza errori.
 * Significa che vm_fault gestisce correttamente i TLB Miss rimpiazzando le vecchie entry
 * (usando tlb_random() o l'algoritmo Round Robin implementato).
 */

#include <stdio.h>
#include <err.h>
#include <stdint.h>

#ifndef PAGES
#define PAGES  256   /* 256 pagine = 1MB. TLB ha solo 64 entry. */
#endif
#define PGSZ 4096

static unsigned char arena[PAGES * PGSZ];

int main(void) {
    printf("[tlbthrash] tocco %d pagine (stride 4KB)...\n", PAGES);
    
    /* * Accesso sequenziale a 256 pagine.
     * Dopo le prime 64, il TLB è pieno. Le successive devono causare eviction dal TLB.
     * I passaggi successivi verificano che le entry evictate vengano ricaricate.
     */
    for (int pass=0; pass<3; pass++) {
        for (int i=0; i<PAGES; i++) {
            arena[(size_t)i*PGSZ] ^= (unsigned char)(pass + i);
        }
        printf("[tlbthrash] pass %d ok\n", pass);
    }
    printf("[tlbthrash] completato senza crash (TLB RR/Free/Replace test)\n");
    return 0;
}