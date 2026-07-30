/*
 *  Mathlib : A C Library of Special Functions
 *  Copyright (C) 2000-2020 The R Core Team
 *  Copyright (C) 2005-2020 The R Foundation
 *  Copyright (C) 1998 Ross Ihaka
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
 *    double rhyper(double NR, double NB, double n);
 *
 *  DESCRIPTION
 *
 *    Random variates from the hypergeometric distribution.
 *    Returns the number of white balls drawn when kk balls
 *    are drawn at random from an urn containing nn1 white
 *    and nn2 black balls.
 *
 *  REFERENCE
 *
 *    V. Kachitvichyanukul and B. Schmeiser (1985).
 *    ``Computer generation of hypergeometric random variates,''
 *    Journal of Statistical Computation and Simulation 22, 127-145.
 *
 *    The original algorithm had a bug -- R bug report PR#7314 --
 *    giving numbers slightly too small in case III h2pe
 *    where (m < 100 || ix <= 50) , see below.
 */

#include "nmath.h"
#include "dpq.h"
#include <limits.h>
#include "rmath_tls.h"

// afc(i) :=  ln( i! )	[logarithm of the factorial i] = {R:} lgamma(i + 1) = {C:} lgammafn(i + 1)
static double afc(int i)
{
    // If (i > 7), use Stirling's approximation, otherwise use table lookup.
    const static double al[8] =
    {
	0.0,/*ln(0!)=ln(1)*/
	0.0,/*ln(1!)=ln(1)*/
	0.69314718055994530941723212145817,/*ln(2) */
	1.79175946922805500081247735838070,/*ln(6) */
	3.17805383034794561964694160129705,/*ln(24)*/
	4.78749174278204599424770093452324,
	6.57925121201010099506017829290394,
	8.52516136106541430016553103634712
	/* 10.60460290274525022841722740072165, approx. value below =
	   10.6046028788027; rel.error = 2.26 10^{-9}

	  FIXME: Use constants and if(n > ..) decisions from ./stirlerr.c
	  -----  will be even *faster* for n > 500 (or so)
	*/
    };

    if (i < 0) {
	MATHLIB_WARNING(("rhyper.c: afc(i), i=%d < 0 -- SHOULD NOT HAPPEN!\n"), i);
	return -1; // unreached
    }
    if (i <= 7)
	return al[i];
    // else i >= 8 :
    double di = i, i2 = di*di;
    return (di + 0.5) * log(di) - di + M_LN_SQRT_2PI +
	(0.0833333333333333 - 0.00277777777777778 / i2) / di;
}

void Rmath_rhyper_state_init(struct rhyper_state *st)
{
    st->ks = -1;
    st->n1s = -1;
    st->n2s = -1;
}

//     rhyper(NR, NB, n) -- NR 'red', NB 'blue', n drawn, how many are 'red'
double rhyper(double nn1in, double nn2in, double kkin)
{
    /* extern double afc(int); */

    int nn1, nn2, kk;
    int ix; // return value (coerced to double at the very end)
    Rboolean setup1, setup2;

    /* check parameter validity */

    if(!R_FINITE(nn1in) || !R_FINITE(nn2in) || !R_FINITE(kkin))
	ML_WARN_return_NAN;

    nn1in = R_forceint(nn1in);
    nn2in = R_forceint(nn2in);
    kkin  = R_forceint(kkin);

    if (nn1in < 0 || nn2in < 0 || kkin < 0 || kkin > nn1in + nn2in)
	ML_WARN_return_NAN;
    if (nn1in >= INT_MAX || nn2in >= INT_MAX || kkin >= INT_MAX) {
	/* large n -- evade integer overflow (and inappropriate algorithms)
	   -------- */
        // FIXME: Much faster to give rbinom() approx when appropriate; -> see Kuensch(1989)
	// Johnson, Kotz,.. p.258 (top) mention the *four* different binomial approximations
	if(kkin == 1.) { // Bernoulli
	    return rbinom(kkin, nn1in / (nn1in + nn2in));
	}
	// Slow, but safe: return  F^{-1}(U)  where F(.) = phyper(.) and  U ~ U[0,1]
	return qhyper(unif_rand(), nn1in, nn2in, kkin,
		      /*lower_tail =*/ FALSE, /*log_p = */ FALSE);
	// lower_tail=FALSE: a thinko, is still "correct" as equiv. to  U <--> 1-U
    }
    nn1 = (int)nn1in;
    nn2 = (int)nn2in;
    kk  = (int)kkin;

    /* Per-thread state, persistent between calls for the same parameters.
     * Fetched after the INT_MAX escape above, which delegates to rbinom() or
     * qhyper() and uses none of it. */
    Rmath_tls *tls = Rmath_tls_get();
    if (!tls) ML_WARN_return_NAN;
    struct rhyper_state *st = &tls->rhyper;

    /* if new parameter values, initialize */
    if (nn1 != st->n1s || nn2 != st->n2s) { // n1 | n2 is changed: setup all
	setup1 = TRUE;	setup2 = TRUE;
    } else if (kk != st->ks) { // n1 & n2 are unchanged: setup 'k' only
	setup1 = FALSE;	setup2 = TRUE;
    } else { // all three unchanged ==> no setup
	setup1 = FALSE;	setup2 = FALSE;
    }
    if (setup1) { // n1 & n2
	st->n1s = nn1; st->n2s = nn2; // save
	st->N = nn1 + (double)nn2; // avoid int overflow
	if (nn1 <= nn2) {
	    st->n1 = nn1; st->n2 = nn2;
	} else { // nn2 < nn1
	    st->n1 = nn2; st->n2 = nn1;
	}
	// now have n1 <= n2
    }
    if (setup2) { // k
	st->ks = kk; // save
	if ((double)kk + kk >= st->N) { // this could overflow
	    st->k = (int)(st->N - kk);
	} else {
	    st->k = kk;
	}
    }
    if (setup1 || setup2) {
	st->m = (int) ((st->k + 1.) * (st->n1 + 1.) / (st->N + 2.)); // m := floor(adjusted mean E[.])
	st->minjx = imax2(0, st->k - st->n2);
	st->maxjx = imin2(st->n1, st->k);
#ifdef DEBUG_rhyper
	REprintf("rhyper(n1=%d, n2=%d, k=%d), setup: floor(a.mean)=: m = %d, [min,maxjx]= [%d,%d]\n",
		 nn1, nn2, kk, st->m, st->minjx, st->maxjx);
#endif
    }
    /* generate random variate --- Three basic cases */

    if (st->minjx == st->maxjx) { /* I: degenerate distribution ---------------- */
#ifdef DEBUG_rhyper
	REprintf("rhyper(), branch I (degenerate): ix := maxjx = %d\n", st->maxjx);
#endif
	ix = st->maxjx;
	goto L_finis; // return appropriate variate

    } else if (st->m - st->minjx < 10) { // II: (Scaled) algorithm HIN (inverse transformation) ----
	const static double scale = 1e25; // scaling factor against (early) underflow
	const static double con = 57.5646273248511421;
					  // 25*log(10) = log(scale) { <==> exp(con) == scale }
	if (setup1 || setup2) {
	    double lw; // log(w);  w = exp(lw) * scale = exp(lw + log(scale)) = exp(lw + con)
	    if (st->k < st->n2) {
		lw = afc(st->n2) + afc(st->n1 + st->n2 - st->k) - afc(st->n2 - st->k) - afc(st->n1 + st->n2);
	    } else {
		lw = afc(st->n1) + afc(     st->k     ) - afc(st->k - st->n2) - afc(st->n1 + st->n2);
	    }
	    st->w = exp(lw + con);
	}
	double p, u;
#ifdef DEBUG_rhyper
	REprintf("rhyper(), branch II; w = %g > 0\n", st->w);
#endif
      L10:
	p = st->w;
	ix = st->minjx;
	u = unif_rand() * scale;
#ifdef DEBUG_rhyper
	REprintf("  _new_ u = %g\n", u);
#endif
	while (u > p) {
	    u -= p;
	    p *= ((double) st->n1 - ix) * (st->k - ix);
	    ix++;
	    p = p / ix / (st->n2 - st->k + ix);
#ifdef DEBUG_rhyper
	    REprintf("       ix=%3d, u=%11g, p=%20.14g (u-p=%g)\n", ix, u, p, u-p);
#endif
	    if (ix > st->maxjx)
		goto L10;
	    // FIXME  if(p == 0.)  we also "have lost"  => goto L10
	}

    } else { /* III : H2PE Algorithm --------------------------------------- */

	double u,v;

	if (setup1 || setup2) {
	    st->s = sqrt((st->N - st->k) * st->k * st->n1 * st->n2 / (st->N - 1) / st->N / st->N);

	    /* remark: d is defined in reference without int. */
	    /* the truncation centers the cell boundaries at 0.5 */

	    st->d = (int) (1.5 * st->s) + .5;
	    st->xl = st->m - st->d + .5;
	    st->xr = st->m + st->d + .5;
	    st->a = afc(st->m) + afc(st->n1 - st->m) + afc(st->k - st->m) + afc(st->n2 - st->k + st->m);
	    st->kl = exp(st->a - afc((int) (st->xl)) - afc((int) (st->n1 - st->xl))
		     - afc((int) (st->k - st->xl))
		     - afc((int) (st->n2 - st->k + st->xl)));
	    st->kr = exp(st->a - afc((int) (st->xr - 1))
		     - afc((int) (st->n1 - st->xr + 1))
		     - afc((int) (st->k - st->xr + 1))
		     - afc((int) (st->n2 - st->k + st->xr - 1)));
	    st->lamdl = -log(st->xl * (st->n2 - st->k + st->xl) / (st->n1 - st->xl + 1) / (st->k - st->xl + 1));
	    st->lamdr = -log((st->n1 - st->xr + 1) * (st->k - st->xr + 1) / st->xr / (st->n2 - st->k + st->xr));
	    st->p1 = st->d + st->d;
	    st->p2 = st->p1 + st->kl / st->lamdl;
	    st->p3 = st->p2 + st->kr / st->lamdr;
	}
#ifdef DEBUG_rhyper
	REprintf("rhyper(), branch III {accept/reject}: (xl,xr)= (%g,%g); (lamdl,lamdr)= (%g,%g)\n",
		 st->xl, st->xr, st->lamdl,st->lamdr);
	REprintf("-------- p123= c(%g,%g,%g)\n", st->p1,st->p2, st->p3);
#endif
	int n_uv = 0;
      L30:
	u = unif_rand() * st->p3;
	v = unif_rand();
	n_uv++;
	if(n_uv >= 10000) {
	    REprintf("rhyper(*, n1=%d, n2=%d, k=%d): branch III: giving up after %d rejections\n",
		     nn1, nn2, kk, n_uv);
	    ML_WARN_return_NAN;
        }
#ifdef DEBUG_rhyper
	REprintf(" ... L30 [%d]: new (u=%g, v ~ U[0,1]=%g): ", n_uv, u,v);
#endif

	if (u < st->p1) {		/* rectangular region */
	    ix = (int) (st->xl + u);
	} else if (u <= st->p2) {	/* left tail */
	    ix = (int) (st->xl + log(v) / st->lamdl);
	    if (ix < st->minjx)
		goto L30;
	    v = v * (u - st->p1) * st->lamdl;
	} else {		/* right tail */
	    ix = (int) (st->xr - log(v) / st->lamdr);
	    if (ix > st->maxjx)
		goto L30;
	    v = v * (u - st->p2) * st->lamdr;
	}

	/* acceptance/rejection test */
	Rboolean reject = TRUE;

	if (st->m < 100 || ix <= 50) {
	    /* explicit evaluation */
	    /* The original algorithm (and TOMS 668) have
		   f = f * i * (n2 - k + i) / (n1 - i) / (k - i);
	       in the (m > ix) case, but the definition of the
	       recurrence relation on p134 shows that the +1 is
	       needed. */
	    int i;
	    double f = 1.0;
	    if (st->m < ix) {
		for (i = st->m + 1; i <= ix; i++)
		    f = f * (st->n1 - i + 1) * (st->k - i + 1) / (st->n2 - st->k + i) / i;
	    } else if (st->m > ix) {
		for (i = ix + 1; i <= st->m; i++)
		    f = f * i * (st->n2 - st->k + i) / (st->n1 - i + 1) / (st->k - i + 1);
	    }
	    if (v <= f) {
		reject = FALSE;
	    }
	} else {

	    const static double deltal = 0.0078;
	    const static double deltau = 0.0034;

	    double e, g, r, t, y;
	    double de, dg, dr, ds, dt, gl, gu, nk, nm, ub;
	    double xk, xm, xn, y1, ym, yn, yk, alv;

#ifdef DEBUG_rhyper
	    REprintf(" ... accept/reject 'large' case v=%g\n", v);
#endif
	    /* squeeze using upper and lower bounds */
	    y = ix;
	    y1 = y + 1.0;
	    ym = y - st->m;
	    yn = st->n1 - y + 1.0;
	    yk = st->k - y + 1.0;
	    nk = st->n2 - st->k + y1;
	    r = -ym / y1;
	    st->s = ym / yn;
	    t = ym / yk;
	    e = -ym / nk;
	    g = yn * yk / (y1 * nk) - 1.0;
	    dg = 1.0;
	    if (g < 0.0)
		dg = 1.0 + g;
	    gu = g * (1.0 + g * (-0.5 + g / 3.0));
	    gl = gu - .25 * (g * g * g * g) / dg;
	    xm = st->m + 0.5;
	    xn = st->n1 - st->m + 0.5;
	    xk = st->k - st->m + 0.5;
	    nm = st->n2 - st->k + xm;
	    ub = y * gu - st->m * gl + deltau
		+ xm * r * (1. + r * (-0.5 + r / 3.0))
		+ xn * st->s * (1. + st->s * (-0.5 + st->s / 3.0))
		+ xk * t * (1. + t * (-0.5 + t / 3.0))
		+ nm * e * (1. + e * (-0.5 + e / 3.0));
	    /* test against upper bound */
	    alv = log(v);
	    if (alv > ub) {
		reject = TRUE;
	    } else {
				/* test against lower bound */
		dr = xm * (r * r * r * r);
		if (r < 0.0)
		    dr /= (1.0 + r);
		ds = xn * (st->s * st->s * st->s * st->s);
		if (st->s < 0.0)
		    ds /= (1.0 + st->s);
		dt = xk * (t * t * t * t);
		if (t < 0.0)
		    dt /= (1.0 + t);
		de = nm * (e * e * e * e);
		if (e < 0.0)
		    de /= (1.0 + e);
		if (alv < ub - 0.25 * (dr + ds + dt + de)
		    + (y + st->m) * (gl - gu) - deltal) {
		    reject = FALSE;
		}
		else {
		    /* * Stirling's formula to machine accuracy
		     */
		    if (alv <= (st->a - afc(ix) - afc(st->n1 - ix)
				- afc(st->k - ix) - afc(st->n2 - st->k + ix))) {
			reject = FALSE;
		    } else {
			reject = TRUE;
		    }
		}
	    }
	} // else
	if (reject)
	    goto L30;

    } // end{branch III}


L_finis:  /* return appropriate variate */

#ifdef DEBUG_rhyper
    REprintf(" L_finis: ix = %d, then", ix);
#endif
    if ((double)kk + kk >= st->N) {
	if (nn1 > nn2) {
	    ix = kk - nn2 + ix;
	} else {
	    ix = nn1 - ix;
	}
    } else if (nn1 > nn2) {
	ix = kk - ix;
    }
#ifdef DEBUG_rhyper
    REprintf(" %d\n", ix);
#endif
    return ix;
}
