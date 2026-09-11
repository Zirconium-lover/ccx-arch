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

---

# Answered, 2026-09-11

This section is the reply to the brief above, not a revision of it.  Every
number is from `research/06-TANGENT-VERDICT.md`, which carries the tables.

## §1 - which population is the defect: **answered, and the answer is none**

Every *defect* population is empty.  The element that measures wrong is the
one that HAS the rank-1 term.  The discrepancy is present **before any
damage** - identical to four significant figures with the damage cards
stripped - and about ten times larger after initiation
(`research/05-TANGENT-POPULATIONS.md`).

## §2 - the root cause "already located, do not re-derive it": **refuted**

The brief names the rank-1 term in `mafilldamas.f`.  That term is now
extracted into `src/damrank1.f`, and it is **correct**: it agrees with
`e_c3d`'s own quadruple sum to `4.9e-16` of the block's largest entry, and
with a finite difference of the element internal force to `1.1e-10`.  The
self test breaks it deliberately, in the way that was suspected, and shows
the check going red.  The extraction is bit-identical on every gate case.

The real defects were one file over, in `resultsmech.f`, in the **symmetric**
path: `dD/d(eps)` in tensor-shear convention against an engineering-shear
tangent (2x on shear entries, 4x on shear-shear), and no viscous `beta` at
all.  Corrected, that path stopped destroying runs - and then measured
**worse than applying no correction at all**, so it was deleted.

## §3 - the default mode: **decided, and it is "off"**

`fast-plain`: no correction 23.11 s / 1286 iterations.  `UNSYM` 31.58 s
(+37%) / 1283 (-0.2%) / **byte-identical fracture**.  Deleted `FD_SYM` 25.62 s
/ 1447 (+13%) / one element different.  `UNSYM` stays, off by default,
because it is the only mode that is a Newton method and `s3rad` is unmeasured.

## §4 - re-measure the walls: **the tangent does not move them**

All four tangent modes wall `fast-wrapped` between `theta` 0.15875 and
0.15877 with the **identical 64-element deletion set**.  The §1 contamination
warning stands as a warning about *readings*; it is not an explanation of
*this* wall.

## §5 - scar tissue: **three of the four demonstrably do nothing, and none is deleted yet**

Leave-one-out on both fast decks is in `06-TANGENT-VERDICT`.  Rescue 2 and
the dogleg reproduce the shipped `fast-plain` run **to the digit** - they
never fire.  Dropping the line search is 1.9% fewer iterations there and a
wall 0.5% **later** on `fast-wrapped`.  All four off is better than all four
on, on the deck they were built for.

They are not deleted, and the reason is the brief's own rule: these were
added one per wall on `s3rad` and the DHC decks, and every number above is
from a two-minute deck.  What exists now instead is the gate saying it:
`fast-plain-noglob` and `fast-wrapped-noglob`.  The leave-one-out on `s3rad`
is what turns this into a deletion.

## §6 - the knobs

`CCX_DAMAGE_TANGENT=FD_SYM` deleted.  `CCX_DAMAGE_TANGENT_FULL` measured a
no-op on both fast decks and standing on `s3rad`.  Twelve gate cases now,
from nine.

## The two traps, and how they went

**"The tangent fix cannot be bit-identical."**  It was - every fix landed on
a path no default configuration takes, and all twelve gate cases are
byte-for-byte unchanged across all three commits, `m.frd` included.  That is
a weaker result than the brief expected, and it is the honest one: nothing in
the shipped configuration moved because the shipped configuration never ran
the broken code.

**"Element deletion is a threshold decision - say the criterion before the
run."**  Partly kept.  The physical criterion - same deletion set, same
severance `theta`, same terminal grip reaction - was fixed in advance and
passed on its own terms.  The *numeric* tolerance on `m.dat` for the
no-tangent case was guessed at `1e-4`, went red against a measured `2.18e-4`,
and was widened afterwards.  That is disclosed in the case's own prose rather
than quietly fixed.
