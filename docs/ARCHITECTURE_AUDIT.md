# Architecture audit of the damage/fracture module

Written after three walls in a row were passed by point fixes, on the
observation that the walls keep coming back through different doors.  Every
number here is measured, not estimated.

## 1. Inventory

| measurement | value |
|---|---|
| `src/nonlingeo.c` | **16140 lines** (stock CalculiX 2.23: ~4000) |
| body of the single function `nonlingeo()` | **~14381 lines** |
| `CCX_*` environment switches across the tree | **140** |
| of those, mentioned in no `.md` at all | **95 (68%)** |
| modules carrying a self test | 5 (`pathfollow`, `crackcontrol`, `topodiag`, `lsladder`, +1) |
| lines covered by those self tests | 2159 |
| lines of `nonlingeo()` covered by any unit test | **0** |
| file-scope mutable `damage_*` declarations | 14 |
| static helpers inside `nonlingeo.c` | 35 |
| fastest full validation of a change | **~2.5 hours** (one `s3rad` run) |

The single most important line in that table is the second one.  A
14000-line function cannot be reasoned about locally, so every change is a
global change, and the only way to find out whether it broke something is a
2.5-hour run.

## 2. How this produces the walls we keep hitting

These are not four unrelated defects.  Three of the four trace to the same
two structural properties.

### 2.1 One judgement, four consumers, three partial implementations

The code decides, per node, "this degree of freedom no longer carries load".
That judgement is computed once and then told to consumers **piecemeal**:

| consumer | who tells it | switch |
|---|---|---|
| displacement norm `cam[0]` | `damage_spc_mask`, in `resultsini.c` | `CCX_DAMAGE_AUTOSPC` |
| force norm `ram[0]` | separate code in `nonlingeo.c` | `CCX_DAMAGE_AUTOSPC_FORCE` |
| termination connectivity | a third scan in `nonlingeo.c` | `CCX_FRACTURE_DEADFACET` |
| **the solve itself** | **nobody** | - |

The second wall (`theta=0.2555742`) was *precisely* the gap between rows one
and two: AUTOSPC knew node 1246 had lost its load path and told the
displacement norm, while the force norm - which is what actually vetoed the
increment - was never told.  Adding row two passed that wall and immediately
exposed row four.

**Every wall of this family is the same physical state surfacing through a
consumer that has not been wired up yet.**  Fixing them one at a time is
guaranteed to produce a new wall each time, and that is what happened.

### 2.2 Round-off is amplified into macroscopic divergence by discrete events

The model contains genuine discontinuous switches:

| switch | jump |
|---|---|
| damage initiation, `deff > d0` | onto the softening branch |
| loading/unloading, `deff` vs `dmax` | tangent changes discontinuously |
| crack-face closure, `deltal(1) = 0` | normal stiffness `g*Kn -> Kn`, **five orders** - *now removable, see below* |
| element deletion, `D > 1-DEADALL` | **the mesh topology changes** |

Parallel assembly and PARDISO sum in a thread-count-dependent order, and
floating-point addition is not associative, so the last bits differ.
`MKL_CBWR=COMPATIBLE`, which this project sets, fixes reproducibility across
instruction sets - **not across thread counts**, a distinction that was
being relied on silently.

A 1e-16 difference then decides which side of a threshold an integration
point falls on, an element dies one increment earlier, and the mesh itself
differs.  Measured here: two runs identical for 482 attempts diverged at
attempt 483 with the *same* `theta` and the *same* `dtime`, differing only by
6 Newton iterations against 5 - a borderline convergence decision tipped by
round-off.  Twenty increments later they were on different walls.

**One of the four has since been measured to the ground and regularised.**
The crack-face closure switch leaves the traction continuous but its slope
jumps by `1/gmin`.  On the fast wrapped deck at its wall, seven UC6 points sit
exactly on that kink and change category at *every* line-search rung down to
`eps=6.1e-5`, while the residual grows strictly linearly in `eps` and never
falls below its base value - Newton cannot converge on a kink it cannot step
off.  `CCX_UC6_CONTACT_SMOOTH` blends the two slopes over a penetration band;
the transitions go 19 -> 0, the base residual goes `3.48e-02 -> 1.89e-10`, and
the deck runs from `theta=0.158766` to `theta=1`.  The same 64 elements are
deleted, in the same order.  That is the shape the remaining three should be
taken in: measure the kink, regularise it, and prove the answer did not move.

Part of this is irreducible: quasi-brittle fracture with element erosion IS a
sequence of topology jumps.  Commercial codes meet the same problem, which is
why Abaqus ships `*STATIC, STABILIZE`.  But the amplification is made worse
by having no damping mechanism of that kind here, and by the convergence
criteria being pointwise (`max|R|`) rather than energy- or norm-based, so a
single degree of freedom can veto an otherwise converged increment.

### 2.3 Combinatorial, untested switch space

140 switches, 68% undocumented, none covered by a test that runs in less than
hours.  There is no record of which combinations have ever been run together.
A concrete cost, paid during this very session: three flags were changed at
once for one run, it diverged, and the cause could not be attributed without
launching two further 2.5-hour runs to separate the variables.

## 3. What the architecture should be

The tree already contains the right pattern, four times over: `lsladder.c`,
`pathfollow.c`, `crackcontrol.c` and `topodiag.c` are small, single-purpose,
independently testable, and each **refuses to arm when its own self test
fails**.  That pattern works.  It simply was never applied to the other
12000 lines.

Proposed decomposition, each unit with a self test and an explicit contract:

| unit | owns | replaces |
|---|---|---|
| `damstate` | THE predicate: is this dof/element/facet load-carrying? Computed once per increment, consumed by all four consumers. | `damage_spc_mask` + AUTOSPC_FORCE + DEADFACET |
| `damerode` | deletion and erosion policy, and the node cleanup that must follow | scattered DE1.3 code |
| `damstab` | stabilization: viscous damping in the Abaqus sense, with a reported dissipated-energy fraction | nothing exists today |
| `damconv` | convergence criteria and the slow-Newton controller | inline `ram/cam/qam` logic |
| `damsolve` | Newton orchestration: line search, trust region, rescue ladder | inline, ~4 mechanisms deep |
| `damdiag` | every probe, arm-able, never on a solution path | WALLDIAG, TOPODIAG, BATCHTRACE |

The contract that makes this safe: **a unit may read solver state; only
`damsolve` and `damerode` may change it.**  Diagnostics may change nothing.
Today that boundary is not drawn anywhere, which is why a read-only
diagnostic switch could plausibly be suspected of changing a trajectory.

## 4. Invariants that must become tests

None of these is enforced today; each corresponds to a defect already paid
for.

1. **Feature-off equivalence.**  With every `CCX_*` unset, the binary
   reproduces stock CalculiX bit for bit on a reference deck.  Today this is
   checked by hand, occasionally, on a 2.5-hour run.
2. **Determinism at fixed thread count.**  Same binary, same flags, same
   threads, twice, bit-identical.  Never verified.
3. **Thread-count sensitivity is measured, not assumed.**  Now measured, and
   the answer is bad: **the same binary with the same flags on 4 threads
   instead of 2 diverges from the 2-thread run at attempt 483**, increment
   269, taking 6 Newton iterations where the reference took 5 at the *same*
   `theta` and the *same* `dtime`.  Twenty increments later the two runs are
   on different walls - one reaches `theta=0.5575`, the other stops at
   `theta=0.255032`.

   The discrimination was done properly, because the first attribution was
   wrong.  A run with three flags changed at once diverged and
   `CCX_FRACTURE_DEADFACET` was blamed; its code is read-only, so two further
   runs were launched to separate the variables:

   | run | threads | flags | result |
   |---|---|---|---|
   | reference | 2 | AUTOSPC_FORCE | - |
   | `THREADTEST` | **4** | AUTOSPC_FORCE | **diverges at attempt 483** |
   | `ANIM2` | 2 | AUTOSPC_FORCE + VTK series | identical |

   The thread count reproduces the divergence exactly - same attempt, same
   increment, same 6-against-5 iterations - and the read-only switch is
   innocent.  `MKL_CBWR=COMPATIBLE`, which this project sets and relied on,
   fixes reproducibility across instruction sets, **not across thread
   counts**.

   Consequence for every comparison in this repository: **runs at different
   thread counts are not comparable**, and any A/B must fix the thread count
   as carefully as it fixes the deck.
4. **Every switch is documented, defaults off, and has one test.**  95 fail
   the first clause today.
5. **The load-path predicate is consistent.**  A node excluded from one
   consumer is excluded from all of them, or the inconsistency is deliberate
   and stated.

## 5. Features worth importing, and what each would buy

| feature | where it is standard | which wall it addresses |
|---|---|---|
| viscous stabilization with an energy-fraction report (`*STATIC, STABILIZE`) | Abaqus | the local snap-through that makes `dtheta` collapse; damps the discrete-event amplification of 2.2 |
| energy- or norm-based convergence alongside the pointwise one | most implicit codes | a single dead dof vetoing an otherwise converged increment - the second wall exactly |
| proper element erosion with orphan-node cleanup | LS-DYNA, Abaqus VUMAT erosion | the fragment deadlock; today a node keeps its dofs after its last element dies |
| contact regularization over a small interval | any contact code | the five-order tangent jump at `deltal(1)=0` |
| arc-length / dissipation-controlled continuation | Riks, and `pathfollow.c` here | genuine limit points; already built, never engaged on `s3rad` at `DEADALL=1e-2` |

## 6. How to get there without breaking what works

The blocker is not knowing what to do - it is that **validation costs 2.5
hours**, so no refactor can be checked cheaply.  Everything else follows from
fixing that.

1. **Build a fast regression deck.**  A specimen small enough to run in
   minutes that still reproduces the wall classes: a collapsed-support
   fragment, a limit point, a deletion cascade.  Without this, nothing below
   is affordable.
2. **Freeze behaviour with characterization tests** on that deck, at the
   current head, before touching anything.
3. **Extract one unit at a time**, strangler-style, each landing with its own
   self test and a feature-off equivalence check.  Start with `damstate`,
   because it is the one that keeps producing walls.
4. **Only then** add `damstab` and the rest of section 5.
5. **Retire switches** as their behaviour becomes either default or deleted.
   140 is not a feature set, it is an untested configuration space.

## 7. Honest note on how this was reached

Three of the fixes in this repository, including the one this session added,
are point fixes: each identified a real mechanism, measured it, and repaired
it where it surfaced.  Each was also a new switch.  That is how the count got
to 140, and this session contributed to it rather than reversing it.  The
audit exists because the fourth wall made the pattern impossible to miss.

---

## Status of the plan, updated as steps land

| step | state |
|---|---|
| a validation that runs in minutes | **done** - `test/regress/run.py`, 6 cases, ~3 min, proven able to go red |
| one owner for the load-path judgement | **done** - `src/damstate.c`, verified bit-identical on a deck where the predicate actually decides, and over 849 attempts / 3135 deletions of `s3rad` |
| the crack-face closure discontinuity | **done** - `CCX_UC6_CONTACT_SMOOTH`, measured to remove the wall without moving the fracture |
| every switch documented and reported | **partly** - `tools/mkswitches.py` generates `docs/SWITCHES.md` and the registry `src/damswitch.c` reports every run's configuration and names anything set that the binary does not read; 94 of the 139 still have no prose anywhere |
| the other three discontinuities | open |
| viscous stabilization | open |
| retire switches | open |

### A hypothesis this raised and measurement rejected

The load-path judgement is a **per-node diagonal** test, so it is blind by
construction to a piece of the model that is internally stiff but attached to
nothing: every node in it has healthy elements of its own, and only the
assembled system is singular, in that piece's six rigid-body modes.  That is
the class Abaqus's `*STATIC, STABILIZE` exists for, and the obvious next unit
looked like a connected-component check.

Measured on the reference run instead of assumed (`test/s3rad/fragments.py`,
state at increment 800, `theta=0.4064`, against the FINAL deletion set, which
makes the test conservative):

| a facet counts as a load path only above `g` | components | adrift |
|---|---|---|
| 0 (any surviving facet) | 1 | 0 |
| 1e-3 | 1 | 0 |
| 0.1 | 1 | 0 |
| **0.5** | **2** | **0** |

Of 5316 facets carrying state, 1904 are already at the residual floor and
2099 still above `g=0.5`.  **Nothing is adrift at any threshold.**  So the
free-fragment class does not occur here, and building a unit for it would
have been building for a failure this model does not have.

The same table says something else worth keeping: counting only facets that
retain more than half their stiffness, the model is **in two pieces, one at
each grip**.  That is the severance result again - previously measured from
the reaction force falling 2676.81 to 1.51 - arrived at independently, by
connectivity, from a different file.

### Housekeeping noticed while doing the above

`src/ccx_2.22` is a 6.5 MB ELF executable **tracked in git**, inherited from
the `Import: CalculiX 2.22 original sources` commit and present at this
branch's base.  The working brief says binaries stay out of Git.  Left alone
because removing a file from the vendored import is the repository owner's
call, not a side effect of a fracture investigation.

