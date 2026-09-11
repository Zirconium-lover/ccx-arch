/*     The declarations: what an option is, not merely that it exists.
 *
 *     Hand written, and deliberately not generated - a declaration scraped
 *     from the line that reads it is the same information the tree already
 *     had.  Each entry states the type, the legal spellings, the range, the
 *     default and one line of prose, and docs/SWITCHES.md is generated FROM
 *     here rather than scraped from getenv.
 *
 *     Scope of this first batch: every option that any test in this tree or
 *     test/s3rad/run_s3rad.sh sets - that is, every option whose behaviour
 *     is actually pinned by something.  The other 116 names the binary reads
 *     are reported as UNDECLARED, and that list is the retirement queue.
 *
 *     RANGES record what the code does, including where it does it silently.
 *     CCX_DAMAGE_AUTOSPC is clamped at 1.e-1 inside nonlingeo.c with no
 *     message (05-DEBT.md item 7): a deck whose worst node sits at 1.04e-01
 *     masks nobody and gives no indication why.  Declaring the range does
 *     not fix that; it makes it visible in one place instead of none.
 *
 *     SPELLINGS are recorded as they are, not as they should be.  Four
 *     mutually incompatible notions of "true" are in use here.  Writing them
 *     down is the first step to having one.
 */
#ifndef CCXOPT_DECL_H
#define CCXOPT_DECL_H

typedef enum{
  CCXOPT_BOOL, CCXOPT_INT, CCXOPT_REAL, CCXOPT_ENUM, CCXOPT_STRING
}ccxopt_type;

typedef struct{
  const char *name;
  ITG type;
  const char *dflt;        /* the default, as text                        */
  double lo,hi;            /* inclusive range; lo>hi means unbounded      */
  const char *choices;     /* "A|B|C" for CCXOPT_ENUM, else NULL          */
  const char *doc;         /* one line, and it has to be true             */
  const char *deprecated;  /* replacement or reason, or NULL              */
}ccxopt_decl;

#define CCXOPT_UNBOUNDED 1.,0.

static const ccxopt_decl ccxopt_decl_table[]={

/* ---- the load-path judgement (damstate.c) ------------------------- */
{"CCX_DAMAGE_AUTOSPC",CCXOPT_REAL,"unset (no mask)",0.,1.e-1,NULL,
 "fraction of a node's OWN intact assembled diagonal below which it is "
 "judged to have lost its load path and is excluded from the displacement "
 "residual; silently clamped to 1.e-1 in nonlingeo.c, so a deck whose worst "
 "node is at 1.04e-01 can never exercise it",NULL},

{"CCX_DAMAGE_AUTOSPC_FORCE",CCXOPT_BOOL,"unset (off)",CCXOPT_UNBOUNDED,NULL,
 "extend the same judgement to the FORCE residual.  Set to anything.  Prints "
 "the excluded residual next to the criterion: if that number stops "
 "returning to zero the mechanism has become a fiction and is hiding a real "
 "imbalance (02-DIAGNOSTICS.md section 4)",NULL},

{"CCX_DAMAGE_DEADALL",CCXOPT_REAL,"unset (off)",0.,0.5,NULL,
 "a node whose entire live support is dead below this fraction is treated as "
 "having none; clamped to [0,0.5] in nonlingeo.c",NULL},

/* ---- erosion and topology ----------------------------------------- */
{"CCX_DAMAGE_DELETE_MAT",CCXOPT_STRING,"unset (no filter)",CCXOPT_UNBOUNDED,NULL,
 "restrict terminal deletion to elements of these materials; ALL means every "
 "material",NULL},

{"CCX_DAMAGE_TOPOLOGY",CCXOPT_ENUM,"immediate",CCXOPT_UNBOUNDED,
 "DEFERRED|deferred|1",
 "DEFERRED batches topology changes into one transaction committed at the "
 "end of the increment instead of applying each deletion as it is found",NULL},

{"CCX_FRACTURE_TERMINATION",CCXOPT_STRING,"unset (never terminates)",
 CCXOPT_UNBOUNDED,NULL,
 "SETA:SETB - stop the run when no load path remains between these two node "
 "sets.  This is the only thing standing between a run and the phantom "
 "regime of 05-DEBT.md item 1",NULL},

{"CCX_FRACTURE_DEADFACET",CCXOPT_BOOL,"unset (off)",CCXOPT_UNBOUNDED,NULL,
 "exclude a cohesive facet whose every integration point has failed from the "
 "load path used by the termination test.  Any value except the string 0 "
 "means on",NULL},

{"CCX_FRACTURE_LINK",CCXOPT_ENUM,"NODE",CCXOPT_UNBOUNDED,
 "NODE|node|FACE|face",
 "what counts as a connection when the termination test walks the live bulk: "
 "sharing a node, or sharing a whole face",NULL},

/* ---- globalization ------------------------------------------------ */
{"CCX_DAMAGE_LINESEARCH",CCXOPT_ENUM,"off",CCXOPT_UNBOUNDED,
 "ADAPTIVE|adaptive|1",
 "arm the adaptive damage line-search ladder (lsladder.c).  Measured cost: "
 "the ladder and the rescues together consume 4.6% of the run "
 "(research/01-PROFILING.md)",NULL},

{"CCX_DAMAGE_REEQ_RESCUE2",CCXOPT_BOOL,"unset (off)",CCXOPT_UNBOUNDED,NULL,
 "arm Rescue level 2 with an event step.  Set to anything, INCLUDING 0",NULL},

{"CCX_DAMAGE_REEQ_SCALE",CCXOPT_ENUM,"off",CCXOPT_UNBOUNDED,
 "PHYSICAL|physical|1",
 "scale the re-equilibration step by a physical length rather than by the "
 "residual norm",NULL},

{"CCX_DAMAGE_TR_DOGLEG",CCXOPT_BOOL,"unset (off)",CCXOPT_UNBOUNDED,NULL,
 "arm the dogleg trust region as rescue level 3.  Set to anything, INCLUDING "
 "0.  Refuses to arm without CCX_DAMAGE_REEQ_RESCUE2: it is a level on top "
 "of Rescue 2, not a replacement",NULL},

/* ---- path following ----------------------------------------------- */
{"CCX_PATHFOLLOW",CCXOPT_REAL,"unset (off)",CCXOPT_UNBOUNDED,NULL,
 "dissipation path following: tau per increment.  Must be strictly positive; "
 "the code refuses to arm otherwise",NULL},

{"CCX_PATHFOLLOW_DTHETA",CCXOPT_REAL,"1.e-3",CCXOPT_UNBOUNDED,NULL,
 "the load-factor increment at which path following engages; a non-positive "
 "value falls back to the default",NULL},

{"CCX_CRACK_CONTROL",CCXOPT_REAL,"unset (off)",CCXOPT_UNBOUNDED,NULL,
 "crack control: the control increment per step, which must be strictly "
 "positive or the mechanism refuses to arm.  Needs UC6 elements present and "
 "its own kinematics self test to pass.  Mutually exclusive with "
 "CCX_PATHFOLLOW_COD; the code refuses to arm if both are set",NULL},

{"CCX_CRACK_CONTROL_ENGAGE",CCXOPT_INT,"0",CCXOPT_UNBOUNDED,NULL,
 "increment at which crack control engages",NULL},

{"CCX_DISSIPATION_CONTROL",CCXOPT_ENUM,"unset (off)",CCXOPT_UNBOUNDED,"1|2",
 "dissipation control mode; 2 assembles the f_hat = dR/dlambda vector.  Inert "
 "unless CCX_DISSIPATION_TARGET is positive",NULL},

{"CCX_DISSIPATION_TARGET",CCXOPT_REAL,"unset (off)",CCXOPT_UNBOUNDED,NULL,
 "dissipation target per increment; a positive value also turns the "
 "dissipation report on",NULL},

/* ---- the operator ------------------------------------------------- */
{"CCX_DAMAGE_TANGENT",CCXOPT_ENUM,"stock symmetric",CCXOPT_UNBOUNDED,
 "FD_SYM|fd_sym|1|UNSYM|unsym|2",
 "which tangent to assemble: FD_SYM (1) a finite-difference symmetric "
 "tangent, UNSYM (2) the asymmetric path with the constitutive tangent left "
 "untouched.  NOTHING IN THIS TREE VERIFIES THE TANGENT AGAINST THE "
 "RESIDUAL (07-RESEARCH-AGENDA.md rank 3)",NULL},

{"CCX_DAMAGE_VISCOSITY",CCXOPT_REAL,"0 (off)",CCXOPT_UNBOUNDED,NULL,
 "viscous regularisation eta for the damage evolution; negative values are "
 "clamped to zero",NULL},

/* ---- the constitutive law ----------------------------------------- */
{"CCX_UC6_CONTACT_SMOOTH",CCXOPT_REAL,"unset (sharp law)",CCXOPT_UNBOUNDED,NULL,
 "penetration band over which the crack-face closure kink is blended.  The "
 "kink is a measured factor of 1/gmin = 1e+06 in the normal tangent; the "
 "blend is a byte-for-byte no-op wherever no DAMAGED facet closes",NULL},

/* ---- the linear solver -------------------------------------------- */
{"CCX_PARDISO_REUSE_SYMBOLIC",CCXOPT_ENUM,"off",CCXOPT_UNBOUNDED,
 "1|ON|on|YES|yes",
 "keep PARDISO's symbolic factorisation across numerical factorisations, "
 "keyed on a hash of the sparsity pattern.  MEASURED: it retains the "
 "analysis for 603 of 606 factorisations on fast-wrapped and buys nothing "
 "outside run-to-run noise (handover/04-REFUTED.md)",NULL},

/* ---- operator verification ----------------------------------------
   Found, not built: the directional-derivative check that
   07-RESEARCH-AGENDA.md rank 3 calls "the named hole in the diagnostics"
   was already here, under three undocumented names that no test set.  It is
   PETSc's -snes_test_jacobian in all but the name.  Declared rather than
   retired, because a tool nobody could find is not the same thing as a tool
   that does not exist.  What it says: research/03-OPERATOR.md. */
{"CCX_STRUCT_FD_INC",CCXOPT_INT,"0 (off)",CCXOPT_UNBOUNDED,NULL,
 "arm the operator check at this increment: compare the ASSEMBLED tangent, "
 "column by column, against a central difference of the internal force.  The "
 "run stops afterwards - the probe perturbs the displacement repeatedly, so "
 "the run is diagnostic only",NULL},

{"CCX_STRUCT_FD_ITER",CCXOPT_INT,"0",CCXOPT_UNBOUNDED,NULL,
 "the Newton iteration at which the operator check runs",NULL},

{"CCX_STRUCT_FD_STEP",CCXOPT_INT,"0 (any step)",CCXOPT_UNBOUNDED,NULL,
 "restrict the operator check to this *STEP.  iinc restarts at 1 in every "
 "step, so without this the probe can only ever fire in the first one - "
 "which is the wrong one whenever the interesting state is reached by "
 "unloading, as the crack-face closure benchmark is",NULL},

{"CCX_STRUCT_FD_H",CCXOPT_REAL,"1.e-7",CCXOPT_UNBOUNDED,NULL,
 "the perturbation of the central difference.  Measured: the answer is flat "
 "over 1e-11 to 1e-8 and has moved by 1e-3, so the default sits inside the "
 "converged plateau",NULL},

/* ---- diagnostics that were in the retirement queue only because nobody
   had written them down.  Each answers a question 02-DIAGNOSTICS.md asks a
   human to answer by eye. */
{"CCX_DAMAGE_NODE_DUMP",CCXOPT_INT,"0 (off)",CCXOPT_UNBOUNDED,NULL,
 "dump one named node every increment: every element that touches it, its "
 "type, whether it is still assembled, the damage at each integration point "
 "of a cohesive element, and the assembled diagonal beside its intact "
 "reference.  Global counters have twice disagreed with each other here; "
 "this asks the question directly instead",NULL},

{"CCX_DAMAGE_NODE_INC",CCXOPT_INT,"1",CCXOPT_UNBOUNDED,NULL,
 "the increment from which CCX_DAMAGE_NODE_DUMP starts printing",NULL},

{"CCX_DAMAGE_STIFF_PROBE",CCXOPT_BOOL,"unset (off)",CCXOPT_UNBOUNDED,NULL,
 "print the stiffness census - how many nodes have lost what fraction of "
 "their OWN intact assembled diagonal (02-DIAGNOSTICS.md section 3).  Armed "
 "automatically whenever CCX_DAMAGE_AUTOSPC is",NULL},

{"CCX_DAMAGE_STIFF_MIN",CCXOPT_REAL,"0 (off)",0.,0.1,NULL,
 "report nodes whose assembled diagonal has fallen below this fraction of "
 "their intact value; silently clamped to [0,0.1] like CCX_DAMAGE_AUTOSPC",
 NULL},

{"CCX_DAMAGE_AUTOSPC_NEG",CCXOPT_BOOL,"unset (off)",CCXOPT_UNBOUNDED,NULL,
 "count a NON-POSITIVE assembled diagonal as dead as well.  The census "
 "reports nonpositive= separately, and on every deck measured so far it is "
 "zero",NULL},

/* ---- measurement -------------------------------------------------- */
{"CCX_LOG_VIEW_EVERY",CCXOPT_REAL,"600 (ten minutes)",CCXOPT_UNBOUNDED,NULL,
 "seconds between INTERIM profile reports; 0 prints only at exit.  Its "
 "defender: two 2.3-hour runs of the target deck were killed part way "
 "through and produced no profile at all, because the table was printed "
 "from atexit.  A profiler that reports only at the end is useless on "
 "exactly the runs it exists for",NULL},

{"CCX_LOG_VIEW",CCXOPT_BOOL,"unset (off)",CCXOPT_UNBOUNDED,NULL,
 "print where the run spent its time: named events with a call count, an "
 "inclusive and a self time, and who called whom.  Measurement only; the run "
 "is bit-identical with it on",NULL},

};

#define CCXOPT_DECL_COUNT ((ITG)(sizeof(ccxopt_decl_table)/sizeof(ccxopt_decl_table[0])))

#endif
