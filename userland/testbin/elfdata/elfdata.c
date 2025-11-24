/* elfdata.c
 *
 * OBIETTIVO: Verificare il caricamento on-demand del segmento DATA (variabili inizializzate).
 *
 * COMPORTAMENTO ATTESO:
 * - SUCCESSO: Stampa i valori corretti (0xA1, 0xB2...) e termina con "write su .data OK".
 * - FALLIMENTO: Se i valori sono 0 o errati, c'è un errore in vm_fault nella gestione
 * di SEG_BACK_FILE o nel calcolo dell'offset nel file ELF.
 */

#include <stdio.h>
#include <err.h>
#include <stdint.h>

#define PGSZ 4096
#define PAGES 8

/* * Variabile .data inizializzata. 
 * Questa array ESISTE fisicamente nel file eseguibile elfdata.
 * Il kernel deve fare VOP_READ per caricarla, non bzero.
 */
static unsigned char initdata[PGSZ*PAGES] = {
    [0*PGSZ] = 0xA1, [1*PGSZ] = 0xB2, [2*PGSZ] = 0xC3, [3*PGSZ] = 0xD4,
    [4*PGSZ] = 0xE5, [5*PGSZ] = 0xF6, [6*PGSZ] = 0x17, [7*PGSZ] = 0x28
};

int main(void) {
    /* Verifica lettura: forza il caricamento da disco pagina per pagina */
    for (int i=0; i<PAGES; i++) {
        unsigned off = (unsigned)(i*PGSZ);
        unsigned char v = initdata[off];
        printf("[elfdata] initdata[%u] = 0x%02x\n", off, v);
    }
    
    /* * Verifica scrittura: il segmento .data è Read-Write (RW).
     * Non deve generare eccezioni "TLB Modify" o "ReadOnly".
     * Se crasha qui, hai impostato i permessi sbagliati (RO invece di RW).
     */
    initdata[0] ^= 0xff;
    printf("[elfdata] write su .data OK\n");
    return 0;
}