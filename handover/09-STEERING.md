# 9. Fix the steering: convergence by a mechanism anyone can name

## The goal, in the owner's words

> Исправить руль надо, чтоб сходимость была по понятному механизму и мы могли
> дорабатывать не решатель, а начальные условия со 100% пониманием как
> работают ручки программы.

Unpacked, this is an **end state**, and it is the point of everything below:

**When a run misbehaves, the answer must be "the model is wrong, change the
deck" — not "go debug the Newton loop."**

Today that question costs weeks. Every wall in this project's history was
first a fight about whether the deck or the solver was at fault. The measure
of success is that the question costs minutes, and that a person tuning
boundary conditions never has to open `nonlingeo.c`.

Three things stand between here and there, and they are in a strict order.

## Why the tangent comes first

Not because it is a defect. Because of what was built on top of it.

Six globalization mechanisms were added to this code, one per wall: the
adaptive line-search ladder, transactional backtracking, Rescue levels 1 and
2, a dogleg trust region, and path following. Each was added when Newton
refused to converge.

**Newton refused because the direction it computed was not a Newton
direction.** Measured, not supposed: in the process zone the assembled tangent
disagrees with the differential of the residual by up to ~10% of column
scale, with individual coefficients of the wrong sign, and the one-sided
difference test proves this is a *wrong derivative of a smooth function*, not
a kink.

So an unknown fraction of those six mechanisms is **scar tissue around a
defect nobody knew was there.** Until the direction is right, nobody can say
which mechanism is load-bearing and which is compensating — and "100%
understanding of the knobs" is unreachable while half of them exist to patch
something invisible.

## The root cause, already located. Do not re-derive it.

The term is in `mafilldamas.f`, verbatim from its own header:

```
C = g(D)*C_ep - sigma_eff (x) dD/d(eps)
```

That rank-1 correction **is** `∂σ/∂D · ∂D/∂ε` — the term that makes a damage
tangent consistent. Three measured facts about it:

1. **It exists only under `CCX_DAMAGE_TANGENT=UNSYM`.** The stock path and
   `FD_SYM` carry no such term at all. Hence 151 / 157 / 161 similarly wrong
   coefficients across the three modes: two of them have no damage term by
   construction.
2. **Even in UNSYM it is skipped for about half the elements that need it.**
   The tree prints this itself:
   ```
   [DAMAGE TANGENT HOLE] 11 element(s) with ADVANCING damage carry no rank-1
   term (new maximum); 11 past initiation without one, 10 assembled
   ```
   Eleven without against ten with, in the mode whose only job is to supply
   it.
3. **The signature matches.** The discrepancy tracks `dD/dε`, not `D`. Where
   damage is not advancing the term is legitimately zero and the tangent is
   correct — which is why the elastic control (5.4e-08), the closed cohesive
   facet (3.2e-14 with a textbook `h²` slope) and a barely-moving element are
   all clean.

An objection that would have voided all of this was raised and eliminated
first: the probe differentiated around `v = vold + b` while the matrix is
assembled at `vold`, a one-step offset that looks exactly like a wrong
tangent. `CCX_STRUCT_FD_BASE=VOLD` gives 153 WRONG / 9.626e-02 against 157 /
9.573e-02 — the numbers **moved**, proving the switch changed the
differentiation point rather than doing nothing, and the conclusion survived.

## The work, in order

### 1. Which population is the defect

The counters in `mafilldamas` already separate `adv`, `gap`, `hi` and `skip`.
Cross-check them against the elements `opcheck` reports as WRONG.

**Done when** every WRONG element is accounted for by a named counter, and
each counter is classified as *defect* or *legitimate* with a reason. Some
skipping may be correct — a cut-off near `D→1` exists for a reason. Say which.

### 2. Close the hole

**Done when** `opcheck` reports **0 WRONG in the process zone** with the `h²`
slope holding, on at least two decks, and the elastic and cohesive checks stay
clean.

### 3. Decide what the default mode should be

If the rank-1 term is required for consistency, then the stock path and
`FD_SYM` are **structurally inconsistent by construction**, not by accident.
That is a statement about what the default ought to be, and it has never been
made.

**Done when** the default is either changed, with the measurement that
justifies it, or kept, with the measurement that justifies that.

### 4. Re-measure the walls

Every ladder reading taken in the process zone was taken along a direction
that was not the Newton direction. `02-DIAGNOSTICS` §1 now carries that
warning; the first two rows of its table cannot be read until this is fixed.

Re-read them. Some walls may simply not be there.

**Done when** the warning in §1 is removed, or kept with a measured reason.

### 5. Delete what turns out to be scar tissue

For each of the six globalization mechanisms: **name the failure it addresses
and the gate case that goes red without it.** Measured evidence that not all
of them can: at one wall, three consecutive attempts produced bit-identical
residual sequences — two mechanisms ran and did nothing.

**Done when** every survivor has a case that fails without it, and every
casualty is **deleted**, not disabled. A mechanism kept "just in case" is a
place for the next bug to hide.

### 6. Then, and only then, the knobs

"100% understanding of the knobs" is not 139 documented switches. It is a
number small enough that each one can justify itself.

**Done when** every knob a person can set has a declaration with a type, a
range, one sentence of prose and a test that exercises it — and the count is
small. Today: 139 read, 122 set by no test. Three of those orphans have
already turned out to be *working capabilities nobody could find*, so the
retirement pass is a reading pass first and a deletion pass second.

## Two traps, named in advance

**The tangent fix is the first change that cannot be bit-identical.** The
iteration path changes by construction. Acceptance shifts to: **the same
converged answer within a stated tolerance, and fewer iterations.** The
comparison tool built for exactly this exists now, one step earlier in the
queue — use it, and state the tolerance.

**Element deletion is a threshold decision.** A different iteration path can
delete a different element, after which the trajectories diverge
macroscopically — and that is not a defect, it is the known thread-count
sensitivity arriving by another route. At fast-deck scale the honest criterion
is **the same deletion set and the same severance `theta` within tolerance**.
At `s3rad` scale it may not be provable at all. **Say so before the run, not
after** — otherwise a legitimate divergence will cost a day of investigation.

## What success looks like

Not a green suite, and not a smaller `nonlingeo.c`.

**A person changes the deck, runs it, and either it converges or the failure
message names what in the model is wrong.** The solver stops being a thing
that has to be argued with. That is the whole point, and everything above is
in service of it.
