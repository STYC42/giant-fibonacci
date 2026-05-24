#define _POSIX_C_SOURCE 200809L
#include "fib.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>


/* Ten 63-bit primes for modular verification */
static const uint64_t PRIMES[] = {
    9223372036854775783ULL,  /* 2^63 - 25  */
    9223372036854775643ULL,
    9223372036854774899ULL,
    9223372036854774847ULL,
    9223372036854774787ULL,
    9223372036854774629ULL,
    9223372036854774589ULL,
    9223372036854774523ULL,
    9223372036854774511ULL,
    9223372036854774421ULL,
};
#define NPRIMES ((int)(sizeof(PRIMES) / sizeof(PRIMES[0])))

static double elapsed(struct timespec t0)
{
    struct timespec t1;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    return (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;
}

int main(int argc, char *argv[])
{
    uint64_t n = 300000000000ULL; /* F_{3×10^11} by default */
    if (argc == 2) n = (uint64_t)strtoull(argv[1], NULL, 10);

    fprintf(stderr, "Computing F_%llu\n", (unsigned long long)n);
    spill_init();

    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    spilled_t fn, fn1;
    fib_double((mp_limb_t)n, &fn, &fn1);
    fprintf(stderr, "Doubling done in %.1f s\n", elapsed(t0));

    /* Discard F_{n+1} */
    spill_free(&fn1);

    /* Verify */
    fprintf(stderr, "Verifying modulo %d primes...\n", NPRIMES);
    if (!verify_result(&fn, n, PRIMES, NPRIMES)) {
        fprintf(stderr, "VERIFICATION FAILED\n");
        spill_free(&fn);
        return EXIT_FAILURE;
    }
    fprintf(stderr, "All checks passed. Total time: %.1f s\n", elapsed(t0));

    /* Optional: write decimal digits to stdout */
    if (argc >= 3 && argv[2][0] == '-' && argv[2][1] == 'o') {
        fprintf(stderr, "Converting to decimal...\n");
        mp_limb_t *lx = spill_reload(&fn);
        mp_size_t sx = fn.size;
        while (sx > 0 && lx[sx-1] == 0) sx--;

        /* mpz wrapper for output */
        mpz_t z;
        mpz_init(z);
        mpz_import(z, (size_t)sx, -1, sizeof(mp_limb_t), 0, 0, lx);
        free(lx);
        gmp_printf("%Zd\n", z);
        mpz_clear(z);
    }

    spill_free(&fn);
    return EXIT_SUCCESS;
}
