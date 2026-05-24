#ifndef FIB
#define FIB

#include <gmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Threshold (in bits) below which mpn_mul is called directly.
   Set so that 12n <= 64 GB (Schönhage-Strassen workspace factor ~3x). */
#define MUL_THRESHOLD_BITS  (4UL * 1024 * 1024 * 1024 * 8UL)   /* 4 GB in bits */

/* Temporary file directory */
#define SPILL_DIR "/tmp/fib_spill"

/* ------------------------------------------------------------------
   Spilled integer: a named mpn array living on disk.
   ------------------------------------------------------------------ */
typedef struct {
    char   path[256];
    mp_size_t size;   /* number of limbs */
} spilled_t;

/* ------------------------------------------------------------------
   spill / reload / spill_free API
   ------------------------------------------------------------------ */
void spill_init(void);   /* create SPILL_DIR */

/* Write limbs[0..size-1] to a file and record metadata in *s. */
void spill_write(spilled_t *s, const char *name,
                 const mp_limb_t *limbs, mp_size_t size);

/* Allocate and reload. Caller must mpn_free() the returned pointer. */
mp_limb_t *spill_reload(const spilled_t *s);

/* Delete the backing file. */
void spill_free(spilled_t *s);

/* ------------------------------------------------------------------
   Out-of-core multiplication
   Result written to *out (spilled).  Inputs must be spilled.
   ------------------------------------------------------------------ */
void mul_outofcore(spilled_t *out, const char *out_name,
                   spilled_t *a,   spilled_t *b);

/* ------------------------------------------------------------------
   Fast doubling
   Computes (F_n, F_{n+1}) and writes them to *fn / *fn1 (spilled).
   ------------------------------------------------------------------ */
void fib_double(mp_limb_t n, spilled_t *fn, spilled_t *fn1);

/* ------------------------------------------------------------------
   Modular verification
   Returns F_n mod p using machine arithmetic only.
   ------------------------------------------------------------------ */
uint64_t fib_mod(uint64_t n, uint64_t p);

/* Verify X mod p_i == F_n mod p_i for each prime.  Returns 1 on success. */
int verify_result(spilled_t *X, uint64_t n,
                  const uint64_t *primes, int nprimes);

#endif