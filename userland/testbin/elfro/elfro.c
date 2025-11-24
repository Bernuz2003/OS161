/* elfro.c
 *
 * OBIETTIVO: Testare un segmento Read-Only (TEXT/RODATA) grande (2MB).
 *
 * COMPORTAMENTO ATTESO:
 * - SU CONFIG 512KB RAM: MOLTO PROBABILE CRASH (Kernel Starvation).
 * Il working set (2MB) è > RAM (512KB). Il kernel prova a caricare pagine RO. 
 * Deve scartarne altre. Se la RAM si riempie completamente di pagine utente, 
 * kmalloc() fallisce nel creare nuove Page Table entries -> Panic/Kill.
 * - SU CONFIG > 2MB RAM: SUCCESSO. Il test dovrebbe finire calcolando la checksum.
 */

#include <stdio.h>
#include <stdint.h>

#define KB(x) ((x) * 1024UL)
#define MB(x) (KB(x) * 1024UL)
#define PGSZ 4096UL
#define ROBYTES MB(2) /* 2 MiB di rodata: ~512 pagine */

/* 
 * 'const' mette questo array nel segmento .rodata (Read-Only).
 * 'static' evita che vada nello stack.
 * Inizializzato a 0xAA per occupare spazio nel file ELF.
 */
static const unsigned char rohuge[ROBYTES] = {[0 ... ROBYTES - 1] = 0xAA};

int main(void)
{
    volatile const unsigned char *p = &rohuge[0]; /* volatile forza la lettura effettiva */
    unsigned long pages = ROBYTES / PGSZ;
    unsigned long sum = 0;

    /* * Legge un byte per pagina.
     * Poiché l'array è 2MB:
     * - Se RAM < 2MB: Il kernel dovrà fare "eviction".
     * Essendo pagine FILE-BACKED e READ-ONLY, il tuo kernel dovrebbe fare "Drop-in-place"
     * (scartarle senza scrivere nello swap, ricaricandole dal file ELF se servono di nuovo).
     */
    for (unsigned long i = 0; i < ROBYTES; i += PGSZ)
    {
        sum += p[i];
    }

    printf("[elfro] lette %lu pagine da rodata; checksum=%lu\n",
           pages, sum);
    return 0;
}