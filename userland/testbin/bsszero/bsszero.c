#include <stdio.h>
#include <err.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#ifndef PAGES
#define PAGES  512   /* 512 * 4KB = 2MB di BSS */
#endif

#define PGSZ 4096
static unsigned char big[PAGES * PGSZ];

int main(void) {
    size_t i, touched = 0;

    /* Verifica zero-fill: ogni byte della BSS deve partire a 0 */
    for (i = 0; i < sizeof(big); i++) {
        if (big[i] != 0) {
            errx(1, "BSS non zero a offset %lu (val=%u)",
                 (unsigned long)i, (unsigned)big[i]);
        }
    }
    printf("[bsszero] BSS zero-fill OK (%lu bytes)\n",
           (unsigned long)sizeof(big));

    /* Tocca una pagina ogni 4KB e scrivi un pattern */
    for (i = 0; i < sizeof(big); i += PGSZ) {
        big[i] = (unsigned char)((i/PGSZ) & 0xff);
        touched++;
    }
    printf("[bsszero] Scritte %lu pagine (%lu KB)\n",
           (unsigned long)touched, (unsigned long)(touched * 4u));

    /* Rileggi e verifica */
    for (i = 0; i < sizeof(big); i += PGSZ) {
        unsigned char ex = (unsigned char)((i/PGSZ) & 0xff);
        if (big[i] != ex) {
            errx(1, "Mismatch a pagina %lu (got=%u, exp=%u)",
                 (unsigned long)(i/PGSZ), (unsigned)big[i], (unsigned)ex);
        }
    }
    printf("[bsszero] Readback OK\n");
    return 0;
}
