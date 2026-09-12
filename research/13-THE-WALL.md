# The s3rad wall: what held it, and what took it down

*Measured 2026-09-12.  Binary `5420dac`, deck `m12_s3rad_gc24_w.inp`
(sha256 2fb0cf4…), `OMP_NUM_THREADS=MKL_NUM_THREADS=6`, `MKL_CBWR=COMPATIBLE`
on every arm.*

## The wall, stated as a measurement rather than as "it stops"

The target deck ends `rc=201` — *increment size smaller than minimum*.  That
is the symptom.  The diagnosis is in `m.cvg`:

```
att 4 iter 1   resid 0.5790E+00 corr 0.1431E-01
...
att 4 iter 8   resid 0.5632E+00 corr 0.6332E-02
largest residual force= 0.002071 in node 1246 and dof 1
largest correction to disp= 4.173813e-08 in node 2509 and dof 3
```

The residual does not diverge.  It **creeps**: 0.5790 → 0.5632 over eight
iterations, about 0.4% per iteration, while the corrections are at 4e-08.
This is an iteration that has stopped moving while equilibrium is
unsatisfied — a residual component in a direction where there is no
stiffness left to remove it.  No displacement can fix it, so Newton cuts
the step, and cuts it again, until the step underflows.

Node **1246** carries the largest residual force in all twenty sampled
prints.  Offline analysis of the final state:

| | |
|---|---|
| bulk elements touching it | 6 — five deleted, the sixth at `D = 1.0000` exactly, so `g` at the residual floor `1e-4` |
| cohesive facets touching it | 6 — **five fully failed** by `damstate_facet_dead`, the sixth with two of its three integration points gone |

That is a free swinging point.

## Arms

All three arms share the rising branch exactly: peak grip reaction
**3112.81 at theta 0.1575** in every one of them.  They differ only at the
wall.  Compare at the reaction, not at the increment number — the increment
number is *where the solver gave up*, which is the thing under test.

| arm | switches added | rc | last inc | theta | grip reaction | % of peak |
|---|---|---|---|---|---|---|
| `prof2` (baseline) | — | 201 | 599 | 0.255023 | 66.4 | **2.13 %** |
| `dafacet` | `CCX_DAMAGE_DEADALL_FACET=1` | 201 | 664 | 0.254991 | — | — |
| `spcforce` | `DEADALL_FACET + AUTOSPC_FORCE` | 201 | 997 | 0.556835 | 1.3501 | 0.0434 % |
| `spconly` | `CCX_DAMAGE_AUTOSPC_FORCE=1` alone | 201 | 1003 | **0.557017** | 1.3502 | **0.0434 %** |

## What did NOT take the wall down: narrowing the DEADALL guard

`CCX_DAMAGE_DEADALL` deletes the dead bulk around a node whose entire live
support is dead, and skips any node a cohesive facet still holds.  The count
it used included facets that had **separated**, and a separated facet ties
nothing — node 1246 was being skipped on the strength of five failed facets.
`CCX_DAMAGE_DEADALL_FACET` narrows the count to live facets
(commit `5420dac`).

The narrowing is correct and it is not enough.  Node 1246 goes from six ties
to one, and **one live facet is still a tie**, so the guard still skips it —
by its own contract, correctly.  Measured: theta 0.254991 against the
baseline's 0.255023, i.e. the arm walls **0.013 % earlier**, at the same
node, with the same creeping-residual signature.  The extra 65 increments are
the same state ground finer, not progress.  Two extra DEADALL firings
(27 vs 25, 124 elements vs 122) changed nothing.

**The hypothesis was half right.**  "Dead facets hold the node fictitiously"
— confirmed.  "Therefore the node will be freed" — refuted.  What holds the
wall is the sixth facet, which has lost two of three integration points and
is still, by the all-points rule, alive.

## What did take it down: telling the FORCE norm what AUTOSPC already knew

`damstate.c`'s own header says it:

> The second s3rad wall was exactly the gap between the first two: AUTOSPC
> knew node 1246 had lost its load path and told the displacement norm,
> while the force norm — which is what actually vetoed the increment — was
> never told.

`CCX_DAMAGE_AUTOSPC` is in the deck's recipe at `1.e-3`.
`CCX_DAMAGE_AUTOSPC_FORCE` is not, and was not in any s3rad run in this
line of work.  At the wall, **78 nodes** are below `1e-3` and masked from
the displacement norm while the force norm still vetoes on them.

Arming it moves the run from theta 0.255023 to **0.556835** — past the wall
at increment 674 — and the grip reaction follows the specimen down:

| theta | reaction | % of peak |
|---|---|---|
| 0.255006 | 66.69 | 2.14 % ← baseline stops here |
| 0.260500 | 36.08 | 1.16 % |
| 0.261437 | 26.73 | 0.86 % |
| 0.267500 | 17.65 | 0.57 % |
| 0.556834 | 1.35 | **0.043 %** |

A factor of **50 further down the softening branch**, with 3808 elements
deleted against the baseline's 3744.

## The honesty check this switch demands of itself

`CCX_DAMAGE_AUTOSPC_FORCE` is declared with a warning: *if the excluded
residual stops returning to zero the mechanism has become a fiction and is
hiding a real imbalance*.  That is the test that decides whether the result
above is physics or bookkeeping.  Over the whole run, 3 000+ prints:

| | accepted increments | rejected attempts |
|---|---|---|
| count | 521 | 620 |
| final excluded residual / tolerance, max | **0.0079** | 17.81 |
| prints above 1.0 × tolerance | **0** | 8 |

and restricted to the 124 accepted increments **past the old wall**, the
max is 0.0079 and the count above tolerance is again **0**.  Typical accepted
values are 1e-11 to 1e-5 — the excluded dofs do come back to equilibrium,
they are simply slower than the criterion allows mid-iteration.  Within an
increment the sequence is the characteristic one: 1.07e-2 → 5.9e-3 →
1.7e-3 → 6.7e-5.

Every one of the eight over-tolerance prints is on an attempt that was
**thrown away**.  The last of them is the final diverging attempt at
increment 997, where the excluded residual is 5.8323 of a total 5.8327 at
17.7 × tolerance: at the very end the mask is holding essentially the whole
residual.  That attempt was rejected and the run stopped, which is the
mechanism reporting its own limit rather than concealing it.

**Verdict: not a fiction.**  On this deck, at this threshold, the mask
excludes dofs that do reach equilibrium.

## Attribution, measured

`spconly` carries `CCX_DAMAGE_AUTOSPC_FORCE` and nothing else.  It lands on
the same place as the two-switch arm:

| | `spcforce` (both) | `spconly` (force mask alone) |
|---|---|---|
| theta | 0.556835 | 0.557017 |
| grip reaction | 1.3501 | 1.3502 |
| % of peak | 0.0434 % | 0.0434 % |
| elements deleted | 3808 | 3796 |
| DEADALL firings | 42 | 29 |

The two arms agree on theta to 0.03 % and on the terminal reaction to four
significant figures.  **The whole of the gain is `AUTOSPC_FORCE`.**

`DEADALL_FACET` is not inert — it fires thirteen more times and deletes
twelve more elements — and it still changes nothing that can be measured at
the reaction.  It is correct, it has a self test that can be shown failing,
it is bit-identical when off, and on this deck it buys nothing.  By
`CLAUDE.md`'s own test — *name the failure it addresses and the gate case
that would go red without it* — it is a **retirement candidate**, and is
recorded here as one rather than defended.

The honesty check holds on the isolated arm too: 525 accepted increments,
none with a final excluded residual above the tolerance (max 0.0155 of it),
including all 126 past the old wall; the 3 over-tolerance prints are all on
rejected attempts.

## What is still open

1. **There is still no `FRACTURE COMPLETE`.**  The run ends `rc=201`, just
   at 0.043 % of peak instead of 2.13 %.  `CCX_FRACTURE_CUT` was not armed
   in any of these arms; on the fast deck it ends the run at severance
   (`09-SEVERANCE.md`).  The combination `AUTOSPC_FORCE + FRACTURE_CUT` is
   the run that should produce a clean termination rather than a later wall.
3. **The threshold is the deck's, not a derived quantity.**  `1.e-3` came
   from the recipe.  Nothing here measures how the result moves with it.
4. **`DEADALL_FACET` is a retirement candidate**, per the section above.
