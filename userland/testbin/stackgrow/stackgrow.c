#include <stdio.h>
#include <stdint.h>

#define PGSZ  4096UL
#define DEPTH 16      /* 16 * 4KB = ~64KB, sotto il limite di 18 pagine (~72KB) */

static void touch(int depth, volatile unsigned char *accum) {
    volatile unsigned char buf[PGSZ];
    buf[0] = (unsigned char)depth;
    *accum += buf[0];
    if (depth > 0) touch(depth - 1, accum);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;             /* evita -Werror=unused-parameter */
    volatile unsigned char sum = 0;
    touch(DEPTH, &sum);
    printf("[stackgrow] ok, sum=%u\n", (unsigned)sum);
    return 0;
}
