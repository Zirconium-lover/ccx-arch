/*     CalculiX - damage/fracture extension                              */
/*     converge.c: the numbers the convergence judgement is made from.   */

/* Why this module exists
   ----------------------
   Ask this tree "what counts as converged here" and you arrive at five
   places (handover/10-CONVERGENCE.md section 1).  This file is the first of
   them to get an owner: the REDUCTION - what ram, ram1, ram2, uam and qam
   are, and which degrees of freedom are allowed to contribute to them.

   That is a real responsibility and it had no home.  The clearest symptom:
   CCX_DAMAGE_AUTOSPC_FORCE excludes dofs from the force residual by a
   `continue` in the middle of the loop that computes it.  There is no
   object whose answer that is, so it lives where the loop happened to be -
   and the thing it modifies has no name, which is why it could be measured
   for two days before anyone established that on the target deck it changes
   nothing at all (research/14-THE-TRAP.md).

   Contract
   --------
     - one call per Newton iteration, after the solve and before the
       verdict;
     - it reads the solution vector and the load measures and writes the
       norms; it decides nothing and prints nothing;
     - what it EXCLUDED is part of its state, not a local.  The declaration
       of CCX_DAMAGE_AUTOSPC_FORCE promises the excluded residual is
       reported next to the criterion it was excluded from, and a promise
       like that belongs to the thing that does the excluding;
     - the module refuses to arm if its self test fails, the discipline
       lsladder.c set.

   What this is NOT
   ----------------
   Not the verdict (checkconvergence.c still owns the boolean), not the
   increment control, not the printing.  Those are steps B and C and the
   Increment and Monitor objects; 10-CONVERGENCE.md sections 3 and 4 say
   why they are held back rather than done here.

   Provenance
   ----------
   Transcribed from nonlingeo.c lines 12509-12601 and the two `ram < 1e-6`
   cut-offs that followed them, unchanged.  Bit-identity is the standard for
   this step and it is checked, not assumed: gate and target deck.        */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "CalculiX.h"

void converge_init(converge *c,double qam_floor,ITG mask_force,
                   const ITG *mask,ITG mask_nk)
{
  c->qam_floor=qam_floor;
  c->mask_force=mask_force;
  c->mask=mask;
  c->mask_nk=mask_nk;
  c->qam_peak=0.;
  c->excl_max=0.;
  c->excl_node=0;
  c->excl_count=0;
}

/* One Newton iteration's worth of norms.
 *
 * ram[0],ram[1]  largest |residual| over the mechanical / thermal dofs
 * ram[2],ram[3]  the equation it was found at, carried as k+0.5
 * ram[4],ram[5]  the face-to-face contact divergence pair
 * ram1,ram2      the two previous iterations, for the divergence test
 * uam            high-water mark of the correction
 * qam            running average of the load measure, optionally floored
 *
 * The order is load-bearing and is the order it had inline: the mortar pair
 * is formed from the UNCUT ram[0], and only then is ram cut off at 1e-6. */
void converge_norms(converge *c,const double *b,const ITG *neq,
                    const ITG *nactdofinv,ITG mt,ITG ithermal,ITG mortar,
                    ITG ne,ITG ne0,ITG neold,
                    const double *qa,const double *qamold,ITG jnz,
                    double qau,double ea,
                    double *ram,double *ram1,double *ram2,
                    const double *cam,double *uam,double *qam)
{
  ITG k;
  double err;

  /* the correction high-water mark and the load reference */

  if(ithermal!=2){
    if(cam[0]>uam[0]){
      uam[0]=cam[0];}
    if(qau<1.e-10){
      if(qa[0]>ea*qam[0]){
        qam[0]=(qamold[0]*jnz+qa[0])/(jnz+1);}
      else {
        qam[0]=qamold[0];}
    }
    /* CCX_DAMAGE_QAM_FLOOR - DIAGNOSTIC, default off.

       The force criterion is RELATIVE: the verdict tests ram[0] against
       c1[0]*qam[0], and qam[0] is a running average of the internal force
       over increments.  On a specimen that is unloading as it breaks,
       qam[0] follows the load down and the absolute tolerance collapses
       with it.  Measured on run_s0_fine_grad14 at its wall: qa=0.001552,
       qam=0.001604, residual 0.005474 - a miss by 3.4x - while the
       specimen was still carrying 2.33 N, 22.7% of its peak, and the
       residual had just fallen 14x in one iteration.  The run died on a
       vanishing reference, not on a growing residual.

       Flooring qam at a fraction of the largest value it ever reached keeps
       the tolerance tied to the load the specimen ONCE carried.

       THIS CHANGES THE CONVERGENCE CRITERION AND THEREFORE THE ANSWER.  It
       is a diagnostic for whether a given wall is criterial or physical.  A
       run that goes further with it is NOT thereby a success - that has to
       be shown on the physics (E-108) - and adopting it needs the full
       verify + ladder gate. */
    if(c->qam_floor>0.){
      if(qam[0]>c->qam_peak){c->qam_peak=qam[0];}
      if(qam[0]<c->qam_floor*c->qam_peak){
        qam[0]=c->qam_floor*c->qam_peak;}
    }
  }
  if(ithermal>1){
    if(cam[1]>uam[1]){
      uam[1]=cam[1];}
    if(qau<1.e-10){
      if(qa[1]>ea*qam[1]){
        qam[1]=(qamold[1]*jnz+qa[1])/(jnz+1);}
      else {
        qam[1]=qamold[1];}
    }
  }

  /* the two previous residuals, then the new one */

  for(k=0;k<2;++k){
    ram2[k]=ram1[k];
    ram1[k]=ram[k];
    ram[k]=0.;
  }

  if(ithermal!=2){
    c->excl_max=0.;c->excl_node=0;c->excl_count=0;
    for(k=0;k<neq[0];++k){
      err=fabs(b[k]);
      /* CCX_DAMAGE_AUTOSPC_FORCE: a node damstate.c has judged to have lost
         its load path does not get to veto the increment through the force
         norm.  Excluded, not hidden - see excl_* below. */
      if((c->mask_force!=0)&&(c->mask!=NULL)&&(nactdofinv!=NULL)){
        ITG spcnd=nactdofinv[k]/mt;
        if((spcnd>=0)&&(spcnd<c->mask_nk)&&(c->mask[spcnd]!=0)){
          c->excl_count++;
          if(err>c->excl_max){
            c->excl_max=err;c->excl_node=spcnd+1;}
          continue;
        }
      }
      if(err>ram[0]){
        ram[0]=err;
        ram[2]=k+0.5;}
    }
  }
  if(ithermal>1){
    for(k=neq[0];k<neq[1];++k){
      err=fabs(b[k]);
      if(err>ram[1]){
        ram[1]=err;
        ram[3]=k+0.5;}
    }
  }

  /* Divergence criteria for face-to-face penalty is different */

  if(mortar==1){
    for(k=4;k<6;++k){
      ram2[k]=ram1[k];
      ram1[k]=ram[k];
    }
    ram[4]=ram[0]+ram1[0];
    ram[5]=(ne-ne0)-(neold-ne0)+0.5;
  }

  /* next line is inserted to cope with stress-less temperature
     calculations.  AFTER the mortar pair, which is formed from the uncut
     value - that order was in the original and is not incidental. */

  if(ithermal!=2){
    if(ram[0]<1.e-6){
      ram[0]=0.;}
  }
  if(ithermal>1){
    if(ram[1]<1.e-6){
      ram[1]=0.;}
  }
}

/* Present the norms.  Separate from computing them, because "produce a
 * diagnostic" and "present it" are two jobs and this file only does the
 * second one here under protest: the printing belongs to Monitor, and this
 * is a way-station so that nonlingeo() has two named calls where it had
 * ninety lines of arithmetic interleaved with output.
 *
 * Byte-for-byte the text that was inline, including the two blank lines
 * after the correction and the position of fflush.  The AUTOSPC-FORCE line
 * is printed from the object's own excl_* state, which is the whole point:
 * the declaration of that switch promises the excluded residual appears
 * next to the criterion it was excluded from, and now one object both
 * excludes and reports.
 *
 * ran is ctrl[18], the coefficient the force criterion uses, so the printed
 * tolerance is the one the verdict will actually apply. */
void converge_report(const converge *c,const ITG *nactdofinv,ITG mt,
                     ITG ithermal,double ran,
                     const double *qa,const double *qam,const double *ram,
                     const double *cam,const double *uam)
{
  ITG inode,idir;

  if(ithermal!=2){
    printf(" average force= %f\n",qa[0]);
    printf(" time avg. forc= %f\n",qam[0]);
    if((c->mask_force!=0)&&(c->excl_count>0)){
      /* Auditable by construction: the excluded peak is printed next to
         the criterion it was excluded from, so a masked residual that
         starts to grow is visible in the same place ram[0] is read. */
      printf("[DAMAGE AUTOSPC-FORCE] excluded %" ITGFORMAT " dof(s) on "
             "AUTOSPC-masked nodes from ram[0]; largest excluded "
             "residual %.6e at node %" ITGFORMAT " (ram[0]=%.6e, "
             "tolerance %.6e = %.4f x qam)%s",
             c->excl_count,c->excl_max,c->excl_node,
             ram[0],ran*qam[0],
             (qam[0]>0.)?c->excl_max/qam[0]:0.,"\n");
    }
    if((ITG)((double)nactdofinv[(ITG)ram[2]]/mt)+1==0){
      printf(" largest residual force= %f\n",
             ram[0]);
    }else{
      inode=(ITG)((double)nactdofinv[(ITG)ram[2]]/mt)+1;
      idir=nactdofinv[(ITG)ram[2]]-mt*(inode-1);
      printf(" largest residual force= %f in node %" ITGFORMAT
             " and dof %" ITGFORMAT "\n",
             ram[0],inode,idir);
    }
    printf(" largest increment of disp= %e\n",uam[0]);
    if((ITG)cam[3]==0){
      printf(" largest correction to disp= %e\n\n",
             cam[0]);
    }else{
      inode=(ITG)((double)nactdofinv[(ITG)cam[3]]/mt)+1;
      idir=nactdofinv[(ITG)cam[3]]-mt*(inode-1);
      printf(" largest correction to disp= %e in node %" ITGFORMAT
             " and dof %" ITGFORMAT "\n\n",cam[0],inode,idir);
    }
  }
  if(ithermal>1){
    printf(" average flux= %f\n",qa[1]);
    printf(" time avg. flux= %f\n",qam[1]);
    if((ITG)((double)nactdofinv[(ITG)ram[3]]/mt)+1==0){
      printf(" largest residual flux= %f\n",
             ram[1]);
    }else{
      inode=(ITG)((double)nactdofinv[(ITG)ram[3]]/mt)+1;
      idir=nactdofinv[(ITG)ram[3]]-mt*(inode-1);
      printf(" largest residual flux= %f in node %" ITGFORMAT
             " and dof %" ITGFORMAT "\n",ram[1],inode,idir);
    }
    printf(" largest increment of temp= %e\n",uam[1]);
    if((ITG)cam[4]==0){
      printf(" largest correction to temp= %e\n\n",
             cam[1]);
    }else{
      inode=(ITG)((double)nactdofinv[(ITG)cam[4]]/mt)+1;
      idir=nactdofinv[(ITG)cam[4]]-mt*(inode-1);
      printf(" largest correction to temp= %e in node %" ITGFORMAT
             " and dof %" ITGFORMAT "\n\n",cam[1],inode,idir);
    }
  }
  fflush(stdout);
}

/* ---------------------------------------------------------------- tests */

static ITG cvg_chk(const char *name,double got,double want,double tol,
                   ITG *nbad)
{
  ITG ok=(fabs(got-want)<=tol);
  /* %.17g, not %g: the ordering check below differs from its expectation
     by 1e-7 in a number of order 5, and a report that prints "got=5 want=5
     FAIL" reads as a broken test rather than a caught defect. */
  printf("   %-36s got=%-22.17g want=%-22.17g %s%s",
         name,got,want,ok?"ok":"*** FAIL ***","\n");
  if(!ok) (*nbad)++;
  return ok;
}

ITG converge_selftest(void)
{
  ITG nbad=0,neq[2],nactdofinv[24],mt=4,k;
  double b[8],qa[2],qamold[2],ram[6],ram1[6],ram2[6],cam[2],uam[2],qam[2];
  ITG mask[8];
  converge c;

  printf("[CONVERGE] self test%s","\n");

  /* eight mechanical equations on nodes 0..5, mt=4 so node = dof/mt */
  neq[0]=8; neq[1]=8;
  for(k=0;k<8;k++) nactdofinv[k]=mt*(k/2)+1;   /* two dofs per node */
  for(k=0;k<8;k++) b[k]=0.;
  b[1]=-3.0;      /* node 0 */
  b[4]= 7.0;      /* node 2 - the largest                              */
  b[6]= 2.0;      /* node 3 */
  qa[0]=10.; qa[1]=0.;
  qamold[0]=4.; qamold[1]=0.;
  for(k=0;k<6;k++){ram[k]=0.;ram1[k]=0.;ram2[k]=0.;}
  cam[0]=0.5; cam[1]=0.;
  uam[0]=0.2; uam[1]=0.;
  qam[0]=4.;  qam[1]=0.;

  /* A: no mask.  The reduction finds |b| max and where it was. */
  converge_init(&c,0.,0,NULL,0);
  converge_norms(&c,b,neq,nactdofinv,mt,1,0,0,0,0,qa,qamold,1,0.,1.e-2,
                 ram,ram1,ram2,cam,uam,qam);
  cvg_chk("A largest residual",ram[0],7.0,0.,&nbad);
  cvg_chk("A found at equation 4",ram[2],4.5,0.,&nbad);
  cvg_chk("A uam takes the correction",uam[0],0.5,0.,&nbad);
  cvg_chk("A qam running average",qam[0],(4.*1+10.)/(1+1),1.e-14,&nbad);
  cvg_chk("A nothing excluded",(double)c.excl_count,0.,0.,&nbad);

  /* B: mask node 2 - the one carrying the largest residual. */
  for(k=0;k<8;k++) mask[k]=0;
  mask[2]=1;
  qam[0]=4.;uam[0]=0.2;
  converge_init(&c,0.,1,mask,8);
  converge_norms(&c,b,neq,nactdofinv,mt,1,0,0,0,0,qa,qamold,1,0.,1.e-2,
                 ram,ram1,ram2,cam,uam,qam);
  cvg_chk("B masked node does not veto",ram[0],3.0,0.,&nbad);
  cvg_chk("B excluded peak reported",c.excl_max,7.0,0.,&nbad);
  cvg_chk("B excluded node is 1-based",(double)c.excl_node,3.,0.,&nbad);
  cvg_chk("B excluded dof count",(double)c.excl_count,2.,0.,&nbad);

  /* C: the mask is inert unless AUTOSPC_FORCE armed it */
  qam[0]=4.;
  converge_init(&c,0.,0,mask,8);
  converge_norms(&c,b,neq,nactdofinv,mt,1,0,0,0,0,qa,qamold,1,0.,1.e-2,
                 ram,ram1,ram2,cam,uam,qam);
  cvg_chk("C mask off: full residual",ram[0],7.0,0.,&nbad);

  /* D: the history shifts by exactly one iteration */
  cvg_chk("D ram1 is the previous ram",ram1[0],3.0,0.,&nbad);
  cvg_chk("D ram2 is the one before",ram2[0],7.0,0.,&nbad);

  /* E: the 1e-6 cut-off, and that it is applied AFTER the mortar pair */
  for(k=0;k<8;k++) b[k]=0.;
  b[3]=1.e-7;
  for(k=0;k<6;k++){ram[k]=0.;ram1[k]=0.;ram2[k]=0.;}
  ram[0]=5.;                 /* shifts into ram1[0] inside the call       */
  qam[0]=4.;
  converge_init(&c,0.,0,NULL,0);
  converge_norms(&c,b,neq,nactdofinv,mt,1,1,12,10,10,qa,qamold,1,0.,1.e-2,
                 ram,ram1,ram2,cam,uam,qam);
  cvg_chk("E tiny residual is cut to zero",ram[0],0.,0.,&nbad);
  /* ram[4]=ram[0]+ram1[0] is formed BEFORE the cut-off, so it must carry
     the 1e-7 and not a bare 5.0.  This is the one ordering in the block
     that is easy to get wrong on a move and impossible to see afterwards. */
  cvg_chk("E mortar pair used the UNCUT value",ram[4],5.+1.e-7,1.e-12,&nbad);
  cvg_chk("E mortar element count",ram[5],2.5,0.,&nbad);

  /* F: the qam floor holds the reference at a fraction of its peak */
  qam[0]=100.;qamold[0]=100.;qa[0]=1.e-6;
  converge_init(&c,0.25,0,NULL,0);
  converge_norms(&c,b,neq,nactdofinv,mt,1,0,0,0,0,qa,qamold,1,0.,1.e-2,
                 ram,ram1,ram2,cam,uam,qam);
  qamold[0]=1.;
  converge_norms(&c,b,neq,nactdofinv,mt,1,0,0,0,0,qa,qamold,1,0.,1.e-2,
                 ram,ram1,ram2,cam,uam,qam);
  cvg_chk("F qam floored at 0.25 of its peak",qam[0],25.,1.e-12,&nbad);

  printf("[CONVERGE] self test %s (%" ITGFORMAT " failure(s))%s",
         nbad?"FAILED":"PASSED",nbad,"\n");
  return nbad;
}
