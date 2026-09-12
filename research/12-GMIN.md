# 12. The residual-stiffness floor: what it costs and what it buys

`damggmin` floors `g = 1 - D` so a fully damaged element keeps a fraction of
its stiffness until the terminal scan deletes it.  Its own comment in
`resultsmech.f` states the trade:

> At 1e-4 a few such elements are harmless, but a notch process zone holds
> thousands of them at once and the operator becomes badly scaled: that is
> the regime where the DHC runs stall.  Raising the floor trades a small
> unreleased load for conditioning.

Half of that was measured this session - the **cost**: `research/01-PROFILING.md`
shows the floor's conditioning forcing PARDISO into 8062 iterative-refinement
steps over 5344 solves on `s3rad`, about 5.6% of the run.  The **benefit** had
never been measured at all.  This is that measurement.

## The sweep

Six decades, both fast decks, everything else shipped, two threads.

`fast-plain`, which completes:

| gmin | rc | inc | iters | deletions | wall |
|---|---|---|---|---|---|
| 1e-06 | 0 | 507 | 1283 | 90 | 26.0 s |
| 1e-05 | 0 | 507 | 1283 | 90 | 25.0 s |
| **1e-04** (default) | 0 | 507 | **1283** | 90 | 24.6 s |
| 1e-03 | 0 | 508 | 1284 | 90 | 26.0 s |
| 1e-02 | 0 | 508 | 1311 | 90 | 25.9 s |
| 1e-01 | 0 | 512 | 1363 | 90 | 28.8 s |

**Insensitive over four decades and mildly costly above them.**  Refinement
steps are zero at every value - this deck has no conditioning regime at all,
so it cannot answer the question the floor exists for.

`fast-wrapped`, which walls:

| gmin | rc | inc | theta | iters | deletions | iters/inc |
|---|---|---|---|---|---|---|
| 1e-06 | 201 | 99 | 0.158766 | 606 | 64 | 6.12 |
| 1e-05 | 201 | 99 | 0.158766 | 606 | 64 | 6.12 |
| **1e-04** (default) | 201 | 99 | 0.158766 | 606 | 64 | 6.12 |
| 1e-03 | 201 | 99 | 0.158766 | 618 | 64 | 6.24 |
| **1e-02** | **0** | **520** | **1.000000** | 1354 | **64** | **2.60** |
| 1e-01 | 201 | 61 | 0.108003 | 445 | 62 | 7.30 |

**At 1e-02 the wall is gone.**  The run completes the load history, with the
**identical 64-element deletion set** - verified by hash, not by count - and
the cost per increment **halves**, 2.60 Newton iterations against 6.12.

At 1e-01 it is worse than the default: the run walls *earlier*, at increment
61, and loses two elements from the fracture.  So the useful band is narrow,
and the default sits four decades below it.

## Two cures, one wall

`fast-wrapped-smooth` already removes this same wall with the crack-face
regulariser, and arrives at the same place: `rc=0`, `theta=1.0`, the same 64
elements, at increment 515 against the floor's 520.

**Two mechanisms that share no code remove one wall and produce one
fracture.**  That is the strongest statement available about what the wall
is: a conditioning phenomenon, reachable from either the contact law or the
bulk stiffness floor, and not a unique defect in either.

## What this does not yet say

The fast decks show **zero** refinement steps at every floor value, so the
conditioning mechanism the floor is *for* does not exist on them - what is
measured above is something else that happens to respond to the same knob.
The claim in the comment is about "a notch process zone holding thousands of
floored elements at once", and only `s3rad` has one.  The arm that tests it
is running: `CCX_DAMAGE_GMIN=1.e-2` against the default, with the refinement
counter as the conditioning proxy.

---

# At scale the trade inverts

`s3rad`, shipped configuration, `CCX_DAMAGE_GMIN=1.e-2`, four threads,
against the same deck at the default:

| | ends at | theta | deletions | wall | refinement steps | how it ends |
|---|---|---|---|---|---|---|
| 1e-04 (default) | 599 | 0.2550244 | 3741 | 4170 s | **8062 / 5344 solves, mean 1.509** | `rc=201` tmin |
| **1e-02** | **250** | **0.196059** | 881 | 1270 s | **10 / 1844 solves, mean 0.005** | `rc=201` tmin |

## The conditioning claim is confirmed, and quantified

`resultsmech.f` says the floor buys conditioning.  It does, and the size of
the effect is not subtle: **iterative refinement essentially disappears**,
from 1.509 steps a solve to 0.005 - a factor of three hundred.  PARDISO stops
needing to refine at all.  The 5.6% of runtime that the floor was costing in
refinement is gone.

That is the first direct measurement of what the floor is for, and the
mechanism is exactly the one claimed.

## And the run fails earlier anyway

It walls at increment **250**, `theta=0.196`, against the default's 599 and
0.255 - **23% less of the load history**, having deleted 881 elements against
3741.  Same ending, `*ERROR: increment size smaller than minimum`, and the
same rescue pile-up: eight `[DAMAGE RESCUE] WALL` events, six of them
`divergence-cutback-below-tmin` rather than the `too-slow` that ends the
default run.

**Better conditioning, worse outcome.**  The two are not the same axis.

The cost showed up before the failure did.  At `theta = 0.1955`, reached by
both, the fracture is the same - 766 deletions against 765, so the floor is
not distorting the physics there - but the raised floor needed **412 attempts
against 327**, 26% more, to get there.

(One methodological note, because it nearly produced a wrong report: compared
at the same INCREMENT the two runs look wildly different - 1334 deletions
against 772 at increment 234 - and compared at the same THETA they are
identical.  The increment number is not a physical coordinate.  `09-STEERING`
says exactly this and it caught me anyway.)

## What it means

The floor's benefit on `fast-wrapped` was real - it removed a wall - but it
does not generalise.  On the deck that matters, raising it four decades
removes the conditioning problem completely and the run still fails, earlier,
by divergence rather than by slow cutback.

So **conditioning is not what limits `s3rad`.**  The 5.6% the floor costs in
refinement is buying something that is not the binding constraint, and
lowering the floor would not help either - 1e-06 through 1e-03 are
indistinguishable on both fast decks.

**The default stays at 1e-04.**  It sits in the flat band on every deck
measured, and the one value that changes anything for the better does so only
on a deck small enough not to have the process zone the floor was written for.

What this rules out is worth as much as what it finds: the next place to look
for the `s3rad` wall is not the operator's scaling.  Six of the eight wall
events here are `divergence`, not `too-slow` - the Newton iteration is
diverging, not creeping - and that is a different investigation.

---

# DEADALL at 5e-2: the wall moves 0.18% and stays

`CCX_DAMAGE_DEADALL` deletes the elements around a node whose entire live
support has fallen below a fraction of its stiffness and which carries no
cohesive facet.  The decks run it at `1e-2`.  Raised to `5e-2`, five times
more permissive:

| | ends at | theta | deletions | wall | DEADALL firings | refinement | ending |
|---|---|---|---|---|---|---|---|
| 1e-02 (deck default) | 599 | 0.2550244 | 3741 | 4170 s | 148 | 8062 / 5344, mean 1.509 | `rc=201` tmin |
| **5e-02** | **607** | **0.2554880** | 3747 | 3493 s | **148** | 7580 / 5133, mean 1.477 | `rc=201` tmin |

**Eight increments further, and 0.00046 more load factor - 0.18%.**  Then the
same `*ERROR: increment size smaller than minimum`.

Three things in that table are worth more than the headline.

**The firing count is identical: 148 and 148.**  A five-fold loosening of the
threshold does not change how often the mechanism engages.  That says the
nodes it acts on are not marginal - when a node's whole live support dies it
dies far below either threshold, so the value of the knob between 1e-2 and
5e-2 is almost immaterial to when it fires.

**The fracture moves, slightly.**  At the baseline's own stopping load factor
the looser threshold has deleted 3744 elements against 3741, and the sets are
not identical.  Three elements, out of 3741, at the very end - small, and
real.  Element deletion is a threshold decision and this is what one looks
like.

**Conditioning barely moves**: 1.477 refinement steps a solve against 1.509.
This knob is not a conditioning knob.

## What the two knobs together rule out

`CCX_DAMAGE_GMIN` raised a hundredfold removes the conditioning problem
entirely - refinement from 1.509 steps a solve to 0.005 - and the run then
fails **earlier**, at increment 250.

`CCX_DAMAGE_DEADALL` raised fivefold changes conditioning not at all and buys
eight increments.

So the `s3rad` wall is not the operator's scaling, and it is not the survival
of nearly-dead elements around unsupported nodes.  Both were plausible, both
are now measured, and both are out.

What is left pointing somewhere: of the six wall events in the default run's
tail, the ones that end it are `too-slow-cutback-below-tmin`, but in the
raised-floor arm six of eight were `divergence-cutback-below-tmin` - the
Newton iteration diverging rather than creeping.  The next question is which
of those the wall actually is, and that is a question about the residual, not
about any of these thresholds.
