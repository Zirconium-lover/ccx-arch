/*     CalculiX - A 3-dimensional finite element program                 */
/*              Copyright (C) 1998-2025 Guido Dhondt                     */

/*     This program is free software; you can redistribute it and/or     */
/*     modify it under the terms of the GNU General Public License as    */
/*     published by the Free Software Foundation(version 2);             */

/*     This program is distributed in the hope that it will be useful,   */
/*     but WITHOUT ANY WARRANTY; without even the implied warranty of    */
/*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the     */
/*     GNU General Public License for more details.                      */

/*     You should have received a copy of the GNU General Public License */
/*     along with this program; if not, write to the Free Software       */
/*     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.         */

/*
  THE INNER KRYLOV SOLVE OF THE NEWTON-KRYLOV CORRECTOR
  =====================================================

  This is the arithmetic of the corrector and nothing else: an Arnoldi
  process with Givens rotations, right-preconditioned, driven entirely by
  vectors the caller hands it.  It never touches the mesh, the residual, the
  solver or any state, so it can be - and is - tested against synthetic
  operators with a KNOWN answer, which is the only way to be sure that what
  the Newton loop later does to a 29501-equation model is a Krylov method
  and not something that merely looks like one.

  WHY THE CORRECTOR EXISTS

  Measured on the s3rad target with the committed deck, PARDISO and the
  recorded environment, from one restored state, over an epsilon ladder
  reaching 6.1e-05:

      |R(u + eps p) - (R(u) - eps J p)| / (eps |J p|)   is CONSTANT

  at 0.359 (increment 92, iteration 1) and at 0.870 (increment 93, iteration
  12).  A consistent tangent makes that ratio fall like O(eps).  A constant
  ratio is a FIRST-ORDER error: the assembled J is not dR/du.  And the
  consequence was measured iteration by iteration - the Newton iteration's
  linear convergence rate EQUALS that ratio to six decimals:

      iter   |R_k|/|R_k-1|   defect ratio at iteration k-1
        2      0.347210        0.347210
        3      0.656329        0.656329
        4      0.754053        0.754053
       ...
       12      0.873022        0.873022

  So the corrector cannot be fixed by choosing a better step length along
  that direction, and cutting the increment does not help either: the ratio
  is a property of the operator, not of the step.

  WHAT IT DOES INSTEAD

  It stops asking the assembled tangent for the Jacobian and asks it only to
  precondition.  The Jacobian action is MEASURED from the residual, which is
  the one quantity in the calculation that is not in doubt:

      solve  A z = r0  by GMRES right-preconditioned with J,
      matvec  A d = ( r0 - b(u + sigma d) ) / sigma .

  One matvec is one residual evaluation and one back-substitution against
  the LU PARDISO has already computed; it costs no factorisation.  When the
  tangent IS the derivative the first Krylov vector already solves the
  system, the method returns exactly J^{-1} r0, and nothing changes.

  THE CONTRACT

    nkgmres_start   sets v0 = r0/|r0|
    nkgmres_basis   the vector the caller must precondition (that is v_j)
    nkgmres_slot    where the caller writes d_j = M^{-1} v_j
    nkgmres_absorb  the caller hands back w = A d_j; returns 0 to stop
    nkgmres_solution   z = sum_k y_k d_k
    nkgmres_residual   the current |r0 - A z| estimate, free from the
                       rotations, so no extra evaluation is needed to
                       report how well the inner solve did
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "CalculiX.h"

void nkgmres_reset(nkgmres *g)
{
  g->n=0;g->mmax=0;g->m=0;g->j=0;g->conv=0;g->beta=0.;g->eta=0.1;
  g->v=NULL;g->z=NULL;g->hh=NULL;g->cs=NULL;g->sn=NULL;g->gg=NULL;g->yy=NULL;
}

/* The caller owns the storage: nonlingeo allocates with NNEW so that the
   allocation log sees it, and this unit is then pure arithmetic. */

void nkgmres_attach(nkgmres *g,ITG n,ITG mmax,double *v,double *z,double *hh,
                    double *cs,double *sn,double *gg,double *yy)
{
  g->n=n;g->mmax=mmax;g->m=0;g->j=0;g->conv=0;g->beta=0.;g->eta=0.1;
  g->v=v;g->z=z;g->hh=hh;g->cs=cs;g->sn=sn;g->gg=gg;g->yy=yy;
}

/* Returns 0 when there is nothing to solve. */

ITG nkgmres_start(nkgmres *g,const double *r0,ITG m,double eta)
{
  ITG i;
  double b=0.;

  g->j=0;g->conv=0;
  g->m=(m<1)?1:((m>g->mmax)?g->mmax:m);
  g->eta=((eta>0.)&&(eta<1.))?eta:0.1;
  for(i=0;i<g->n;i++) b+=r0[i]*r0[i];
  b=sqrt(b);
  g->beta=b;
  if(!(b>0.)) return 0;
  for(i=0;i<g->n;i++) g->v[i]=r0[i]/b;
  for(i=0;i<=g->mmax;i++) g->gg[i]=0.;
  g->gg[0]=b;
  return 1;
}

const double *nkgmres_basis(const nkgmres *g){ return &g->v[g->j*g->n]; }

double *nkgmres_slot(nkgmres *g){ return &g->z[g->j*g->n]; }

/* w = A d_j.  Orthogonalise, rotate, test.  Returns 1 to continue, 0 when
   the inner solve is finished - converged, out of budget, or a happy
   breakdown, which is convergence too. */

ITG nkgmres_absorb(nkgmres *g,double *w)
{
  ITG i,k,j=g->j,mm=g->mmax;
  double t,nrm,h,c,s;

  for(k=0;k<=j;k++){
    t=0.;
    for(i=0;i<g->n;i++) t+=w[i]*g->v[k*g->n+i];
    g->hh[k*mm+j]=t;
    for(i=0;i<g->n;i++) w[i]-=t*g->v[k*g->n+i];
  }
  nrm=0.;
  for(i=0;i<g->n;i++) nrm+=w[i]*w[i];
  nrm=sqrt(nrm);
  g->hh[(j+1)*mm+j]=nrm;
  if(nrm>1.e-300){
    for(i=0;i<g->n;i++) g->v[(j+1)*g->n+i]=w[i]/nrm;
  }

  for(k=0;k<j;k++){
    t          = g->cs[k]*g->hh[k*mm+j]     + g->sn[k]*g->hh[(k+1)*mm+j];
    g->hh[(k+1)*mm+j] = -g->sn[k]*g->hh[k*mm+j] + g->cs[k]*g->hh[(k+1)*mm+j];
    g->hh[k*mm+j]=t;
  }
  h=sqrt(g->hh[j*mm+j]*g->hh[j*mm+j]+g->hh[(j+1)*mm+j]*g->hh[(j+1)*mm+j]);
  if(!(h>1.e-300)){ g->j=j+1; return 0; }
  c=g->hh[j*mm+j]/h;
  s=g->hh[(j+1)*mm+j]/h;
  g->cs[j]=c;g->sn[j]=s;
  g->hh[j*mm+j]=h;
  g->hh[(j+1)*mm+j]=0.;
  g->gg[j+1]=-s*g->gg[j];
  g->gg[j]  = c*g->gg[j];

  g->j=j+1;
  if(fabs(g->gg[j+1])<=g->eta*g->beta){ g->conv=1; return 0; }
  if(nrm<=1.e-300){ g->conv=1; return 0; }   /* happy breakdown */
  if(g->j>=g->m) return 0;
  return 1;
}

double nkgmres_residual(const nkgmres *g){ return fabs(g->gg[g->j]); }

/* z = sum_k y_k d_k, with y from the triangular system the rotations left. */

void nkgmres_solution(nkgmres *g,double *x)
{
  ITG i,k,mm=g->mmax;
  double t;

  for(i=0;i<g->n;i++) x[i]=0.;
  if(g->j<1) return;
  for(i=g->j-1;i>=0;i--){
    t=g->gg[i];
    for(k=i+1;k<g->j;k++) t-=g->hh[i*mm+k]*g->yy[k];
    if(fabs(g->hh[i*mm+i])<=1.e-300){ g->yy[i]=0.; }
    else{ g->yy[i]=t/g->hh[i*mm+i]; }
  }
  for(k=0;k<g->j;k++){
    t=g->yy[k];
    if(t==0.) continue;
    for(i=0;i<g->n;i++) x[i]+=t*g->z[k*g->n+i];
  }
}

/* ------------------------------------------------------------------ */
/* Regression test.  Synthetic operators with a known answer.          */
/* ------------------------------------------------------------------ */

#define NKT_N 24
#define NKT_M 12

/* The whole point of the corrector is a preconditioner that is WRONG in a
   few directions and right everywhere else, so the test operator is built
   that way on purpose: M is diagonal, A = M + a low-rank perturbation, and
   the exact answer is known by construction. */

static void nkt_apply(const double *md,const double *u,const double *vv,
                      ITG nrank,const double *x,double *y)
{
  ITG i,r;
  double t;
  for(i=0;i<NKT_N;i++) y[i]=md[i]*x[i];
  for(r=0;r<nrank;r++){
    t=0.;
    for(i=0;i<NKT_N;i++) t+=vv[r*NKT_N+i]*x[i];
    for(i=0;i<NKT_N;i++) y[i]+=u[r*NKT_N+i]*t;
  }
}

static double nkt_run(ITG nrank,double scale,ITG mallow,double eta,
                      ITG *nused,double *rrep)
{
  nkgmres g;
  double md[NKT_N],u[2*NKT_N],vv[2*NKT_N],r0[NKT_N],x[NKT_N],w[NKT_N],ax[NKT_N];
  double *v,*z,*hh,*cs,*sn,*gg,*yy;
  ITG i,r;
  double e=0.,n0=0.;

  v=(double *)calloc((size_t)(NKT_M+1)*NKT_N,sizeof(double));
  z=(double *)calloc((size_t)NKT_M*NKT_N,sizeof(double));
  hh=(double *)calloc((size_t)(NKT_M+1)*NKT_M,sizeof(double));
  cs=(double *)calloc((size_t)NKT_M,sizeof(double));
  sn=(double *)calloc((size_t)NKT_M,sizeof(double));
  gg=(double *)calloc((size_t)(NKT_M+1),sizeof(double));
  yy=(double *)calloc((size_t)NKT_M,sizeof(double));

  for(i=0;i<NKT_N;i++){
    md[i]=1.+0.37*(double)((i*7)%11);
    r0[i]=sin(0.7*(double)i)+0.3*cos(2.1*(double)i);
  }
  for(r=0;r<2;r++){
    for(i=0;i<NKT_N;i++){
      u[r*NKT_N+i]=scale*sin(1.3*(double)i+2.0*(double)r);
      vv[r*NKT_N+i]=cos(0.9*(double)i-1.1*(double)r);
    }
  }

  nkgmres_reset(&g);
  nkgmres_attach(&g,NKT_N,NKT_M,v,z,hh,cs,sn,gg,yy);
  if(nkgmres_start(&g,r0,mallow,eta)!=0){
    while(1){
      const double *b=nkgmres_basis(&g);
      double *d=nkgmres_slot(&g);
      for(i=0;i<NKT_N;i++) d[i]=b[i]/md[i];        /* M^{-1} v_j */
      nkt_apply(md,u,vv,nrank,d,w);                /* w = A d_j  */
      if(nkgmres_absorb(&g,w)==0) break;
    }
  }
  *nused=g.j;
  *rrep=nkgmres_residual(&g);
  nkgmres_solution(&g,x);
  nkt_apply(md,u,vv,nrank,x,ax);
  for(i=0;i<NKT_N;i++){
    e+=(r0[i]-ax[i])*(r0[i]-ax[i]);
    n0+=r0[i]*r0[i];
  }
  free(v);free(z);free(hh);free(cs);free(sn);free(gg);free(yy);
  return sqrt(e)/sqrt(n0);
}

ITG nkgmres_selftest(void)
{
  ITG nbad=0,nu;
  double rel,rep;

  printf("[NKGMRES] self test\n");

  /* ---- A: preconditioner EXACT -> one iteration, exact answer ------
     This is the case that must be inert: where the assembled tangent is
     the Jacobian the corrector has to reproduce the plain step. */

  rel=nkt_run(0,0.,NKT_M,1.e-12,&nu,&rep);
  printf("   %-34s iterations=%" ITGFORMAT " |r0-Ax|/|r0|=%.3e  %s\n",
         "A exact preconditioner",nu,rel,
         ((nu==1)&&(rel<1.e-12))?"ok":"FAIL");
  if(!((nu==1)&&(rel<1.e-12))) nbad++;

  /* ---- B: rank-1 error -> at most two iterations, exact ------------ */

  rel=nkt_run(1,0.8,NKT_M,1.e-12,&nu,&rep);
  printf("   %-34s iterations=%" ITGFORMAT " |r0-Ax|/|r0|=%.3e  %s\n",
         "B rank-1 preconditioner error",nu,rel,
         ((nu<=2)&&(rel<1.e-10))?"ok":"FAIL");
  if(!((nu<=2)&&(rel<1.e-10))) nbad++;

  /* ---- C: rank-2 error -> at most three iterations, exact ---------- */

  rel=nkt_run(2,0.8,NKT_M,1.e-12,&nu,&rep);
  printf("   %-34s iterations=%" ITGFORMAT " |r0-Ax|/|r0|=%.3e  %s\n",
         "C rank-2 preconditioner error",nu,rel,
         ((nu<=3)&&(rel<1.e-10))?"ok":"FAIL");
  if(!((nu<=3)&&(rel<1.e-10))) nbad++;

  /* ---- D: the reported residual is the true one -------------------- */
  /* The rotations carry |r0-Ax| for free.  If that estimate and the
     measured norm disagree the log would report a quality the step does
     not have, so the two are compared rather than trusted. */

  rel=nkt_run(2,0.8,2,1.e-12,&nu,&rep);
  {
    double n0=0.,i0;
    ITG i;
    for(i=0;i<NKT_N;i++){
      i0=sin(0.7*(double)i)+0.3*cos(2.1*(double)i);
      n0+=i0*i0;
    }
    n0=sqrt(n0);
    printf("   %-34s reported=%.6e measured=%.6e  %s\n",
           "D residual estimate is exact",rep,rel*n0,
           (fabs(rep-rel*n0)<=1.e-8*n0)?"ok":"FAIL");
    if(!(fabs(rep-rel*n0)<=1.e-8*n0)) nbad++;
  }

  /* ---- E: a budget of one is still a descent of the linear residual - */
  /* With one iteration GMRES is the exact least-squares step along the
     preconditioned direction, so it can never be worse than the plain
     preconditioned step, which is the step the solver takes today. */

  {
    double r1,rm;
    ITG n1,nm;
    r1=nkt_run(2,0.8,1,1.e-12,&n1,&rep);
    rm=nkt_run(2,0.8,NKT_M,1.e-12,&nm,&rep);
    printf("   %-34s m=1 gives %.4e, m=%" ITGFORMAT " gives %.4e  %s\n",
           "E more iterations never hurt",r1,nm,rm,
           (rm<=r1+1.e-14)?"ok":"FAIL");
    if(!(rm<=r1+1.e-14)) nbad++;
  }

  /* ---- F: eta stops the inner solve early -------------------------- */

  rel=nkt_run(2,0.8,NKT_M,0.5,&nu,&rep);
  printf("   %-34s iterations=%" ITGFORMAT " residual/|r0|<=0.5  %s\n",
         "F eta is honoured",nu,(rel<=0.5)?"ok":"FAIL");
  if(!(rel<=0.5)) nbad++;

  printf("[NKGMRES] self test %s (%" ITGFORMAT " failure(s))\n",
         (nbad==0)?"PASSED":"FAILED",nbad);
  return nbad;
}
