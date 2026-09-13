# The trap, taken apart

*2026-09-13.  Offline analysis of four completed arms — no new runs.  All
four are binary `389909a7` on the committed deck; `base2` carries no
overrides, `dafacet` only `CCX_DAMAGE_DEADALL_FACET=1`, `spconly` only
`CCX_DAMAGE_AUTOSPC_FORCE=1`, `spcforce` both.*

`14-THE-TRAP.md` established that the wall is a trap entered on a
one-element perturbation.  This is what is inside it.

## The trap is two elements

Every arm that gets past theta 0.255 deletes the same two elements.  The
arm that does not, does not.

| arm | element **19535** — last bulk at node 1246 | element **42198** — last bulk at node 7118 | max theta |
|---|---|---|---|
| `base2` | inc 648 @ 0.256013 | inc 628 @ 0.254500 | 0.556913 |
| `spconly` | inc 639 @ 0.256077 | inc 628 @ 0.254500 | 0.557017 |
| `spcforce` | inc 668 @ **0.254999** | inc 683 @ 0.259500 | 0.556896 |
| `dafacet` | **ALIVE** | **ALIVE** | **0.254991** |

Node 1246 has six bulk elements and six cohesive facets.  Five of the six
bulk elements are deleted at increments 191, 204, 307, 331 and 514 — *in
every arm, identically*.  The sixth is 19535.  Node 7118 has ten bulk
elements and no facets; nine go identically in every arm, and the tenth is
42198.

So at theta ≈ 0.2545 the specimen has two nodes each hanging on exactly one
element, and the run continues only if both of those elements come out.

**`dafacet` died 8e-06 of theta short.**  Its last accepted step is theta
0.254991; `spcforce` removes 19535 at 0.254999.  That is the whole margin:
0.003 % of load.

## Why the element does not come out

In `dafacet`'s final state element 19535 stands at instantaneous damage
**D = 1.0000**.  The terminal criterion is `Ddelete = 0.9990` applied to the
**viscous** damage, which lags it (`beta = dtime/(eta+dtime)`, `eta = 1e-4`).
The batch prints at increments 651–654 show the mechanism working exactly at
that edge: `batch_Dvis` = 0.99906, 0.99903, 0.99910, 0.99924.

So the deadlock has this shape:

1. node 1246 keeps one element, 19535, whose stiffness is `g = 1e-4` — the
   residual-stiffness floor;
2. that node is where the residual sits: it carries the largest residual
   force in 121 of the 633 prints after the divergence, more than any other;
3. the increment diverges and the step is cut;
4. deletion is judged on a viscous variable that advances by `beta` per
   accepted increment, and `beta` falls with the step — 0.229 at increment
   640, 0.0103 at the last;
5. the element stays, the node stays, the step keeps collapsing.

**A hypothesis I tested and rejected.**  The obvious reading of step 4 is
that the viscous damage *freezes*: `beta -> 0` as `dtime -> 0`, so the
variable converges to a limit below the threshold and the element can never
be deleted.  That is not what the numbers say.  Summing `beta` over
`dafacet`'s remaining accepted increments gives **19.4** — the viscous
variable has ample room left to travel.  The deadlock is not arithmetic
starvation of the viscous update, and I have not established what it is.

## What decides which arm falls in

Up to theta 0.2500 `base2` and `dafacet` have deleted **exactly the same
3711 elements**.  Between 0.2500 and 0.2550 they part: `dafacet` deletes
thirteen elements `base2` does not, and `base2` deletes one `dafacet` does
not — 42198, node 7118's last support, 0.171 from node 1246.

The first divergence is a single deletion.  At increment 610 the narrowed
facet guard let node 2613 qualify, and element **17889** came out — 0.639
away from node 1246, in a part of the specimen that has nothing to do with
the trap.  Eighteen increments later `base2` takes 42198 and walks on;
`dafacet` never does.

That is the whole causal chain, and it is worth stating plainly: **a
deletion 0.64 away, eighteen increments early, decides whether the specimen
finishes breaking.**

## What this does to the AUTOSPC_FORCE retraction

`797fdfd` retracted the claim that `CCX_DAMAGE_AUTOSPC_FORCE` moves
anything, on the grounds that `base2` and `spconly` land in the same place.
That retraction stands for the three arms that never meet the trap.  The
2x2 is now complete and has one cell the retraction did not cover:

| | no `DEADALL_FACET` | `DEADALL_FACET` |
|---|---|---|
| **no `AUTOSPC_FORCE`** | `base2` — no trap | `dafacet` — **TRAP** |
| **`AUTOSPC_FORCE`** | `spconly` — no trap | `spcforce` — no trap |

`DEADALL_FACET` alone traps; `DEADALL_FACET` with the force mask does not.
In that one cell the force mask is what got 19535 deleted — at theta
0.254999, against `dafacet`'s terminal 0.254991.  **A margin of 8e-06.**
One cell, one arm, a margin five orders below the quantity: this is a
coincidence until it is repeated, not a mechanism, and it does not reinstate
the retracted claim.

## The gate does not cover this

`fast-wrapped` also ends `rc=201`, and it is a different animal.  Its wall
node 440 has **all four** of its bulk elements deleted and three live
cohesive facets — a purely cohesive-supported node, the Class B conditioning
case.  The s3rad trap node has one *live* bulk element at the stiffness
floor and six live facets.  The three-minute gate does not reproduce the
thing that stops the target deck.

## What would settle it

The trap is reproducible: commit `5420dac` still carries
`CCX_DAMAGE_DEADALL_FACET`, so building that commit and running the deck
with the switch gives `dafacet` again, deterministically.  Against that
fixture three hypotheses are one run each:

1. **the viscous lag is the blocker** — run with `CCX_DAMAGE_VISCOSITY=0`,
   so deletion is judged on the instantaneous damage.  If 19535 goes and the
   run walks on, the criterion is the defect;
2. **the threshold is the blocker** — lower `Ddelete` below 0.999;
3. **the mechanism is starved on the cutback path** — `DEADALL` runs only
   where an increment converged without a cutback, which is precisely not
   the state the trap is in.

Hypothesis 3 is the one I would test first, because it needs no change to
any criterion: it asks whether the mechanism written to free exactly this
node is ever *called* while the run is in the trap.
