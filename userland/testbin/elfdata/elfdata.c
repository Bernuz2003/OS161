#include <stdio.h>
#include <err.h>
#include <stdint.h>

#define PGSZ 4096
#define PAGES 8
/* .data inizializzata: ogni pagina ha un marker al primo byte */
static unsigned char initdata[PGSZ*PAGES] = {
    [0*PGSZ] = 0xA1, [1*PGSZ] = 0xB2, [2*PGSZ] = 0xC3, [3*PGSZ] = 0xD4,
    [4*PGSZ] = 0xE5, [5*PGSZ] = 0xF6, [6*PGSZ] = 0x17, [7*PGSZ] = 0x28
};

int main(void) {
    for (int i=0; i<PAGES; i++) {
        unsigned off = (unsigned)(i*PGSZ);
        unsigned char v = initdata[off];
        printf("[elfdata] initdata[%u] = 0x%02x\n", off, v);
    }
    /* scrittura legale su .data */
    initdata[0] ^= 0xff;
    printf("[elfdata] write su .data OK\n");
    return 0;
}
