/*     Monitor: presenting a quantity, which is not the same job as
 *     producing one.
 *
 *     The diagnostics in this tree are printf calls interleaved with the
 *     solve, in ad-hoc formats, gated by environment variables.
 *     02-DIAGNOSTICS.md is eleven readings a human performs by eye, and it
 *     cannot be anything else while the output is prose.
 *
 *     PETSc's separation is the model: a monitor PRODUCES nothing and a
 *     viewer DECIDES nothing.  Here that means census.c computes and this
 *     file prints, and it prints twice - the line the tree already had, byte
 *     for byte, and a machine-readable record beside it.
 *
 *     The legacy line is preserved exactly on purpose.  test/regress/run.py
 *     reads below_1e-3 and worst=node_N_at_R out of it to pin two of the
 *     nine gate cases; changing that format would have made this extraction
 *     a behaviour change wearing a refactor's clothes.
 */
#include <stdio.h>
#include "CalculiX.h"

/*  One JSON object per line, prefixed so it can be grepped out of a log that
 *  also carries the solver's own chatter:
 *
 *      grep '^\[MON\] ' run.log | jq -s '...'
 *
 *  This is what turns 02-DIAGNOSTICS.md from a document into a program.
 */
void monitor_stiffness(const stiffcensus *c,ITG iinc,double time){
  /* the line as it has always been - the gate parses it */
  printf("[DAMAGE STIFFNESS] inc=%" ITGFORMAT " time=%.12e "
         "below_1e-3=%" ITGFORMAT " below_1e-2=%" ITGFORMAT
         " below_1e-1=%" ITGFORMAT " nonpositive=%" ITGFORMAT
         " worst=node_%" ITGFORMAT "_at_%.4e\n",
         iinc,time,c->below1,c->below2,c->below3,c->nonpositive,
         c->worst,c->worstratio);
  /* and the same numbers in a form nothing has to be taught to read */
  printf("[MON] {\"kind\":\"stiffness\",\"inc\":%" ITGFORMAT
         ",\"time\":%.12e,\"below_1e-3\":%" ITGFORMAT
         ",\"below_1e-2\":%" ITGFORMAT ",\"below_1e-1\":%" ITGFORMAT
         ",\"nonpositive\":%" ITGFORMAT ",\"nodes\":%" ITGFORMAT
         ",\"worst_node\":%" ITGFORMAT ",\"worst_ratio\":%.6e}\n",
         iinc,time,c->below1,c->below2,c->below3,c->nonpositive,
         c->nnode,c->worst,c->worstratio);
  fflush(stdout);
}
