/* swapstress.c
 *
 * OBIETTIVO: Riempire completamente la RAM e il file di SWAP (9MB).
 *
 * COMPORTAMENTO ATTESO:
 * - SUCCESSO DEL TEST = PANIC DEL KERNEL.
 */

#include <stdio.h>
#include <stdlib.h> // Aggiunto per malloc
#include <err.h>
#include <stdint.h>

#ifndef PAGES
#define PAGES 4096 /* 16MB totali (4096 * 4KB) */
#endif
#define PGSZ 4096

static unsigned char arena[PAGES * PGSZ];

int main(void)
{
    unsigned int i;

    printf("[swapstress] primo sweep: write per pagina (%u KB totali)\n",
           (unsigned)(PAGES * 4));
    
    /* * Scrive su 16MB di memoria virtuale.
     * A un certo punto (circa dopo RAM size + 9MB = 13MB), lo swap finirà.
     * Qui ci aspettiamo il PANIC del kernel.
     */
    for (i = 0; i < PAGES; i++)
    {
        arena[i * PGSZ] = (unsigned char)(i & 0xff);
        if ((i & 0xFF) == 0)
            printf(".");
    }

    /* Se arriviamo qui, abbiamo allocato 16MB senza crashare */
    printf("\n[swapstress] secondo sweep: rilettura (swap-in atteso)\n");
    size_t mism = 0;
    for (i = 0; i < PAGES; i++)
    {
        unsigned char ex = (unsigned char)(i & 0xff);
        if (arena[i * PGSZ] != ex)
            mism++;
        if ((i & 0xFF) == 0)
            printf(".");
    }
    printf("\n[swapstress] mismatch=%lu (atteso 0). Done.\n",
           (unsigned long)mism);

    return mism ? 1 : 0;
}