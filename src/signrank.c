/*
 *  Mathlib : A C Library of Special Functions
 *  Copyright (C) 1999-2024  The R Core Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, a copy is available at
 *  https://www.R-project.org/Licenses/
 *
 *  SYNOPSIS
 *
 *    #include <Rmath.h>
 *    double dsignrank(double x, double n, int give_log)
 *    double psignrank(double x, double n, int lower_tail, int log_p)
 *    double qsignrank(double x, double n, int lower_tail, int log_p)
 *    double rsignrank(double n)
 *
 *  DESCRIPTION
 *
 *    dsignrank	   The density of the Wilcoxon Signed Rank distribution.
 *    psignrank	   The distribution function of the Wilcoxon Signed Rank
 *		   distribution.
 *    qsignrank	   The quantile function of the Wilcoxon Signed Rank
 *		   distribution.
 *    rsignrank	   Random variates from the Wilcoxon Signed Rank
 *		   distribution.
 */

#include "nmath.h"
#include "dpq.h"
#include "rmath_tls.h"

/* The table lives in the calling thread's Rmath_tls block; see rmath_tls.h.
 * Only the pointer and its size are per-thread state -- the table itself was
 * always on the heap. */

void Rmath_signrank_state_free(struct signrank_state *st)
{
    if (!st->w) return;

    free((void *) st->w);
    st->w = 0;
    st->allocated_n = 0;
}

void signrank_free(void)
{
    Rmath_tls *tls = Rmath_tls_ptr;

    if (tls) Rmath_signrank_state_free(&tls->signrank);
}

static void
w_init_maybe(struct signrank_state *st, int n)
{
    int u, c;

    u = n * (n + 1) / 2;
    c = (u / 2);

    if (st->w) {
        if(n != st->allocated_n) {
	    Rmath_signrank_state_free(st);
	}
	else return;
    }

    if(!st->w) {
	st->w = (double *) calloc((size_t) c + 1, sizeof(double));
#ifdef MATHLIB_STANDALONE
	if (!st->w) MATHLIB_ERROR("%s", _("signrank allocation error"));
#endif
	st->allocated_n = n;
    }
}

static double
csignrank(struct signrank_state *st, int k, int n)
{
    int c, u, j;

#ifndef MATHLIB_STANDALONE
    R_CheckUserInterrupt();
#endif

    u = n * (n + 1) / 2;
    c = (u / 2);

    if (k < 0 || k > u)
	return 0;
    if (k > c)
	k = u - k;

    if (n == 1)
        return 1.;
    if (st->w[0] == 1.)
        return st->w[k];

    st->w[0] = st->w[1] = 1.;
    for(j = 2; j < n+1; ++j) {
        int i, end = imin2(j*(j+1)/2, c);
	for(i = end; i >= j; --i)
	    st->w[i] += st->w[i-j];
    }

    return st->w[k];
}

double dsignrank(double x, double n, int give_log)
{
    double d;

#ifdef IEEE_754
    /* NaNs propagated correctly */
    if (ISNAN(x) || ISNAN(n)) return(x + n);
#endif
    n = R_forceint(n);
    if (n <= 0)
	ML_WARN_return_NAN;

    if (R_nonint(x))
	return(R_D__0);
    x = R_forceint(x);
    if ((x < 0) || (x > (n * (n + 1) / 2)))
	return(R_D__0);

    int nn = (int) n;
    Rmath_tls *tls = Rmath_tls_get();
    if (!tls) ML_WARN_return_NAN;
    struct signrank_state *st = &tls->signrank;
    w_init_maybe(st, nn);
    d = R_D_exp(log(csignrank(st, (int) x, nn)) - n * M_LN2);

    return(d);
}

double psignrank(double x, double n, int lower_tail, int log_p)
{
    int i;
    double f, p;

#ifdef IEEE_754
    if (ISNAN(x) || ISNAN(n))
    return(x + n);
#endif
    if (!R_FINITE(n)) ML_WARN_return_NAN;
    n = R_forceint(n);
    if (n <= 0) ML_WARN_return_NAN;

    x = R_forceint(x + 1e-7);
    if (x < 0.0)
	return(R_DT_0);
    if (x >= n * (n + 1) / 2)
	return(R_DT_1);

    int nn = (int) n;
    Rmath_tls *tls = Rmath_tls_get();
    if (!tls) ML_WARN_return_NAN;
    struct signrank_state *st = &tls->signrank;
    w_init_maybe(st, nn);
    f = exp(- n * M_LN2);
    p = 0;
    if (x <= (n * (n + 1) / 4)) {
	for (i = 0; i <= x; i++)
	    p += csignrank(st, i, nn) * f;
    }
    else {
	x = n * (n + 1) / 2 - x;
	for (i = 0; i < x; i++)
	    p += csignrank(st, i, nn) * f;
	lower_tail = !lower_tail; /* p = 1 - p; */
    }

    return(R_DT_val(p));
} /* psignrank() */

double qsignrank(double x, double n, int lower_tail, int log_p)
{
    double f, p;

#ifdef IEEE_754
    if (ISNAN(x) || ISNAN(n))
	return(x + n);
#endif
    if (!R_FINITE(x) || !R_FINITE(n))
	ML_WARN_return_NAN;
    R_Q_P01_check(x);

    n = R_forceint(n);
    if (n <= 0)
	ML_WARN_return_NAN;

    if (x == R_DT_0)
	return(0);
    if (x == R_DT_1)
	return(n * (n + 1) / 2);

    if(log_p || !lower_tail)
	x = R_DT_qIv(x); /* lower_tail,non-log "p" */

    int nn = (int) n;
    Rmath_tls *tls = Rmath_tls_get();
    if (!tls) ML_WARN_return_NAN;
    struct signrank_state *st = &tls->signrank;
    w_init_maybe(st, nn);
    f = exp(- n * M_LN2);
    p = 0;
    int q = 0;
    if (x <= 0.5) {
	x = x - 10 * DBL_EPSILON;
	for (;;) {
	    p += csignrank(st, q, nn) * f;
	    if (p >= x)
		break;
	    q++;
	}
    }
    else {
	x = 1 - x + 10 * DBL_EPSILON;
	for (;;) {
	    p += csignrank(st, q, nn) * f;
	    if (p > x) {
		q = (int)(n * (n + 1) / 2 - q);
		break;
	    }
	    q++;
	}
    }

    return(q);
}

double rsignrank(double n)
{
    int i, k;
    double r;

#ifdef IEEE_754
    /* NaNs propagated correctly */
    if (ISNAN(n)) return(n);
#endif
    n = R_forceint(n);
    if (n < 0) ML_WARN_return_NAN;

    if (n == 0)
	return(0);
    r = 0.0;
    k = (int) n;
    for (i = 0; i < k; ) {
	r += (++i) * floor(unif_rand() + 0.5);
    }
    return(r);
}
