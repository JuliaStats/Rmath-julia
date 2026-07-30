/*
 *  Mathlib : A C Library of Special Functions
 *  Copyright (C) 1998 Ross Ihaka
 *  Copyright (C) 2000-2011 The R Core Team
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
 *    double rpois(double lambda)
 *
 *  DESCRIPTION
 *
 *    Random variates from the Poisson distribution.
 *
 *  REFERENCE
 *
 *    Ahrens, J.H. and Dieter, U. (1982).
 *    Computer generation of Poisson deviates
 *    from modified normal distributions.
 *    ACM Trans. Math. Software 8, 163-179.
 */

#include "nmath.h"
#include "rmath_tls.h"

#define a0	-0.5
#define a1	 0.3333333
#define a2	-0.2500068
#define a3	 0.2000118
#define a4	-0.1661269
#define a5	 0.1421878
#define a6	-0.1384794
#define a7	 0.1250060

#define one_7	0.1428571428571428571
#define one_12	0.0833333333333333333
#define one_24	0.0416666666666666667

#define repeat for(;;)

void Rmath_rpois_state_init(struct rpois_state *st)
{
    st->muprev = 0.;
    st->muprev2 = 0.;
}

double rpois(double mu)
{
    /* Factorial Table (0:9)! */
    const static double fact[10] =
    {
	1., 1., 2., 6., 24., 120., 720., 5040., 40320., 362880.
    };

    /* Local Vars  [initialize some for -Wall]: */
    double del, difmuk= 0., E= 0., fk= 0., fx, fy, g, px, py, t, u= 0., v, x;
    double pois = -1.;
    int k, kflag, big_mu, new_big_mu = FALSE;

    if (!R_FINITE(mu) || mu < 0)
	ML_WARN_return_NAN;

    if (mu <= 0.)
	return 0.;

    /* Per-thread state, persistent between calls for the same mu : */
    Rmath_tls *tls = Rmath_tls_get();
    if (!tls) ML_WARN_return_NAN;
    struct rpois_state *st = &tls->rpois;

    big_mu = mu >= 10.;
    if(big_mu)
	new_big_mu = FALSE;

    if (!(big_mu && mu == st->muprev)) {/* maybe compute new persistent par.s */

	if (big_mu) {
	    new_big_mu = TRUE;
	    /* Case A. (recalculation of s,d,l	because mu has changed):
	     * The poisson probabilities pk exceed the discrete normal
	     * probabilities fk whenever k >= m(mu).
	     */
	    st->muprev = mu;
	    st->s = sqrt(mu);
	    st->d = 6. * mu * mu;
	    st->big_l = floor(mu - 1.1484);
	    /* = an upper bound to m(mu) for all mu >= 10.*/
	}
	else { /* Small mu ( < 10) -- not using normal approx. */

	    /* Case B. (start new table and calculate p0 if necessary) */

	    /*muprev = 0.;-* such that next time, mu != muprev ..*/
	    if (mu != st->muprev) {
		st->muprev = mu;
		st->m = imax2(1, (int) mu);
		st->l = 0; /* pp[] is already ok up to pp[l] */
		st->q = st->p0 = st->p = exp(-mu);
	    }

	    repeat {
		/* Step U. uniform sample for inversion method */
		u = unif_rand();
		if (u <= st->p0)
		    return 0.;

		/* Step T. table comparison until the end pp[l] of the
		   pp-table of cumulative poisson probabilities
		   (0.458 > ~= pp[9](= 0.45792971447) for mu=10 ) */
		if (st->l != 0) {
		    for (k = (u <= 0.458) ? 1 : imin2(st->l, st->m);  k <= st->l; k++)
			if (u <= st->pp[k])
			    return (double)k;
		    if (st->l == 35) /* u > pp[35] */
			continue;
		}
		/* Step C. creation of new poisson
		   probabilities p[l..] and their cumulatives q =: pp[k] */
		st->l++;
		for (k = st->l; k <= 35; k++) {
		    st->p *= mu / k;
		    st->q += st->p;
		    st->pp[k] = st->q;
		    if (u <= st->q) {
			st->l = k;
			return (double)k;
		    }
		}
		st->l = 35;
	    } /* end(repeat) */
	}/* mu < 10 */

    } /* end {initialize persistent vars} */

/* Only if mu >= 10 : ----------------------- */

    /* Step N. normal sample */
    g = mu + st->s * norm_rand();/* norm_rand() ~ N(0,1), standard normal */

    if (g >= 0.) {
	pois = floor(g);
	/* Step I. immediate acceptance if pois is large enough */
	if (pois >= st->big_l)
	    return pois;
	/* Step S. squeeze acceptance */
	fk = pois;
	difmuk = mu - fk;
	u = unif_rand(); /* ~ U(0,1) - sample */
	if (st->d * u >= difmuk * difmuk * difmuk)
	    return pois;
    }

    /* Step P. preparations for steps Q and H.
       (recalculations of parameters if necessary) */

    if (new_big_mu || mu != st->muprev2) {
        /* Careful! muprev2 is not always == muprev
	   because one might have exited in step I or S
	   */
        st->muprev2 = mu;
	st->omega = M_1_SQRT_2PI / st->s;
	/* The quantities b1, b2, c3, c2, c1, c0 are for the Hermite
	 * approximations to the discrete normal probabilities fk. */

	st->b1 = one_24 / mu;
	st->b2 = 0.3 * st->b1 * st->b1;
	st->c3 = one_7 * st->b1 * st->b2;
	st->c2 = st->b2 - 15. * st->c3;
	st->c1 = st->b1 - 6. * st->b2 + 45. * st->c3;
	st->c0 = 1. - st->b1 + 3. * st->b2 - 15. * st->c3;
	st->c = 0.1069 / mu; /* guarantees majorization by the 'hat'-function. */
    }

    if (g >= 0.) {
	/* 'Subroutine' F is called (kflag=0 for correct return) */
	kflag = 0;
	goto Step_F;
    }


    repeat {
	/* Step E. Exponential Sample */

	E = exp_rand();	/* ~ Exp(1) (standard exponential) */

	/*  sample t from the laplace 'hat'
	    (if t <= -0.6744 then pk < fk for all mu >= 10.) */
	u = 2 * unif_rand() - 1.;
	t = 1.8 + fsign(E, u);
	if (t > -0.6744) {
	    pois = floor(mu + st->s * t);
	    fk = pois;
	    difmuk = mu - fk;

	    /* 'subroutine' F is called (kflag=1 for correct return) */
	    kflag = 1;

	  Step_F: /* 'subroutine' F : calculation of px,py,fx,fy. */

	    if (pois < 10) { /* use factorials from table fact[] */
		px = -mu;
		py = pow(mu, pois) / fact[(int)pois];
	    }
	    else {
		/* Case pois >= 10 uses polynomial approximation
		   a0-a7 for accuracy when advisable */
		del = one_12 / fk;
		del = del * (1. - 4.8 * del * del);
		v = difmuk / fk;
		if (fabs(v) <= 0.25)
		    px = fk * v * v * (((((((a7 * v + a6) * v + a5) * v + a4) *
					  v + a3) * v + a2) * v + a1) * v + a0)
			- del;
		else /* |v| > 1/4 */
		    px = fk * log(1. + v) - difmuk - del;
		py = M_1_SQRT_2PI / sqrt(fk);
	    }
	    x = (0.5 - difmuk) / st->s;
	    x *= x;/* x^2 */
	    fx = -0.5 * x;
	    fy = st->omega * (((st->c3 * x + st->c2) * x + st->c1) * x + st->c0);
	    if (kflag > 0) {
		/* Step H. Hat acceptance (E is repeated on rejection) */
		if (st->c * fabs(u) <= py * exp(px + E) - fy * exp(fx + E))
		    break;
	    } else
		/* Step Q. Quotient acceptance (rare case) */
		if (fy - u * fy <= py * exp(px - fx))
		    break;
	}/* t > -.67.. */
    }
    return pois;
}
