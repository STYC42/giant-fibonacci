#include "fib.h"
#include "trace.h"
#include <assert.h>
 
static void spill_const(spilled_t *s, const char *name, mp_limb_t val)
{
    mp_limb_t limb = val;
    spill_write(s, name, &limb, 1);
}
 
static void spill_sqr(spilled_t *out, const char *out_name, spilled_t *x)
{
    mp_limb_t *lx = spill_reload(x);
    mp_size_t sx = x->size;
    while (sx > 0 && lx[sx-1] == 0) sx--;
 
    mp_size_t sr = (sx == 0) ? 1 : 2 * sx;
    mp_limb_t *res = calloc((size_t)sr, sizeof(mp_limb_t));
    if (!res) { perror("calloc"); exit(EXIT_FAILURE); }
 
    if (sx > 0) mpn_sqr(res, lx, sx);
    free(lx);
 
    while (sr > 1 && res[sr-1] == 0) sr--;
    spill_write(out, out_name, res, sr);
    free(res);
}
 
static void spill_add(spilled_t *out, const char *out_name,
                      spilled_t *a, spilled_t *b)
{
    mp_limb_t *la = spill_reload(a);
    mp_limb_t *lb = spill_reload(b);
    mp_size_t sa = a->size, sb = b->size;
 
    mp_size_t sr = (sa > sb ? sa : sb) + 1;
    mp_limb_t *res = calloc((size_t)sr, sizeof(mp_limb_t));
    if (!res) { perror("calloc"); exit(EXIT_FAILURE); }
 
    if (sa >= sb)
        res[sa] = mpn_add(res, la, sa, lb, sb);
    else
        res[sb] = mpn_add(res, lb, sb, la, sa);
 
    free(la); free(lb);
    while (sr > 1 && res[sr-1] == 0) sr--;
    spill_write(out, out_name, res, sr);
    free(res);
}
 
static void spill_sub(spilled_t *out, const char *out_name,
                      spilled_t *a, spilled_t *b)
{
    mp_limb_t *la = spill_reload(a);
    mp_limb_t *lb = spill_reload(b);
    mp_size_t sa = a->size, sb = b->size;
 
    assert(sa >= sb);
    mp_limb_t *res = malloc(sizeof(mp_limb_t) * (size_t)sa);
    if (!res) { perror("malloc"); exit(EXIT_FAILURE); }
 
    mpn_sub(res, la, sa, lb, sb);
    free(la); free(lb);
 
    mp_size_t sr = sa;
    while (sr > 1 && res[sr-1] == 0) sr--;
    spill_write(out, out_name, res, sr);
    free(res);
}
 
static void spill_lshift1(spilled_t *out, const char *out_name, spilled_t *a)
{
    mp_limb_t *la = spill_reload(a);
    mp_size_t sa = a->size;
    mp_limb_t *res = malloc(sizeof(mp_limb_t) * (size_t)(sa + 1));
    if (!res) { perror("malloc"); exit(EXIT_FAILURE); }
 
    res[sa] = mpn_lshift(res, la, sa, 1);
    free(la);
 
    mp_size_t sr = sa + (res[sa] ? 1 : 0);
    while (sr > 1 && res[sr-1] == 0) sr--;
    spill_write(out, out_name, res, sr);
    free(res);
}
 
static unsigned long fib_counter = 0;
 
static void fib_double_rec(mp_limb_t n,
                            spilled_t *fn,  const char *fn_name,
                            spilled_t *fn1, const char *fn1_name)
{
    if (n == 0) {
        spill_const(fn,  fn_name,  0);
        spill_const(fn1, fn1_name, 1);
        return;
    }
 
    unsigned long ctr = ++fib_counter;
    char a_name[64], b_name[64], tmp_name[64];
    spilled_t a, b;
 
    snprintf(a_name, sizeof(a_name), "fa_%lu", ctr);
    snprintf(b_name, sizeof(b_name), "fb_%lu", ctr);
 
    fib_double_rec(n / 2, &a, a_name, &b, b_name);
 
    TRACE("fib    n=%-20lu  step %s\n", (unsigned long)n, n % 2 ? "odd" : "even");
    double t0 = trace_now();
 
    spilled_t two_b;
    snprintf(tmp_name, sizeof(tmp_name), "2b_%lu", ctr);
    spill_lshift1(&two_b, tmp_name, &b);
 
    spilled_t two_b_minus_a;
    snprintf(tmp_name, sizeof(tmp_name), "2bma_%lu", ctr);
    spill_sub(&two_b_minus_a, tmp_name, &two_b, &a);
    spill_free(&two_b);
 
    spilled_t c;
    snprintf(tmp_name, sizeof(tmp_name), "c_%lu", ctr);
    mul_outofcore(&c, tmp_name, &a, &two_b_minus_a);
    spill_free(&two_b_minus_a);
 
    spilled_t a2;
    snprintf(tmp_name, sizeof(tmp_name), "a2_%lu", ctr);
    spill_sqr(&a2, tmp_name, &a);
    spill_free(&a);
 
    spilled_t b2;
    snprintf(tmp_name, sizeof(tmp_name), "b2_%lu", ctr);
    spill_sqr(&b2, tmp_name, &b);
    spill_free(&b);
 
    spilled_t d;
    snprintf(tmp_name, sizeof(tmp_name), "d_%lu", ctr);
    spill_add(&d, tmp_name, &a2, &b2);
    spill_free(&a2);
    spill_free(&b2);
 
    if (n % 2 == 0) {
        mp_size_t sc = c.size, sd = d.size;
        mp_limb_t *lc = spill_reload(&c); spill_free(&c);
        mp_limb_t *ld = spill_reload(&d); spill_free(&d);
        spill_write(fn,  fn_name,  lc, sc); free(lc);
        spill_write(fn1, fn1_name, ld, sd); free(ld);
    } else {
        spilled_t cd;
        snprintf(tmp_name, sizeof(tmp_name), "cd_%lu", ctr);
        spill_add(&cd, tmp_name, &c, &d);
        spill_free(&c);
 
        mp_size_t sd = d.size, scd = cd.size;
        mp_limb_t *ld  = spill_reload(&d);  spill_free(&d);
        mp_limb_t *lcd = spill_reload(&cd); spill_free(&cd);
        spill_write(fn,  fn_name,  ld,  sd);  free(ld);
        spill_write(fn1, fn1_name, lcd, scd); free(lcd);
    }
 
    TRACE("fib    n=%-20lu  done  %.2f s\n", (unsigned long)n, TRACE_T(t0));
}
 
void fib_double(mp_limb_t n, spilled_t *fn, spilled_t *fn1)
{
    fib_double_rec(n, fn, "fn_out", fn1, "fn1_out");
}
 
