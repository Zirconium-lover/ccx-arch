# Next task: pass the second `s3rad` wall, which is now diagnosed

## Objective

The second wall has been measured AT the second wall and it is an EVENT
problem, not a linearisation problem. The remaining task is to act on that
diagnosis: make the corrector event-aware on the ordinary Newton path, and
drive `s3rad` to normal step termination or to loss of load-carrying
connectivity.

The engineering objective is a physically credible complete `s3rad`
fracture/separation, not merely a larger increment number. **The specimen has
never separated.** The best run to date stops at accepted increment 554,
`theta=0.255574`, with 6780 live bulk elements still carrying load.

Mixed-mode crack control and `src/lsladder.c` are established regressions.
Do not retune them unless a direct measurement invalidates one of their
assumptions.

## Facts already established - do not re-derive these

- The old wall at `theta~0.21218` is passed at the original
  `CCX_DAMAGE_DEADALL=1.e-2` by extending the backtracking ladder below 0.1
  and returning the best measured rung.
- The new wall is reproduced exactly: increment 554 committed,
  `theta=0.255574`, `rc=201`, 6780 live bulk elements, 415 deletion batches,
  3734 elements deleted, 566 line-search activations, 7439 s on 2 threads.
- **The tangent at the wall is CONSISTENT.** The linear-model defect ratio
  falls like `O(eps)` over an eps ladder - 0.712, 0.370, 0.186, 0.092, 0.045,
  0.024, 0.017 - with a floor eleven orders above cancellation. The wall is
  NOT a wrong Jacobian.
- **The step is wrong.** `|p_N|inf = 1.0806`, an order-one displacement on a
  specimen whose grip has moved 0.2556. The full step multiplies the residual
  by 32.
- **6069 integration points change branch along that step** - 4511 UC6
  loading/unloading, 1554 bulk plastic - and every one of them between
  `eps=0.03` and `eps=1`. The linear model is therefore good over about 3% of
  the step, which is exactly the `alpha ~ 0.004` the search finds.
- The topology at the new wall is sound, re-measured and not carried over:
  one component, no floating piece, no orphan dof, no isolated equation, and
  the residual orthogonal to the near-null space to twenty-one digits while
  the correction carries at most 1.6e-05 of itself there.
- What HAS changed is the support: 76 nodes have lost 99.9% of their
  assembled diagonal (3 at increment 140), and the residual peaks at node
  1244, held by ONE live bulk element and six fully failed cohesive facets.
- The deletion loop is refuted at the new wall too, re-measured with
  `CCX_DAMAGE_BATCH_TRACE=1`: the last committed batch is 415 at increment
  551, so increments 552-555 commit no deletion at all.
- **A Newton-Krylov corrector was built, measured, and REFUTED.** It repairs
  the separate increment-92 tangent defect but stops the run at
  `theta=0.191828` against `0.255574` with it off. Its functional patch is
  reverted. Do not rebuild it; the A/B table is in `test/s3rad/README.md`.

## The one open question that is NOT the wall

A separate, real first-order tangent defect exists around increment 92, where
the run first cuts back. There the defect ratio is CONSTANT (0.359, 0.870)
instead of `O(eps)`, the Newton iteration's linear convergence rate equals it
to six decimals every iteration, and its asymptotic size tracks the viscous
factor `beta = dtime/(eta + dtime)`. It is NOT the damage-consistent rank-1
term: that term's `dD/d(eps)` is present at max 36.4 and mean 4.18 over 6850
elements, and its action on the Newton step is 0.27% of the operator's - two
to three orders below the defect. **The missing derivative is still unnamed.**

It costs iterations and cutbacks all the way up, and it is NOT what stops the
run: by `theta=0.2556` the viscosity has damped it to 1.7%. Naming it is
worth doing on its own merits; it is not the path to the wall.

`CCX_DAMAGE_WALL_THETA` prints a census of `damjac`'s two halves at the armed
increment, which separates "the derivative is tiny" from "the assembly loses
it". That census is already taken and says the derivative is present, so the
next place to look is between `damageq` in `resultsmech.f` and what
`mafilldamas.f` actually assembles from it.

## The first experiment

**Hypothesis.** The wall is the line search damping a step that is valid over
3% of its length. If the step is truncated at the FIRST crossing of the
loading/unloading set instead of scaled by a scalar `alpha`, the residual
falls by the full `alpha` of the truncated step rather than by a fraction of
it.

**The one measurement that can reject it.** Arm `CCX_DAMAGE_WALL_THETA` at the
wall and compare, on the same restored base state, the residual reduction of
the truncated step against the reduction the ladder achieves at its accepted
rung. If truncation buys no more than the ladder does, the hypothesis is
dead and the wall is not an event-ordering problem after all.

This is one armed increment, not a run. Both quantities are already printed
by the existing ladder instrumentation. **Measure before building.**

If it survives, the implementation is an event-aware corrector on the
ORDINARY Newton path: stop the step at the first branch crossing, re-assemble
on the new branch, continue. The machinery exists in the tree but only on the
same-load re-equilibration path (`[DAMAGE EVT]` in `nonlingeo.c`, which
locates the first UC6 tension/compression crossing and takes that event
step). What the wall needs is the same idea keyed on the loading/unloading
set - which is where the 4511 crossings are - and on the ordinary path, which
is where the wall is.

Do not disguise an event corrector as another damping threshold. If it needs
a threshold to work, it is not the fix.

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
