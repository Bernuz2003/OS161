#include <stdio.h>
#include <stdint.h>

#define KB(x) ((x) * 1024UL)
#define MB(x) (KB(x) * 1024UL)
#define PGSZ 4096UL
#define ROBYTES MB(2) /* 2 MiB di rodata: ~512 pagine */

static const unsigned char rohuge[ROBYTES] = {[0 ... ROBYTES - 1] = 0xAA};

int main(void)
{
    volatile const unsigned char *p = &rohuge[0]; /* <-- conserva 'const' */
    unsigned long pages = ROBYTES / PGSZ;
    unsigned long sum = 0;

    /* leggi una byte per pagina per forzare page-in dal segmento RO */
    for (unsigned long i = 0; i < ROBYTES; i += PGSZ)
    {
        sum += p[i];
    }

    printf("[elfro] lette %lu pagine da rodata; checksum=%lu\n",
           pages, sum);
    return 0;
}
