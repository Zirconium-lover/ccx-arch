# Next task: diagnose and pass the second `s3rad` wall

## Objective

Starting from the proven line-search fix, determine why stock/rescue stops at
accepted increment 555 (`theta=0.255574`) and implement the smallest justified
algorithmic correction. The eventual engineering objective is a physically
credible complete `s3rad` fracture/separation, not merely a larger increment
number.

Mixed-mode crack control and `src/lsladder.c` are established regressions.
Do not retune them unless a direct measurement invalidates one of their
assumptions.

## Facts already established

- The old wall at `theta~0.21218` is passed at the original
  `CCX_DAMAGE_DEADALL=1.e-2` by extending the backtracking ladder below 0.1 and
  returning the best measured rung.
- The deletion-loop hypothesis was false at the old wall: there was no deletion
  batch on that increment, no repeated `(committed state, batch)`, no orphan
  active DOF and no detached component.
- Those topology measurements apply to the old wall only. Re-measure rather
  than assuming they also describe the new wall.
- At the new wall the corrected ladder finds a contracting trial on every
  iteration at `alpha=0.0035...0.0067`, but the residual contracts by only about
  0.2% per iteration and the full Newton step is about 150 times worse. A
  deeper ladder alone is therefore not a solution.
- The specimen is not separated: the current run stops at `theta=0.255574`.

Full evidence and exact commands are in `test/s3rad/README.md`.

## First diagnostic

Reproduce the new wall with the committed deck, PARDISO and the environment in
`test/s3rad/run_s3rad.sh`. Use stock/rescue; do not engage crack control.
Capture one fully restorable base state near the failing iterations and answer:

1. Is the assembled tangent a directional derivative of the residual there?
   Compare `R(u+eps*p)` with `R(u)+eps*K*p` over an epsilon ladder from the same
   restored state. Check the actual Newton direction and several independent
   directions.
2. Which material/interface active-set changes occur along the line-search
   direction? Count and identify loading/unloading, initiation, full failure
   and deletion transitions for each rung.
3. Where do the residual and Newton correction live? Report the dominant DOFs,
   nodes, elements/materials and their relationship to the fracture front and
   most recent deletion batch.
4. Is the Newton direction a descent direction for the merit function actually
   used by the line search? Measure the directional derivative, not only trial
   norms.
5. At this new wall, do connectivity, active DOFs, sparse structure and several
   deflated null-vector probes still rule out a topological mode? Reuse
   `src/topodiag.c`; do not infer this from the old-wall report.
6. Are all perturbation and rejected-trial history/topology arrays reproducible
   after restore?

Use these measurements to distinguish at least: inconsistent tangent/history
linearisation, nonsmooth active-set switching, loss of rank/topology, and a
merit-function/globalisation defect.

## Acting on the result

- If the tangent is inconsistent, repair the responsible derivative or state
  lifecycle and prove the directional linearisation afterward.
- If a nonsmooth active-set transition is decisive, test an event-aware,
  active-set or semismooth correction. Do not disguise it as another arbitrary
  damping threshold.
- If a topological mode appears at this new wall, handle true orphan DOFs
  explicitly; never silently constrain a physically detached component.
- If all hypotheses are rejected, report that evidence and formulate the next
  smallest discriminating experiment before changing production code.

## Validation and success

The causal A/B must use the same deck, PARDISO binary family and environment;
the only functional difference is the proposed new fix. Keep
`CCX_DAMAGE_DEADALL=1.e-2`, the existing viscosity, tangent, AUTOSPC and stock
convergence criteria. Validate first with crack-control engagement off.

Compare more than the stopping increment:

- `theta`/load factor and reaction history;
- `deffmax`, process-zone and failed UC6 integration-point counts;
- cumulative deleted elements and exact deletion batches;
- residual, iterations, cutbacks and accepted line-search scales;
- connectivity or physical separation of the specimen.

A fix passes the second wall only if it leaves the old-wall regression intact,
accepts sustained converged increments beyond the former state and produces
physical fracture progress. A complete success additionally reaches normal
step termination or demonstrates loss of load-carrying connectivity. If a new
wall or a resource limit stops the run, call the result partial.

## Compute budget

Before launching long runs, print the chosen budget in the log. A sensible
default is at most four full `s3rad` runs for this investigation: one baseline
reproduction if existing artifacts are insufficient, one combined diagnostic
run, and one final A/B pair. Suggested wall-clock limits are two hours for a
diagnostic run and six hours for the final fixed arm. These are defaults, not
a ban: an additional long run is allowed when it tests a new falsifiable
hypothesis, but state why it is necessary first.

Small unit/benchmark runs are not counted in that budget. Do not impose an
arbitrary accepted-increment cap and then describe reaching it as completion.

## Final delivery

Keep only the proven fix, necessary regression tests and concise diagnostics.
Commit the result, update `test/s3rad/README.md`, and provide either the pushed
branch or a verified bundle. The report must say explicitly whether the sample
fully separated.
