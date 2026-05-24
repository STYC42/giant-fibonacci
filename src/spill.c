#define _POSIX_C_SOURCE 200809L
#include "fib.h"
#include "trace.h"
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

void spill_init(void)
{
    if (mkdir(SPILL_DIR, 0700) != 0 && errno != EEXIST) {
        perror("mkdir " SPILL_DIR);
        exit(EXIT_FAILURE);
    }
}

void spill_write(spilled_t *s, const char *name,
                 const mp_limb_t *limbs, mp_size_t size)
{
    snprintf(s->path, sizeof(s->path), "%s/%s.bin", SPILL_DIR, name);
    s->size = size;

    double t0 = trace_now();

    FILE *f = fopen(s->path, "wb");
    if (!f) { perror(s->path); exit(EXIT_FAILURE); }

    if (fwrite(&size, sizeof(size), 1, f) != 1 ||
        fwrite(limbs, sizeof(mp_limb_t), (size_t)size, f) != (size_t)size)
    {
        fprintf(stderr, "spill_write: short write to %s\n", s->path);
        exit(EXIT_FAILURE);
    }
    fclose(f);

    double bytes = (double)size * sizeof(mp_limb_t);
    double dt    = trace_now() - t0;
    TRACE("spill  %-24s  %7.1f MB  %6.2f s  %5.0f MB/s\n",
          name, bytes / 1e6, dt, dt > 0 ? bytes / 1e6 / dt : 0.0);
}

void spill_read(const spilled_t *s, mp_limb_t *limbs)
{
    FILE *f = fopen(s->path, "rb");
    if (!f) { perror(s->path); exit(EXIT_FAILURE); }

    mp_size_t size;
    if (fread(&size, sizeof(size), 1, f) != 1 || size != s->size) {
        fprintf(stderr, "spill_read: metadata mismatch in %s\n", s->path);
        exit(EXIT_FAILURE);
    }
    if (fread(limbs, sizeof(mp_limb_t), (size_t)size, f) != (size_t)size) {
        fprintf(stderr, "spill_read: short read from %s\n", s->path);
        exit(EXIT_FAILURE);
    }
    fclose(f);
}

mp_limb_t *spill_reload(const spilled_t *s)
{
    mp_limb_t *buf = malloc(sizeof(mp_limb_t) * (size_t)s->size);
    if (!buf) { perror("malloc"); exit(EXIT_FAILURE); }

    double t0 = trace_now();
    spill_read(s, buf);
    double bytes = (double)s->size * sizeof(mp_limb_t);
    double dt    = trace_now() - t0;
    TRACE("reload %-24s  %7.1f MB  %6.2f s  %5.0f MB/s\n",
          s->path + sizeof(SPILL_DIR), bytes / 1e6, dt,
          dt > 0 ? bytes / 1e6 / dt : 0.0);

    return buf;
}

void spill_free(spilled_t *s)
{
    if (s->path[0]) {
        remove(s->path);
        s->path[0] = '\0';
    }
}
