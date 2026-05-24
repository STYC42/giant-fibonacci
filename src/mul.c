#include "fib.h"
#include "trace.h"
#include <assert.h>


static mp_size_t split_at(const mp_limb_t *x, mp_size_t sx,
                           mp_size_t n_limbs,
                           mp_limb_t **xl_out, mp_size_t *xl_sz,
                           mp_limb_t **xh_out, mp_size_t *xh_sz)
{
    mp_size_t lo_sz = (n_limbs < sx) ? n_limbs : sx;
    mp_size_t hi_sz = (sx > n_limbs) ? (sx - n_limbs) : 0;

    mp_limb_t *xl = malloc(sizeof(mp_limb_t) * (size_t)(lo_sz + 1));
    mp_limb_t *xh = malloc(sizeof(mp_limb_t) * (size_t)(hi_sz + 1));
    if (!xl || !xh) { perror("malloc"); exit(EXIT_FAILURE); }

    memcpy(xl, x, sizeof(mp_limb_t) * (size_t)lo_sz);
    *xl_sz = lo_sz;

    if (hi_sz > 0)
        memcpy(xh, x + n_limbs, sizeof(mp_limb_t) * (size_t)hi_sz);
    *xh_sz = hi_sz;

    *xl_out = xl;
    *xh_out = xh;
    return n_limbs;
}

static mp_size_t mpn_add_wrap(mp_limb_t *res,
                               const mp_limb_t *a, mp_size_t sa,
                               const mp_limb_t *b, mp_size_t sb)
{
    if (sa < sb) {
        const mp_limb_t *t; mp_size_t ts;
        t = a; a = b; b = t;
        ts = sa; sa = sb; sb = ts;
    }
    mp_limb_t carry = mpn_add(res, a, sa, b, sb);
    res[sa] = carry;
    return sa + (carry ? 1 : 0);
}

static mp_size_t mpn_sub_wrap(mp_limb_t *res,
                               const mp_limb_t *a, mp_size_t sa,
                               const mp_limb_t *b, mp_size_t sb)
{
    assert(sa >= sb);
    mpn_sub(res, a, sa, b, sb);
    mp_size_t sz = sa;
    while (sz > 0 && res[sz - 1] == 0) sz--;
    return sz;
}

static mp_limb_t *karatsuba_combine(
    const mp_limb_t *z0, mp_size_t sz0,
    const mp_limb_t *z1, mp_size_t sz1,
    const mp_limb_t *z2, mp_size_t sz2,
    mp_size_t n_limbs,
    mp_size_t *out_size)
{
    mp_size_t mid_alloc = sz1 + 1;
    mp_limb_t *mid = calloc((size_t)mid_alloc, sizeof(mp_limb_t));
    if (!mid) { perror("calloc"); exit(EXIT_FAILURE); }

    mp_size_t mid_sz = sz1;
    memcpy(mid, z1, sizeof(mp_limb_t) * (size_t)sz1);

    if (sz2 > 0) mid_sz = mpn_sub_wrap(mid, mid, mid_sz, z2, sz2);
    if (sz0 > 0) mid_sz = mpn_sub_wrap(mid, mid, mid_sz, z0, sz0);

    mp_size_t res_size = sz2 + 2 * n_limbs + 2;
    mp_limb_t *res = calloc((size_t)res_size, sizeof(mp_limb_t));
    if (!res) { perror("calloc"); exit(EXIT_FAILURE); }

    if (sz0 > 0)
        mpn_add(res, res, res_size, z0, sz0);
    if (mid_sz > 0)
        mpn_add(res + n_limbs, res + n_limbs, res_size - n_limbs, mid, mid_sz);
    if (sz2 > 0)
        mpn_add(res + 2 * n_limbs, res + 2 * n_limbs, res_size - 2 * n_limbs,
                z2, sz2);

    free(mid);
    while (res_size > 0 && res[res_size - 1] == 0) res_size--;
    *out_size = res_size;
    return res;
}

static unsigned long mul_counter = 0;

void mul_outofcore(spilled_t *out, const char *out_name,
                   spilled_t *a,   spilled_t *b)
{
    mp_bitcnt_t bits_a = (mp_bitcnt_t)a->size * GMP_NUMB_BITS;
    mp_bitcnt_t bits_b = (mp_bitcnt_t)b->size * GMP_NUMB_BITS;
    mp_bitcnt_t n_bits = (bits_a + bits_b) / 4;

    if (n_bits < MUL_THRESHOLD_BITS) {
        TRACE("mul    %-24s  %.1f + %.1f MB  (direct)\n",
              out_name, bits_a / 8e6, bits_b / 8e6);
        double t0 = trace_now();

        mp_limb_t *la = spill_reload(a);
        mp_limb_t *lb = spill_reload(b);

        mp_size_t sa = a->size, sb = b->size;
        while (sa > 0 && la[sa-1] == 0) sa--;
        while (sb > 0 && lb[sb-1] == 0) sb--;

        mp_size_t sr = (sa == 0 || sb == 0) ? 1 : sa + sb;
        mp_limb_t *res = calloc((size_t)sr, sizeof(mp_limb_t));
        if (!res) { perror("calloc"); exit(EXIT_FAILURE); }

        if (sa > 0 && sb > 0)
            mpn_mul(res, la, sa, lb, sb);

        free(la);
        free(lb);

        while (sr > 1 && res[sr-1] == 0) sr--;
        spill_write(out, out_name, res, sr);
        free(res);

        TRACE("mul    %-24s  done  %.2f s\n", out_name, TRACE_T(t0));
        return;
    }

    TRACE("mul    %-24s  %.1f + %.1f MB  (Karatsuba)\n",
          out_name, bits_a / 8e6, bits_b / 8e6);
    double t0 = trace_now();

    mp_size_t n_limbs = (mp_size_t)(n_bits / GMP_NUMB_BITS);
    if (n_limbs == 0) n_limbs = 1;

    char name_buf[64];
    unsigned long ctr = ++mul_counter;

    mp_limb_t *la = spill_reload(a);
    mp_limb_t *al, *ah;
    mp_size_t  sal, sah;
    split_at(la, a->size, n_limbs, &al, &sal, &ah, &sah);
    free(la);

    mp_limb_t *lb = spill_reload(b);
    mp_limb_t *bl, *bh;
    mp_size_t  sbl, sbh;
    split_at(lb, b->size, n_limbs, &bl, &sbl, &bh, &sbh);
    free(lb);

    mp_size_t ssa_alloc = (sal > sah ? sal : sah) + 1;
    mp_size_t ssb_alloc = (sbl > sbh ? sbl : sbh) + 1;
    mp_limb_t *sa_limbs = malloc(sizeof(mp_limb_t) * (size_t)ssa_alloc);
    mp_limb_t *sb_limbs = malloc(sizeof(mp_limb_t) * (size_t)ssb_alloc);
    if (!sa_limbs || !sb_limbs) { perror("malloc"); exit(EXIT_FAILURE); }
    mp_size_t ssa = mpn_add_wrap(sa_limbs, al, sal, ah, sah);
    mp_size_t ssb = mpn_add_wrap(sb_limbs, bl, sbl, bh, sbh);

    spilled_t s_al, s_ah, s_bl, s_bh, s_sa, s_sb;
    snprintf(name_buf, sizeof(name_buf), "al_%lu", ctr);
    spill_write(&s_al, name_buf, al, sal);  free(al);
    snprintf(name_buf, sizeof(name_buf), "ah_%lu", ctr);
    spill_write(&s_ah, name_buf, ah, sah);  free(ah);
    snprintf(name_buf, sizeof(name_buf), "bl_%lu", ctr);
    spill_write(&s_bl, name_buf, bl, sbl);  free(bl);
    snprintf(name_buf, sizeof(name_buf), "bh_%lu", ctr);
    spill_write(&s_bh, name_buf, bh, sbh);  free(bh);
    snprintf(name_buf, sizeof(name_buf), "sa_%lu", ctr);
    spill_write(&s_sa, name_buf, sa_limbs, ssa);  free(sa_limbs);
    snprintf(name_buf, sizeof(name_buf), "sb_%lu", ctr);
    spill_write(&s_sb, name_buf, sb_limbs, ssb);  free(sb_limbs);

    spilled_t z1;
    snprintf(name_buf, sizeof(name_buf), "z1_%lu", ctr);
    mul_outofcore(&z1, name_buf, &s_sa, &s_sb);
    spill_free(&s_sa);
    spill_free(&s_sb);

    spilled_t z0;
    snprintf(name_buf, sizeof(name_buf), "z0_%lu", ctr);
    mul_outofcore(&z0, name_buf, &s_al, &s_bl);
    spill_free(&s_al);
    spill_free(&s_bl);

    spilled_t z2;
    snprintf(name_buf, sizeof(name_buf), "z2_%lu", ctr);
    mul_outofcore(&z2, name_buf, &s_ah, &s_bh);
    spill_free(&s_ah);
    spill_free(&s_bh);

    mp_limb_t *lz0 = spill_reload(&z0);
    mp_limb_t *lz1 = spill_reload(&z1);
    mp_limb_t *lz2 = spill_reload(&z2);

    mp_size_t res_sz;
    mp_limb_t *res = karatsuba_combine(
        lz0, z0.size, lz1, z1.size, lz2, z2.size, n_limbs, &res_sz);

    free(lz0); free(lz1); free(lz2);
    spill_free(&z0); spill_free(&z1); spill_free(&z2);

    spill_write(out, out_name, res, res_sz);
    free(res);

    TRACE("mul    %-24s  done  %.2f s\n", out_name, TRACE_T(t0));
}
