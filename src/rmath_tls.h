/*
 *  Rmath-julia : per-thread mutable state for the random generators
 *
 *  This file is specific to Rmath-julia and is not part of R.  `make update`
 *  extracts R's sources over src/ but never deletes files, so this one
 *  survives an R bump untouched.
 *
 *  Upstream R keeps the generators' setup constants in function-local `static`
 *  variables: a memoisation cache so that repeated calls with unchanged
 *  parameters skip the setup arithmetic.  Rmath-julia needs that state to be
 *  per-thread, but declaring it `_Thread_local` gave the library an 856-byte
 *  static TLS segment, and glibc draws a dlopen'd module's TLS block from a
 *  fixed ~1664-byte process-wide surplus shared with every other shared
 *  library.  Consuming half of it made unrelated libraries fail to load with
 *  "cannot allocate memory in static TLS block"; see
 *  https://github.com/JuliaStats/Rmath-julia/issues/56.
 *
 *  So the state lives on the heap, reached through one thread-local pointer:
 *  8 bytes of static TLS instead of 856.  A thread that never calls these
 *  generators now pays nothing at all, where before every thread in the
 *  process paid eagerly whether it used Rmath or not.
 *
 *  Every field below is written by the generator that owns it -- verified by
 *  const-qualifying each one and checking the compiler rejects the assignment.
 *  Read-only coefficient tables are left exactly as upstream has them, which is
 *  `static` in some places and `const static` in others; marking those
 *  thread-local would advertise per-thread state that does not exist.
 *
 *  Field names are kept identical to upstream R, and each generator aliases
 *  them back to those names with a block of `#define`s at its fetch site, so
 *  the function bodies stay byte-identical to R's and re-applying
 *  patches/thread-local.patch after `make update` stays mechanical.
 */

#ifndef RMATH_TLS_H
#define RMATH_TLS_H

/* One struct per group of state, mirroring the declarations each generator
 * used to carry. */

struct rbeta_state {
    double beta, gamma, delta, k1, k2;
    double olda, oldb;
};

struct rbinom_state {
    double c, fm, npq, p1, p2, p3, p4, qn;
    double xl, xll, xlr, xm, xr;
    double psave;
    int nsave;
    int m;
};

struct rgamma_state {
    double aa;
    double aaa;
    double s, s2, d;		/* no. 1 (step 1) */
    double q0, b, si, c;	/* no. 2 (step 4) */
};

struct rhyper_state {
    int ks, n1s, n2s;
    int m, minjx, maxjx;
    int k, n1, n2;		/* <- not allowing larger integer par */
    double N;
    /* II : */
    double w;
    /* III: */
    double a, d, s, xl, xr, kl, kr, lamdl, lamdr, p1, p2, p3;
};

struct rpois_state {
    int l, m;
    double b1, b2, c, c0, c1, c2, c3;
    double pp[36], p0, p, q, s, d, omega;
    double big_l;		/* integer "w/o overflow" */
    double muprev, muprev2;
};

/* These two differ from the generators above: the state is a pointer to a
 * table that is itself on the heap, so the container owns a second level of
 * allocation that has to be released before the container itself. */

struct signrank_state {
    double *w;
    int allocated_n;
};

struct wilcox_state {
    double ***w;		/* to store  cwilcox(i,j,k) -> w[i][j][k] */
    int allocated_m, allocated_n;
};

typedef struct {
    struct rbeta_state    rbeta;
    struct rbinom_state   rbinom;
    struct rgamma_state   rgamma;
    struct rhyper_state   rhyper;
    struct rpois_state    rpois;
    struct signrank_state signrank;
    struct wilcox_state   wilcox;
} Rmath_tls;

/* Each generator initialises its own struct, in its own .c file.
 *
 * Contract: an init hook is the mechanical image of the initialisers that were
 * deleted from that generator's declarations -- `static double olda = -1.0;`
 * becomes `st->olda = -1.0;`.  Keeping the hook beside the code that reads the
 * value means a future R release changing a sentinel shows both halves in one
 * patch hunk, instead of leaving them to be correlated across two files.
 *
 * Anything a hook does not name is already zero: Rmath_tls_alloc() callocs.
 *
 * signrank and wilcox deliberately have no hook.  They had no initialisers
 * upstream, and they positively depend on the zeroing: a NULL w means "not
 * allocated yet", and csignrank() uses w[0] == 1. as its "table already built"
 * flag.  A hook there could only do harm.
 */
void Rmath_rbeta_state_init (struct rbeta_state  *st);
void Rmath_rbinom_state_init(struct rbinom_state *st);
void Rmath_rgamma_state_init(struct rgamma_state *st);
void Rmath_rhyper_state_init(struct rhyper_state *st);
void Rmath_rpois_state_init (struct rpois_state  *st);

/* Release the heap that hangs off these two, before the container is freed.
 * The deep-free logic stays in the file that knows the table's shape --
 * wilcox's is a jagged three-level array -- rather than in rmath_tls.c. */
void Rmath_signrank_state_free(struct signrank_state *st);
void Rmath_wilcox_state_free  (struct wilcox_state   *st);

/* The calling thread's state.  Never NULL: a failed allocation raises
 * MATHLIB_ERROR from Rmath_tls_alloc(), so callers need no error path.
 *
 * Split so that the common case is an inlined load-and-test while the
 * allocation stays out of line: this sits on the path of every rbeta, rbinom,
 * rgamma, rhyper and rpois call.  Rmath_tls_ptr has external linkage only so
 * that the inline can reach it across translation units; neither it nor
 * anything else here is public API (nothing in this header is declared in
 * include/Rmath.h).
 */
extern _Thread_local Rmath_tls *Rmath_tls_ptr;
Rmath_tls *Rmath_tls_alloc(void);

static inline Rmath_tls *Rmath_tls_get(void)
{
    Rmath_tls *t = Rmath_tls_ptr;
    return t ? t : Rmath_tls_alloc();
}

#endif /* RMATH_TLS_H */
