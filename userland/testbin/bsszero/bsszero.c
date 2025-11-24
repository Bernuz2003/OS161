/* bsszero.c
 *
 * OBIETTIVO: Verificare che le pagine della sezione BSS (variabili globali non inizializzate)
 * siano riempite di zeri al primo accesso (Zero-Fill-On-Demand).
 *
 * COMPORTAMENTO ATTESO:
 * - SUCCESSO: Il programma termina stampando "Readback OK".
 * - FALLIMENTO: Se stampa "BSS non zero...", la tua vm_fault non sta facendo bzero()
 * sulle pagine nuove allocate.
 */

#include <stdio.h>
#include <err.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#ifndef PAGES
#define PAGES  512   /* 512 pagine * 4KB = 2MB di BSS. */
#endif

#define PGSZ 4096

/*
 * Questa è una variabile globale statica enorme. Non è nel file ELF (risparmia spazio su disco).
 * Il kernel deve allocare pagine fisiche e azzerarle solo quando le tocchiamo.
 */
static unsigned char big[PAGES * PGSZ];

int main(void) {
    size_t i, touched = 0;

    /* * PASSO 1: Verifica Zero-Fill.
     * Leggiamo l'array. Al primo accesso di lettura a una pagina, si genera un TLB Miss.
     * La tua vm_fault deve:
     * 1. Allocare un frame fisico.
     * 2. Eseguire bzero() sul frame.
     * 3. Aggiornare la Page Table e il TLB.
     */
    for (i = 0; i < sizeof(big); i++) {
        if (big[i] != 0) {
            errx(1, "BSS non zero a offset %lu (val=%u) -> ERRORE: Memoria sporca!",
                 (unsigned long)i, (unsigned)big[i]);
        }
    }
    printf("[bsszero] BSS zero-fill OK (%lu bytes)\n",
           (unsigned long)sizeof(big));

    /* * PASSO 2: Scrittura.
     * Scriviamo un pattern noto per verificare che la memoria sia persistente
     * e che non stiamo scrivendo sulla stessa pagina fisica mappata più volte per errore.
     */
    for (i = 0; i < sizeof(big); i += PGSZ) {
        big[i] = (unsigned char)((i/PGSZ) & 0xff);
        touched++;
    }
    printf("[bsszero] Scritte %lu pagine (%lu KB)\n",
           (unsigned long)touched, (unsigned long)(touched * 4u));

    /* * PASSO 3: Rilettura (Readback).
     * Verifica che i dati scritti siano ancora lì. 
     * Se la RAM è poca (es. 512KB), questo ciclo potrebbe forzare degli SWAP-OUT e SWAP-IN.
     */
    for (i = 0; i < sizeof(big); i += PGSZ) {
        unsigned char ex = (unsigned char)((i/PGSZ) & 0xff);
        if (big[i] != ex) {
            errx(1, "Mismatch a pagina %lu (got=%u, exp=%u) -> ERRORE Swap o Aliasing!",
                 (unsigned long)(i/PGSZ), (unsigned)big[i], (unsigned)ex);
        }
    }
    printf("[bsszero] Readback OK\n");
    return 0;
}