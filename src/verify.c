#include "fib.h"
#include <stdint.h>

/*
 * Fast doubling mod p, entirely in uint64_t arithmetic.
 * p must be < 2^63 so that intermediate sums cannot overflow.
 *
 * Returns F_n mod p.
 */
uint64_t fib_mod(uint64_t n, uint64_t p)
{
    if (n == 0) return 0;

    /* Find the highest bit of n */
    int bits = 63 - __builtin_clzll(n);

    uint64_t a = 0; /* F_k */
    uint64_t b = 1; /* F_{k+1} */

    for (int i = bits; i >= 0; i--) {
        /*
         * c = a * (2b - a) mod p
         * d = a^2 + b^2   mod p
         */
        uint64_t two_b = (b >= p - b) ? (2 * b - p) : (2 * b);
        uint64_t two_b_minus_a = (two_b >= a) ? (two_b - a) : (two_b + p - a);

        /* Use 128-bit intermediates to avoid overflow */
        uint64_t c = ((__uint128_t)a * two_b_minus_a) % p;
        uint64_t d = (((__uint128_t)a * a) % p + (__uint128_t)b * b % p) % p;

        if ((n >> i) & 1) {
            a = d;
            b = (c + d < p) ? (c + d) : (c + d - p);
        } else {
            a = c;
            b = d;
        }
    }
    return a;
}

/*
 * verify: given the spilled result X claiming to equal F_n,
 * check X mod p == fib_mod(n, p) for each prime in the table.
 *
 * Returns 1 if all checks pass, 0 otherwise.
 */
int verify_result(spilled_t *X, uint64_t n,
                  const uint64_t *primes, int nprimes)
{
    mp_limb_t *lx = spill_reload(X);
    mp_size_t sx = X->size;
    while (sx > 0 && lx[sx-1] == 0) sx--;

    int ok = 1;
    for (int i = 0; i < nprimes; i++) {
        uint64_t p = primes[i];

        /* Compute X mod p using mpn_mod_1 */
        mp_limb_t xmod;
        if (sx == 0)
            xmod = 0;
        else
            xmod = mpn_mod_1(lx, sx, (mp_limb_t)p);

        uint64_t fmod = fib_mod(n, p);

        if ((uint64_t)xmod != fmod) {
            fprintf(stderr,
                "VERIFICATION FAILED: mod %lu  got %lu  expected %lu\n",
                (unsigned long)p, (unsigned long)xmod, (unsigned long)fmod);
            ok = 0;
        } else {
            fprintf(stderr,
                "ok  mod %lu\n", (unsigned long)p);
        }
    }
    free(lx);
    return ok;
}
