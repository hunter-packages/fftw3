/*
 * Copyright (c) 2002 Matteo Frigo
 * Copyright (c) 2002 Steven G. Johnson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/* $Id: ct.c,v 1.4 2002-06-08 20:57:54 athena Exp $ */

/* generic Cooley-Tukey routines */
#include "dft.h"
#include "ct.h"

#define OPTIMAL_SIZE 12		/* for estimator */

static void destroy(plan *ego_)
{
     plan_ct *ego = (plan_ct *) ego_;

     fftw_twiddle_destroy(ego->td);
     fftw_plan_destroy(ego->cld);
     fftw_stride_destroy(ego->ios);
     fftw_stride_destroy(ego->vs);
     fftw_free(ego);
}

static void awake(plan *ego_, int flg)
{
     plan_ct *ego = (plan_ct *) ego_;
     plan *cld = ego->cld;

     cld->adt->awake(cld, flg);

     if (flg) {
	  if (!ego->td) {
	       ego->td = fftw_mktwiddle(ego->slv->desc->tw, ego->r, ego->m);
	       ego->W = ego->td->W;	/* cache for efficiency */
	  }
     } else {
	  if (ego->td)
	       fftw_twiddle_destroy(ego->td);
	  ego->td = 0;
	  ego->W = 0;
     }
}

static void print(plan *ego_, plan_printf prntf)
{
     plan_ct *ego = (plan_ct *) ego_;
     const solver_ct *slv = ego->slv;
     const ct_desc *e = slv->desc;

     prntf("(%s-%d/%d", slv->nam, ego->r, fftw_twiddle_length(e->tw));
     if (e->is || e->vs)
	  prntf("/is=%d/vs=%d", e->is, e->vs);
     if (ego->vl > 1)
	  prntf("-x%u", ego->vl);
     prntf(" ");
     ego->cld->adt->print(ego->cld, prntf);
     prntf(")");
}

#define divides(a, b) (((uint)(b) % (uint)(a)) == 0)

int fftw_dft_ct_applicable(const solver_ct *ego, const problem *p_)
{
     if (DFTP(p_)) {
	  const problem_dft *p = (const problem_dft *) p_;
	  const ct_desc *e = ego->desc;
	  return (1
		  && p->sz.rnk == 1
		  && p->vecsz.rnk <= 1 
		  && divides(e->radix, p->sz.dims[0].n)
	       );
     }
     return 0;
}


static const plan_adt padt = {
     fftw_dft_solve,
     awake,
     print,
     destroy
};

plan *fftw_mkplan_dft_ct(const solver_ct *ego, 
			 const problem *p_,
			 planner *plnr, 
			 const ctadt *adt)
{
     plan_ct *pln;
     plan *cld;
     uint n, r, m;
     problem *cldp;
     iodim *d, *vd;
     const problem_dft *p;
     const ct_desc *e = ego->desc;

     if (!adt->applicable(ego, p_))
	  return (plan *) 0;

     p = (const problem_dft *) p_;
     d = p->sz.dims;
     vd = p->vecsz.dims;
     n = d[0].n;
     r = e->radix;
     m = n / r;

     cldp = adt->mkcld(ego, p);
     cld = plnr->adt->mkplan(plnr, cldp);
     fftw_problem_destroy(cldp);

     if (!cld)
	  return (plan *) 0;

     pln = MKPLAN_DFT(plan_ct, &padt, adt->apply);

     pln->slv = ego;
     pln->cld = cld;
     pln->k = ego->k;
     pln->r = r;
     pln->m = m;

     pln->is = d[0].is;
     pln->os = d[0].os;

     pln->ios = pln->vs = 0;

     if (p->vecsz.rnk == 1) {
	  pln->vl = vd[0].n;
	  pln->ivs = vd[0].is;
	  pln->ovs = vd[0].os;
     } else {
	  pln->vl = 1;
	  pln->ivs = pln->ovs = 0;
     }

     pln->td = 0;
     pln->super.super.cost =
	 1.0 + 0.1 * fftw_square(e->radix - OPTIMAL_SIZE) + cld->cost;

     adt->finish(pln);

     return &(pln->super.super);
}

solver *fftw_mksolver_dft_ct(union kct k, const ct_desc *desc,
			     const char *nam, const solver_adt *adt)
{
     solver_ct *slv;

     slv = MKSOLVER(solver_ct, adt);

     slv->desc = desc;
     slv->k = k;
     slv->nam = nam;
     return &(slv->super);
}
