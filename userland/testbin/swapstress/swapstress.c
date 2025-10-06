#include <stdio.h>
#include <err.h>
#include <stdint.h>

#ifndef PAGES
#define PAGES 4096 /* 16MB: calibra in base alla RAM */
#endif
#define PGSZ 4096

static unsigned char arena[PAGES * PGSZ];

int main(void)
{
    size_t i;

    printf("[swapstress] primo sweep: write per pagina (%u KB totali)\n",
           (unsigned)(PAGES * 4));
    for (i = 0; i < PAGES; i++)
    {
        arena[i * PGSZ] = (unsigned char)(i & 0xff);
        if ((i & 0xFF) == 0)
            printf(".");
    }
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
