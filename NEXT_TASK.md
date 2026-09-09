# Next task: drive `s3rad` to separation, now that the second wall is passed

## Status

**The second wall is passed.**  `CCX_DAMAGE_AUTOSPC_FORCE=1`, one flag, deck
and `DEADALL=1.e-2` and viscosity and tangent and AUTOSPC threshold and
convergence criteria and PARDISO all unchanged:

| | stock | fix |
|---|---|---|
| last committed increment | 554 | **604 and continuing** |
| grip displacement `theta` | 0.2555742 then death | **0.2635** |
| deletion batches | 415, last at inc 551 | **434** |
| `dtime` | 1.95e-06 then death | **2.00e-03, the deck maximum** |

`dtime` has recovered by a factor of 1000 and sits at `dtmax`; the run is
healthy, not limping.  The pre-wall history is bit-identical to stock over
all 1147 attempts.

## What the wall was - do not re-derive this

The peak force residual sat on **node 1246, a fragment held by ONE live bulk
element with all six of its cohesive facets failed and OPEN** (`ncomp=0`
through increments 553-555, so not the compression switch).  Equilibrating it
needs a displacement of order one against a grip that has moved 0.2556, so
its residual fell 0.1% per Newton iteration and was irreducible in practice.

That component was inherited by every following increment and accumulated -
15.7, 50.4, 74.9, 94.5, 105.7% of the convergence tolerance over increments
551 to 555 - **while `dtheta` collapsed 48x**.  Cutting the step 48x made the
converged equilibrium 6x worse, because the part that will not reduce is not
proportional to the step.  It was also a deadlock: the increment that could
not converge was the one in which that fragment's last element would damage
and delete.

Four remedies were tried and are refuted, each for the same reason - they
repair the step, and the step was never the problem: the Newton-Krylov
corrector (stops EARLIER, at `theta=0.191828`), event truncation (rejected
from the eps ladder before being built), collapsed-node projection of the
step (6.7x on `|R|2`, nothing on `|R|inf`, which is what is judged), and a
smaller increment.

## Two things to watch in the fix

* Acceptance at the wall came at iteration 10 under stock CalculiX's relaxed
  late-iteration tolerance `rap=0.02`, not the strict `ran=0.005`.  At the
  0.6% per iteration the remaining residual was contracting, strict would
  have arrived near iteration 14, which is what `iest=14` said.  Watch that
  this stays true and the fix is not quietly living on `rap`.
* The excluded residual peaked at `0.0086 x qam`, 1.7x the tolerance, on one
  node for eight increments, then returned to machine zero once the fragment
  resolved.  Every excluded residual is printed next to the criterion it was
  excluded from.  **If that number stops returning to zero, the fix has
  become a fiction and must be revisited.**

## The task

Drive the run to normal step termination at `theta=1.0` or to loss of
load-carrying connectivity, and report which.  The specimen has NOT
separated: at `theta=0.2635` it still carries load on roughly 6760 live bulk
elements.

Expect further walls.  Before treating one as new, check whether it is the
same fragment mechanism at a different node - the diagnosis above is cheap to
re-test, because the fix already prints the excluded residual and
`[DAMAGE STIFFNESS]` already counts collapsed nodes.

If a new wall is NOT that mechanism, measure before building.  The tools now
in the tree: `CCX_DAMAGE_WALL_THETA` (linearisation ladder, active-set
census, residual and correction localisation, per-node stiffness at the
residual peak), `CCX_DAMAGE_WALL_MASKSTEP` (the step with collapsed nodes
projected out), `CCX_DAMAGE_BATCH_TRACE`, `CCX_DAMAGE_TMIN`, and
`topodiag`.

## Validation and success

The causal A/B must use the same deck, PARDISO binary family and environment;
the only functional difference is the proposed new fix. Keep
`CCX_DAMAGE_DEADALL=1.e-2`, the existing viscosity, tangent, AUTOSPC and
stock convergence criteria. Validate first with crack-control engagement off.

Compare more than the stopping increment:

- `theta`/load factor and reaction history;
- `deffmax`, process-zone and failed UC6 integration-point counts;
- cumulative deleted elements and exact deletion batches;
- residual, iterations, cutbacks and accepted line-search scales;
- connectivity or physical separation of the specimen.

A fix passes the second wall only if it leaves the old-wall regression
intact, accepts sustained converged increments beyond `theta=0.255574` and
produces physical fracture progress. A complete success additionally reaches
normal step termination or demonstrates loss of load-carrying connectivity.
If a new wall or a resource limit stops the run, call the result partial.

**The Newton-Krylov arm is the cautionary case: it was ahead in `theta` at
every increment from 91 to 141 and still lost the run.** Being ahead early is
not evidence. Only the stopping state is.

## Compute budget

Before launching long runs, print the chosen budget in the log. A sensible
default is at most four full `s3rad` runs for this investigation: one
baseline reproduction if existing artifacts are insufficient, one combined
diagnostic run, and one final A/B pair. Suggested wall-clock limits are two
hours for a diagnostic run and six hours for the final fixed arm. These are
defaults, not a ban: an additional long run is allowed when it tests a new
falsifiable hypothesis, but state why it is necessary first.

A full run to the wall costs about 7400 s on 2 threads on this container, so
a full A/B pair is roughly four hours of wall clock. Budget for it.

Small unit/benchmark runs are not counted in that budget. Do not impose an
arbitrary accepted-increment cap and then describe reaching it as completion.

## Final delivery

Keep only the proven fix, necessary regression tests and concise diagnostics.
Commit the result, update `test/s3rad/README.md`, and provide either the
pushed branch or a verified bundle. The report must say explicitly whether
the sample fully separated.
