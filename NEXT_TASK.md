# Next task: pass the second `s3rad` wall, which is now diagnosed

## Objective

The second wall has been measured AT the second wall. It is not a
linearisation problem and it is not an event-ordering problem: it is a
CONDITIONING problem localised on a single nearly-detached node. The
remaining task is to act on that diagnosis and drive `s3rad` to normal step
termination or to loss of load-carrying connectivity.

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
- **99.96% of that step sits on the three degrees of freedom of ONE node.**
  Node 1244 is held by one live bulk element and six cohesive facets that are
  still in the mesh with zero stiffness. Its assembled diagonal has collapsed
  and it is one of the 76 AUTOSPC counts. Its own residual is 12% of the
  peak; the operator inverts that small residual into a displacement of 1.08.
  The other 29405 degrees of freedom carry 0.04% of the step between them,
  and the next node down is 58x smaller.
- The direction is legitimate - the transpose identity
  `dot(J^T R,p_N)/|R|^2` holds to twelve digits, so it descends `0.5|R|^2`
  with directional derivative exactly `-|R|^2`. The descent is simply spent
  moving a node that is barely attached, while the force imbalance, which is
  at healthy nodes 1245 (7 live bulk) and 5282 (3 live bulk), hardly moves.
- **The best residual reduction available along the exact Newton direction,
  at ANY step length, is 1.03%**, at `eps=0.031`. Below that the model is
  exact and buys `eps`; above it the residual grows, to 32x at `eps=1`.
- 6069 integration points change branch along the step - 4511 UC6
  loading/unloading, 1554 bulk plastic - but that census is referenced to the
  FULL step (`transitions(eps=1)=0`, the count RISES as `eps` falls). Read
  correctly: no crossing below `eps=0.0078`, and 5480 of the 6069 in the last
  half of the step. They are a CONSEQUENCE of the order-one motion of node
  1244's neighbourhood, not an independent cause.
- The topology at the new wall is sound, re-measured and not carried over:
  one component, no floating piece, no orphan dof, no isolated equation, and
  the residual orthogonal to the near-null space to twenty-one digits while
  the correction carries at most 1.6e-04 of itself there. Node 1244 is a SOFT
  mode, not a null mode - it still has one element - which is why the
  near-null probes do not see it.
- What HAS changed is the support: 76 nodes have lost 99.9% of their
  assembled diagonal, against 3 at increment 136 and none at all at increment 91.
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

## Two remedies that are REFUTED before you spend anything on them

**Event truncation.** "6069 crossings, so stop the step at the first one and
re-assemble" is the obvious reading and the eps ladder already rejects it.
The first crossing is just under `eps=0.0625`; truncating there buys 0.64%,
the best rung of any length buys 1.03%, and the ladder's accepted
`alpha ~ 0.004` already gets about 0.4%. A factor of two or three on a
residual that must fall by orders of magnitude is not a fix, and
`checkconvergence`'s `iest > ic` test ends the increment long before 700
iterations at 1% each. Do not build an event-aware corrector for this wall.

**The Newton-Krylov corrector.** Already built, measured, refuted, reverted;
see above and the A/B table in `test/s3rad/README.md`.

**AUTOSPC as it stands** is not a remedy either, for a documented reason: it
excludes a collapsed-diagonal node from the DISPLACEMENT convergence norm
`cam[0]` and deliberately touches neither the solve nor the force residual.
That is correct for what it was built for. This wall is the same pathology
one level down - the collapsed node poisons the STEP, and the run dies on
`ram[0]`.

## The first experiment

**Hypothesis.** The step is unusable because it is spent on nodes whose
assembled diagonal has collapsed. If those degrees of freedom are removed
from it, what remains reduces the residual by substantially more than 1.03%.

**The one measurement that can reject it.** At the wall, form `p_N`, zero its
components on the nodes AUTOSPC's census already masks, and walk the SAME eps
ladder. If the best reduction is still about 1%, the masked nodes are not
what is wasting the step and the hypothesis is dead - and since the event
reading is already rejected, the next place to look is the
`|p_N|2/|R|2 = 3.93` amplification itself.

This is one armed increment on the existing `CCX_DAMAGE_TR_LINCHECK`
machinery, which already evaluates an arbitrary direction on `pass 2`. It is
not a run. **Measure before building.**

If it survives, the question becomes what to DO with those degrees of
freedom, and that choice is physical, not numerical. A node held by one
tetrahedron and six dead facets is very nearly detached; the honest options
are to constrain it explicitly and say so in the output - never silently -
or to let the deletion rule take its last element and orphan it properly,
where `topodiag` and the existing orphan handling will see it. A damping
threshold on the step is not one of the options.

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
