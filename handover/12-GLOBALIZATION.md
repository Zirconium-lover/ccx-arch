# 12. Globalization: six mechanisms, one interface, and a census first

Next after Topology in the migration order. Written before the code.

## 1. Where the responsibility lives today

`05-DEBT.md` section 3 names six mechanisms and the evidence that they do
not all discriminate: at one wall, **three consecutive attempts produced
bit-identical residual sequences** — two rescue levels ran and changed
nothing.

Measured in `nonlingeo.c` today:

| mechanism | references | state |
|---|---|---|
| path following (`pf_*`) | 357 | inline |
| transactional backtracking (`damage_bt_*`) | 67 | inline |
| rescue levels 1 and 2 (`damage_rescue_*`, `ccx_rescue_*`) | 59 | inline |
| dogleg / trust region | 43 | inline |
| adaptive line-search ladder | 10 | already an object, `lsladder.c`, 232 lines |
| event step | — | entangled with rescue level 2 |

**Sixteen environment switches** configure them (`docs/SWITCHES.md`):
`CCX_DAMAGE_LINESEARCH`, `CCX_DAMAGE_BT_{GROWTH,WINDOW,FLOOR}`,
`CCX_DAMAGE_RESCUE_{CORRIDOR,WINDOW,MAXUNREC}`, `CCX_PATHFOLLOW` and eight
`CCX_PATHFOLLOW_*`.

Six strategies, stacked in a fixed order, with no interface and no name for
the thing they all are.

## 2. Survey

**PETSc `SNESLineSearch` and `SNESNEWTONTR`** — `include/petscsnes.h`,
main, fetched 2026-09-13:
<https://raw.githubusercontent.com/petsc/petsc/main/include/petscsnes.h>

Three separate ideas, and this tree has none of them:

1. **`SNESLineSearchType`** (petscsnes.h:755-761) — seven named strategies:
   `none` (damping), `bt`, `secant`, `cp`, `nleqerr`, `bisection`, `shell`.
   Strategy is a *value*, selectable, not a position in a stack.

2. **`SNESNewtonTRFallbackType`** (petscsnes.h:165-169) — `NEWTON`,
   `CAUCHY`, `DOGLEG`: what to do when the trust-region subproblem's
   solution leaves the radius. This tree has dogleg hard-coded as "level
   3", which is one of the three with the choice removed and the name
   lost.

3. **`SNESLineSearchReason`** (petscsnes.h:918-924) — seven reasons a line
   search failed: `SUCCEEDED`, `FAILED_NANORINF`, `FAILED_REDUCT`,
   `FAILED_FUNCTION_DOMAIN`, `FAILED_OBJECTIVE_DOMAIN`,
   `FAILED_JACOBIAN_DOMAIN`, `FAILED_USER`.

**Taken**: the separation of *type* from *reason*. The debt entry — two
rescue levels ran and produced bit-identical residuals — is precisely the
observation `SNESLineSearchReason` makes routine: a mechanism that ran and
did not reduce anything reports `FAILED_REDUCT` rather than being
indistinguishable from one that was never reached. This is the same
separation Convergence step C needed for `rc=201`, and for the same reason.

**Rejected, for now**: the `SetType`/registration machinery. Six mechanisms
that are *stacked* rather than *chosen* cannot become selectable types
until it is known which of them do anything — and that is not known.

## 3. Step A is a census, not an extraction

The brief's test for keeping a mechanism is: *name the failure it addresses
and the gate case that would go red without it*. Nobody can do that for
these six, because nobody has the data. So the first step here is **not**
an extraction: it is an instrument.

For every attempt at every wall, record:

- which mechanism fired;
- what the residual was before and after it;
- whether it changed the iterate at all — the bit-identical-residuals
  observation generalized into a counter rather than an anecdote;
- and if not, why not, on the `SNESLineSearchReason` pattern.

Then a mechanism that never changes an iterate on the gate or on the target
deck is a mechanism with no defender, and deleting it becomes an argument
from data rather than from taste.

This ordering is the brief's own: *optimising algorithms starts with a
measurement*, and this is the same rule applied to deletion. It is also the
cheapest possible first step — counters and a report change no arithmetic,
so bit-identity is the standard and the instrument can be trusted before
anything is removed.

## 4. What is NOT in this object

- **The convergence verdict.** Globalization decides what to do when the
  verdict says no; it does not decide the verdict. `converge.c` owns that
  and the boundary is already clean.
- **Increment control.** Cutback and `dtheta` belong to Increment, which
  `checkconvergence.c` still owns.
- **Deleting anything.** Step A measures. What the measurement justifies is
  step B, with its own hypothesis, as a separate change.

## 5. Step A result: the anecdote is now a rate

Built, armed on every run, and aggregated over all sixteen gate cases:

```
14851 attempts, 4780 with no mechanism at all, 12 reproduced the previous
                                               sequence exactly

mechanism                  firings  attempts  changed  inert  unknown
line-search ladder             143        40       40      0        0
transactional backtrack         48         6        6      0        0
rescue level 1                   9         9        9      0        0
rescue level 2                  15        15        3     12        0
trust region (dogleg)          132         6        6      0        0
path following               20004      9996     9996      0        0
```

**Rescue level 2 is inert in 12 of the 15 attempts in which it acts.** The
debt entry recorded one wall where three consecutive attempts produced
bit-identical residual sequences; it is 80% of this mechanism's firings
across the whole gate.

### What this does and does not establish

It establishes **inertness**, which is the necessary condition for
deleting a mechanism: on twelve of fifteen occasions rescue level 2 acted
and the entire iteration sequence that followed was bit-identical to the
one before it. Nothing it did reached the solve.

It does **not** establish that the other five earn their keep. "Changed the
correction sequence" is a weak positive, and for path following it is close
to tautological: it modifies `b` directly, so the hash necessarily differs.
A mechanism can change the sequence and still not help. The brief's test has
two halves — *name the failure it addresses* and *the gate case that would go
red without it* — and this instrument answers neither. It answers a third
question, "did it do anything at all", and that is the one that makes a
deletion arguable rather than a matter of taste.

Two units in one table, deliberately: `firings` counts calls, everything
after it counts attempts. A mechanism that loops trials inside one attempt
shows many firings and few attempts, which is not a contradiction. The self
test pins that.

### The reporting gap this found, immediately

The first version printed only at the end of `nonlingeo()`. The gate showed
at once why that is wrong: the four cases that hit a wall exit through
`checkconvergence`'s `FORTRAN(stop)` and produced **no census at all** — the
runs the instrument exists for were exactly the runs it did not cover.

This tree had already learned it once. `CCX_LOG_VIEW`'s defender in
`docs/SWITCHES.md` is two 2.3-hour runs killed part way through that
produced no profile, because the table was printed from `atexit`. The
remedy here is `atexit`, and the difference matters: `FORTRAN(stop)` is an
*orderly* exit, so `atexit` runs. A run killed with a signal still loses the
census; if that becomes a problem the answer is LOG_VIEW's — interim
reports — and not another exit hook.
