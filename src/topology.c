/*     CalculiX - damage/fracture extension                              */
/*     topology.c: the transaction that commits an erosion.              */

/* Why this module exists
   ----------------------
   handover/11-TOPOLOGY.md has the argument; the short form is a count.
   nonlingeo.c held 139 references to nine `damage_tent_*` locals, and with
   no object to call, two blocks got written more than once:

     - "discard the transaction" - four SFREE, four =NULL and count=0 -
       appeared FOUR times;
     - "collect what was eroded" - forty-five lines - appeared TWICE,
       differing by one blank line and one closing brace.

   Nobody chose to write the collection loop twice.  It happened because
   there was nothing to call.

   Contract
   --------
     - the marked set is one object with one lifetime, on the DMLabel
       pattern (PETSc include/petscdmlabel.h): created once, passed around,
       discarded by one call.  Freeing three of the four arrays and leaving
       the fourth is no longer expressible;
     - discard is idempotent and safe on a transaction that was never
       opened, because four copies of a lifetime is exactly the shape that
       produces a double free;
     - it records; it does not decide.  What counts as eroded is the damage
       model's answer and this object observes it.

   What this is NOT
   ----------------
   Not a rollback.  Discarding frees the marked set; it does not restore
   ipkon, and there is no code here that pretends it does.  Not the
   sparsity decision either - a committed batch is when the symbolic
   factorization stops being valid, which is a behaviour change with its
   own hypothesis and not part of an extraction.                        */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "CalculiX.h"

void topo_txn_init(topo_txn *t)
{
  t->elem=NULL; t->mat=NULL; t->ip=NULL; t->value=NULL;
  t->count=0; t->step=0; t->increment=0;
  t->step_time=0.; t->total_time=0.;
}

/* Release the marked set.  This is the one that was written out four
   times.  Safe on a transaction that holds nothing, and safe twice: the
   pointers are nulled as they go, which is the property the four copies
   each had to remember separately. */
void topo_txn_discard(topo_txn *t)
{
  if(t->elem!=NULL){
    SFREE(t->elem);  t->elem=NULL;
    SFREE(t->mat);   t->mat=NULL;
    SFREE(t->ip);    t->ip=NULL;
    SFREE(t->value); t->value=NULL;
  }
  t->count=0;
}

/* ---------------------------------------------------------------- tests */

static ITG topo_chki(const char *name,ITG got,ITG want,ITG *nbad)
{
  ITG ok=(got==want);
  printf("   %-36s got=%-10" ITGFORMAT " want=%-10" ITGFORMAT " %s%s",
         name,got,want,ok?"ok":"*** FAIL ***","\n");
  if(!ok) (*nbad)++;
  return ok;
}

ITG topo_selftest(void)
{
  ITG nbad=0;
  topo_txn t;

  printf("[TOPOLOGY] self test%s","\n");

  topo_txn_init(&t);
  topo_chki("a fresh transaction holds nothing",
            (t.elem==NULL)&&(t.count==0),1,&nbad);

  /* discarding a transaction that was never opened is a no-op, not a
     crash - the case four copies of the block each had to get right */
  topo_txn_discard(&t);
  topo_chki("discard of an empty transaction is safe",
            (t.elem==NULL)&&(t.count==0),1,&nbad);

  NNEW(t.elem,ITG,4); NNEW(t.mat,ITG,4); NNEW(t.ip,ITG,4);
  NNEW(t.value,double,4);
  t.count=4; t.step=2; t.increment=17;
  t.elem[0]=11; t.value[3]=0.5;
  topo_chki("an opened transaction holds its marks",
            (t.elem!=NULL)&&(t.count==4)&&(t.elem[0]==11),1,&nbad);

  topo_txn_discard(&t);
  topo_chki("discard releases every array",
            (t.elem==NULL)&&(t.mat==NULL)&&(t.ip==NULL)&&(t.value==NULL),
            1,&nbad);
  topo_chki("discard zeroes the count",t.count,0,&nbad);

  /* the stamp survives discard: what was committed and when is not
     invalidated by releasing the marked set */
  topo_chki("discard leaves the stamp alone",
            (t.step==2)&&(t.increment==17),1,&nbad);

  /* twice, because four copies of a lifetime is the shape that produces a
     double free and this is the check that would have caught it */
  topo_txn_discard(&t);
  topo_chki("discard is idempotent",
            (t.elem==NULL)&&(t.count==0),1,&nbad);

  printf("[TOPOLOGY] self test %s (%" ITGFORMAT " failure(s))%s",
         nbad?"FAILED":"PASSED",nbad,"\n");
  return nbad;
}
