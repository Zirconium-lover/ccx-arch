# 8. The object model: what to build, and how to get there

This is the primary brief. `07-RESEARCH-AGENDA.md` is the survey material
that should inform it.

## 1. What is wrong, structurally

| | |
|---|---|
| `src/nonlingeo.c` | 16140 lines; the `nonlingeo()` body alone is **~14381** |
| what that one function owns | the increment loop, the Newton loop, assembly, the solve, the convergence judgement, six globalization mechanisms, erosion and topology bookkeeping, path following, and every diagnostic |
| `CCX_*` switches | **139**; 63 explained nowhere, 122 set by no test |
| judgements with an owner | 2 (`lsladder.c`, `damstate.c`) |
| judgements without one | what counts as eroded, as converged, as a load path, as a step, as a direction |

Three symptoms worth keeping in mind, because each is a decomposition
argument rather than a bug:

- **The same census is printed from three separate sites** in
  `nonlingeo.c`. Nobody owns it, so it was written three times.
- **A guard was placed inside the transaction it was meant to observe.** The
  connectivity check sat inside `if(damage_tent_count>0)` — the bulk-deletion
  transaction — so on an interface-dominated fracture it could never run,
  however it was configured. A responsibility with no home ends up nested
  inside whatever code happened to be nearby.
- **At one wall, three consecutive attempts produced bit-identical residual
  sequences.** Two globalization mechanisms ran and did nothing. Nobody can
  say what the six are for, because there is no interface that would force
  the question.

## 2. The idiom: object orientation in C, as PETSc does it

Do not invent this. PETSc is the reference implementation and it is C, not
C++:

- an **opaque handle** (`typedef struct _p_SNES *SNES`) so callers cannot
  reach inside;
- an **ops table** of function pointers, so behaviour is pluggable;
- `XXXCreate`, `XXXSetType`, `XXXSetFromOptions`, `XXXSetUp`, `XXXDestroy` —
  a uniform lifecycle;
- **registration** (`SNESRegister`) so a new implementation is added without
  touching the dispatcher;
- **options prefixes** so two instances of the same object are configured
  independently;
- a **reason enum** for every terminal state (`SNESConvergedReason`), so
  "why did it stop" is always answerable.

Study the real headers and implementations, not a description of them. Then
decide what this code needs of it — the full apparatus is probably too much;
the opaque handle, the ops table, the uniform lifecycle and the self test
are probably the right subset.

Add one thing PETSc does not have and this tree already proves the value of:
**every object carries a self test, and refuses to arm if it fails**
(`lsladder.c`, `damstate.c`). Keep that.

## 3. The objects

Proposed. Argue with it — a decomposition you have not argued with is one you
do not understand.

| object | owns the question | today it lives in |
|---|---|---|
| **Options** | what may be configured, its type, range, default and documentation | 139 `getenv` calls + `damswitch.c` (~10% of the job) |
| **Increment** | how big the step is, when to cut back, how many attempts | the increment loop, `tmin` override, cutback logic |
| **Newton** | the iteration loop and its termination | the Newton loop |
| **Direction** | how the correction is computed | Newton / modified / dogleg, inline |
| **Globalization** | how far along it to move | **six mechanisms**: ladder, backtracking, Rescue 1, Rescue 2, dogleg, path following |
| **Convergence** | what counts as converged | `checkconvergence.c` + AUTOSPC + `AUTOSPC_FORCE` + a `qam` floor + a slow-Newton extension |
| **Operator** | the tangent, its mode, and whether it matches the residual | `mafillsmmain` calls + tangent switches; **nothing verifies it** |
| **Topology** | what counts as eroded, and the transaction that commits it | `damage_tent_*`, batch commit, deletion history |
| **LoadPath** | is this still a specimen | being built in the parallel repository as `loadpath.c` — design so it drops in |
| **NodeState** | has this degree of freedom lost its load path | `damstate.c` — already an object, use it as the pattern |
| **Monitor** | producing a diagnostic vs presenting it | `printf` interleaved with the solve, gated by environment variables |

Two of these deserve emphasis:

**Convergence should be a composable tree**, not a function with special
cases bolted on. Trilinos NOX `StatusTest` is the model: `NormF`,
`NormUpdate`, `MaxIters`, `Stagnation`, `Divergence`, combined with AND/OR,
each able to say *why* it fired. AUTOSPC and `AUTOSPC_FORCE` stop being
exceptions inside the judgement and become tests within it — which also makes
the recorded failure of `AUTOSPC_FORCE` (it satisfied its own alarm condition
for having become a fiction) expressible instead of buried.

**Globalization is six mechanisms with no interface.** Once they are behind
one, the question "what is each for" becomes answerable, and the ones that do
nothing become deletable. Survey PETSc's `SNESLineSearch` types and
`SNESNEWTONTR` for the taxonomy.

## 4. Algorithms — but measure before optimising

Nobody has profiled this code. The target deck takes 2.3 hours and no one
knows where it goes. **Do that first**; everything below is a hypothesis
until then.

Candidates, each an architecture decision as much as a numerical one:

- **A fixed sparsity pattern under erosion.** Element death changes the
  matrix structure, so the symbolic factorisation is redone. If dead elements
  keep their entries — zeroed, with a safe diagonal — the pattern is constant
  for the whole run and the symbolic phase happens once. Potentially a large
  fraction of the runtime, and it is a *representation* decision belonging to
  the Topology object. `CCX_PARDISO_REUSE_SYMBOLIC` exists and is limited by
  exactly this.
- **The line-search ladder evaluates a full residual per rung.** Interpolating
  backtracking (quadratic/cubic, Dennis & Schnabel; Nocedal & Wright) usually
  needs one to three evaluations rather than a fixed ladder. Measure how many
  residual evaluations per increment the ladder consumes before deciding.
- **Tangent reuse.** Modified Newton where the tangent changes slowly, full
  Newton where it does not — a Direction/Operator policy, not an `if`.
- **The census computed three times** because it has no owner. One
  computation, several viewers.
- **The run continues past physical separation** — about a fifth of the
  target deck's runtime is spent on a specimen that has already broken. The
  parallel repository is fixing that; it is the rare case where correctness
  and speed are the same change.

## 5. Migration: strangle, do not rewrite

A big-bang rewrite of a 14000-line function will fail, and this tree already
knows the alternative because it has used it twice.

The loop, per object:

1. **Name the responsibility** and find every site that performs it. The
   census being printed from three places is what that search looks like when
   it succeeds.
2. **Define the interface** — the questions the object answers, not the data
   it holds.
3. **Extract into a new translation unit** with a self test that refuses to
   arm on failure.
4. **Leave a call at the original site.** The old code path becomes a view
   onto the new object, exactly as `damage_addiag` became a view onto
   `damstate`.
5. **Prove bit-identity.** Not "the same numbers" — the same bytes, on the
   gate and, for anything that touches the solve, on the target deck.
6. **Only then** change behaviour, as a separate step with its own
   hypothesis.

Suggested order, smallest blast radius first among the high-value ones:

**Options** (mechanical, touches everything, breaks nothing) → **Monitor**
(pure extraction, no behaviour) → **Convergence** (the first real interface)
→ **Topology** (the transaction, and the sparsity decision) →
**Globalization** (where the deletions will be) → **Direction/Newton** (last,
because everything else moves through it).

## 6. What "done" looks like

Not a green suite, and not a design document.

**Done is that a reader can find where a thing is decided.** Ask "what counts
as converged here" and arrive at one file with a self test, instead of five
places and an environment variable. Then a bold change becomes possible,
because it becomes reversible and checkable — which is the whole point, and
the thing this code has never had.
