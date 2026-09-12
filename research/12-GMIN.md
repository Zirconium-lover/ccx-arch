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
