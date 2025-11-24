/* stackgrow.c
 *
 * OBIETTIVO: Verificare che lo stack utente possa crescere oltre i 4KB iniziali.
 *
 * COMPORTAMENTO ATTESO:
 * - SUCCESSO: Stampa "ok, sum=...". Significa che vm_fault riconosce gli indirizzi
 * dello stack (solitamente region stack base - size) e alloca pagine on-demand.
 */

#include <stdio.h>
#include <stdint.h>

#define PGSZ  4096UL
/* * Profondità 16 * 4KB = 64KB.
 * Questo è sicuro perché OS161 di solito definisce lo stack utente max ~18 pagine (72KB) o più.
 * Se aumenti DEPTH troppo, potresti colpire il limite dello stack (VM_STACKPAGES).
 */
#define DEPTH 16      

/* Funzione ricorsiva per consumare stack */
static void touch(int depth, volatile unsigned char *accum) {
    /* Alloca 4KB sullo stack locale in ogni frame della funzione */
    volatile unsigned char buf[PGSZ]; 
    
    buf[0] = (unsigned char)depth;
    *accum += buf[0];
    
    /* Scendi di un altro livello */
    if (depth > 0) touch(depth - 1, accum);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;             
    volatile unsigned char sum = 0;
    
    /* Inizia la discesa ricorsiva */
    touch(DEPTH, &sum);
    
    printf("[stackgrow] ok, sum=%u\n", (unsigned)sum);
    return 0;
}