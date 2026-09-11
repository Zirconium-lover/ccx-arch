# 9. Was the specimen in two pieces, and would anything have noticed?

Raised as a hypothesis: `s3rad` probably broke in two and `[FRACTURE COMPLETE]`
did not catch it.  It is a good hypothesis and it is checkable offline from a
finished run - the expanded deck, the deletion record, the per-element damage
dump and the cohesive state print are all there.  Here is the answer, which is
**no** to the literal claim and **yes, and worse** to the point behind it.

Everything below is from `test/s3rad/_runs/prof2`, the run that reached its own
ending: `rc=201`, increment 599, `theta=0.2550244`, 3741 elements deleted.
`[FRACTURE COMPLETE]` fired **zero** times.

## It was not two pieces

The load-path sweep the solver runs, done offline, under every rule the code
implements, on the final state:

| link rule | cohesive facets | live elements | reached from X0 | XL nodes reached | verdict |
|---|---|---|---|---|---|
| NODE | every facet conducts (**shipped**) | 39150 | 39150 | 180 of 180 | connected |
| NODE | dead facets excluded (`DEADFACET`) | 37385 | 37385 | 180 of 180 | connected |
| NODE | **no facet conducts at all** | 33750 | 24976 | 180 of 180 | connected |
| FACE | every facet conducts | 39150 | 39138 | 180 of 180 | connected |
| FACE | dead facets excluded | 37385 | 37368 | 180 of 180 | connected |
| FACE | **no facet conducts at all** | 33750 | 24958 | 180 of 180 | connected |

Of the 5316 cohesive facets still present, **1765 (33.2%)** are fully failed by
`damstate_facet_dead`'s own rule.  Excluding them changes nothing.  Deleting
*all 5400* interface facets outright changes nothing: the two grip faces are
still linked through surviving bulk.

So the two refinements that exist for exactly this failure -
`CCX_FRACTURE_LINK=FACE` and `CCX_FRACTURE_DEADFACET`, both switched off by
`run_s3rad.sh` - would not have fired either.  The termination test did not
miss a disconnection.  **There was no disconnection to miss.**

Sweeping a damage threshold instead of a deletion list says the same thing
from the other side: with every cohesive facet removed, the bulk alone keeps
the faces connected until elements below `D=0.8` are called dead, and only
severs at `D<0.7`.

## It was finished anyway, and by a wide margin

The grip reaction, from the deck's own `*Node Print, Nset=FACE_XL_NSET,
Totals=Yes`:

| theta | reaction | of peak |
|---|---|---|
| 0.1575 | **3112.81** | **100% (peak)** |
| 0.2100 | ~1556 | 50% |
| 0.2211 | ~778 | 25% |
| 0.2360 | ~311 | 10% |
| 0.2455 | ~156 | 5% |
| **0.2550 (the wall)** | **66.45** | **2.1%** |

And what that cost, mapping load factor to wall clock through the interim
profile tables:

| the run continued past | at increment | at wall second | fraction of the 69 minutes still to come |
|---|---|---|---|
| 50% of peak | 339 | 2052 | **50.7%** |
| 25% of peak | 450 | 2886 | **30.7%** |
| 10% of peak | 520 | 3399 | **18.4%** |
| 5% of peak | 557 | 3687 | 11.4% |

**Nearly a fifth of the run - 12.8 minutes of 69 - happened after the specimen
had lost 90% of its load-bearing capacity**, and half the run happened after it
lost half.  The last 38 increments, below 5% of peak, are the ones where the
rescue apparatus fired twelve times at bit-identically the same place
(`research/08-S3RAD-COMPLETE.md`) before the cutback hit `tmin`.

## What this actually says

The hypothesis was that the test missed a severance.  The truth is sharper and
harder to fix: **the test asks a topology question, and the thing that
finished is a load.**  A specimen holding 2.1% of its peak force through a
thread of barely-damaged bulk is, to any experimentalist, broken; to a
connectivity sweep it is intact, and correctly so.  No setting of
`CCX_FRACTURE_LINK`, `CCX_FRACTURE_DEADFACET` or `CCX_DAMAGE_DEADALL` changes
that, because all four termination switches in this tree are topological.

This is also why the ending looks like a solver failure when it is not.  The
run stops with `*ERROR: increment size smaller than minimum` - which reads as
Newton giving up - on a structure that stopped being a specimen several
thousand seconds earlier.  `09-STEERING` asks for a solver where "the answer
is *the model is wrong, change the deck*, not *go debug the Newton loop*".
Here the model was not wrong; it was **over**, and nothing in the deck could
say so.

## The gap, stated as a deck statement

What is missing is not a smarter sweep.  It is one number the person writing
the deck already knows and currently cannot express:

> stop when the reaction on the loaded set falls below a stated fraction of
> its own peak.

That is a physical termination criterion, it is a property of the experiment
rather than of the solver, it needs no connectivity argument, and it would
have ended this run at increment 520 instead of 599 - saving 18.4% of the wall
clock and, more importantly, removing the twelve-firing rescue pile-up and the
`tmin` error from the record entirely, because neither would have happened.

It belongs next to `CCX_FRACTURE_TERMINATION` rather than inside it: the
existing switch names *which sets* define the load path, and this one names
*how weak* that path may get.  Note that the parallel line of work is building
`loadpath.c` as the owner of "is this still a specimen", so the criterion
above is offered as a measurement and a specification, not as a second
implementation of the same idea.

## Method, so this can be repeated or attacked

`/tmp` scratch scripts did the sweeps; the inputs are all committed run
artefacts: `m.inp` (the expanded deck, with `*Nset` and both element blocks),
`m.damage` (the deletion record), `m.de1.vtk` (`DE1_D` and `ELEMENT_ID` per
surviving bulk cell at increment 598) and the last
`internal state variables ... for set INTERFACE` block of `m.dat`, where a
facet is dead when value 4 - `xstate` index 3, the same index
`damstate_facet_dead` reads - is at or above 0.5 at all three integration
points.
