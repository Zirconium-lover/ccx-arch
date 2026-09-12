# 9. Was the specimen in two pieces, and would anything have noticed?

Raised as a hypothesis: `s3rad` probably broke in two and `[FRACTURE COMPLETE]`
did not catch it.  It is a good hypothesis and it is checkable offline from a
finished run - the expanded deck, the deletion record, the per-element damage
dump and the cohesive state print are all there.  Here is the answer, which is
**no** to the literal claim and **yes, and worse** to the point behind it.

Everything below is from `test/s3rad/_runs/prof2`, the run that reached its own
ending: `rc=201`, increment 599, `theta=0.2550244`, 3741 elements deleted.
`[FRACTURE COMPLETE]` fired **zero** times.

## CORRECTION, same day

The first version of this page concluded "there was no disconnection to
miss", on the strength of the connectivity sweeps below.  That conclusion was
**wrong**, and the error was in the measure, not in the sweeps.

Connectivity is a yes/no question and the honest answer to it is yes.  The
physical question is **area**: the smallest total face area you would have to
break to separate the two grips.  That is a minimum cut, and on the element
adjacency graph - each shared triangular face weighted by its own area times
the smaller of the two elements' `g = 1 - D` - it comes out at

    minimum cut = 0.0031

against a nominal bar section of about 5.7.  **The specimen is held together
by a ligament worth 0.05% of a cross-section.**

Everything else on this page stands; what changes is the verdict.  To any
engineering standard the specimen IS severed, and it carries 2.1% of peak
load through a thread five ten-thousandths of a section wide.  The sweeps
say "connected" and they are right; the answer is simply not the question.

That also explains the observation that some models print
`[FRACTURE COMPLETE]` and others do not, seemingly by luck.  It is luck:
whether the last thread happens to be **deleted** or merely **damaged to
within one part in two thousand of nothing**.  One element decides it.

Three measures of the same final state, in increasing order of usefulness:

| measure | value | verdict |
|---|---|---|
| topological connectivity, any of six rules | connected, 180 of 180 grip nodes | intact |
| matrix area per slab at the worst station | 63% of the full section | intact |
| **minimum cut, area x residual stiffness** | **0.0031, = 0.05% of a section** | **severed** |

The first is what the code computes.  The third is what the question means.

## It was not two pieces, in the sense the code asks

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
miss a disconnection **of the kind it looks for**; see the correction above
for the kind it does not.

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

---

# What the ligament actually is

The minimum cut is not a band, a thread or a process zone.  It is **one
triangular element face**.

    the cut          1 face, area 0.01092, weighted 0.003054
    joining          bulk element 29869 (D = 0.000000)
                  to bulk element 39927 (D = 0.720346)
    shared nodes     251, 3595, 7180
    centroids        (2.092, 3.575, 1.491) and (2.011, 3.575, 1.530)

Verified the blunt way rather than trusted from the max-flow: remove that
single adjacency from the sweep and leave everything else in place, and
**grip B becomes unreachable**.  Nothing else connects the two halves.

Three things about it, and each one matters.

**It is at a free corner.**  The specimen's cross-section spans y from 0.02
to 3.58 and z from 0.02 to 1.58.  The two elements sit at y = 3.575 and
z ≈ 1.51 - the outermost edge in both.  That is the least stressed place in
the section, which is exactly why the damage variable never got there: one
of the two elements is at **D = 0.000**, entirely undamaged, and the other at
0.720, well below the 0.999 that triggers terminal deletion and nowhere near
the residual-stiffness floor.

**The damage model is not wrong about it.**  A free corner carries little
load while the section is intact, so it damages last.  The crack swept the
whole section and left the corner behind.  This is not a defect in the
constitutive law; it is what a local damage law does at a free edge.

**But a single shared face between two tetrahedra is a hinge.**  It
transmits force through three nodes and has essentially no bending
stiffness: the joint has near-zero-energy rotational modes.  `damconnect.f`
already makes this argument one level down, for a *vertex* contact, and
records what it cost - E-75, "two pieces of 12500 and 13300 elements ...
joined through ONE tetrahedron touching each of them at a single vertex ...
the node-level sweep reads it as a load path, so `[FRACTURE COMPLETE]` never
fired".  The `FACE` link rule was written for that case and it does not help
here, because here the connection genuinely *is* a whole face.  The rule is
correct and the answer is still useless.

## Which settles what the wall is

The run ends with `reason=too-slow-cutback-below-tmin` and
`*ERROR: increment size smaller than minimum`, which reads as Newton giving
up.  It is not.  **By increment 599 the model is a mechanism**: two bodies
joined at one triangle, under prescribed displacement, with the hinge free to
rotate.  A mechanism has a singular or near-singular tangent; the Newton
direction is unbounded and no step length rescues it.  Cutting back the
increment cannot help, because the problem is not that the step is too large.

That also disposes of a hope raised by the survey.  **Dissipation path
following cannot remove this wall** - and not because it is badly
implemented.  A path follower traces an equilibrium branch through a limit
point or a snap-back by choosing a better control parameter.  Here there is
no branch left to trace: the structure has lost the degrees of freedom that
made it a structure.  Controlling the load parameter differently does not
give a hinge a stiffness.

## What would actually produce two pieces

The specimen is severed everywhere except one corner face, and the corner
face will never fail on its own, because a local damage law at a free edge
under a load path that no longer passes through it has nothing to respond to.
So the decision has to be made by the *model statement*, not by the
constitutive law:

1. **Judge the load path by area, not by existence.**  `CCX_FRACTURE_LINK`
   already chose between two boolean conduction rules - NODE and FACE - after
   E-75 showed a vertex was not a load path.  The same argument, taken one
   step further, says a *single face* is not a load path either, and the step
   after that says the question was never boolean.  A minimum cut is the
   quantity; a threshold on it, stated in the deck as a fraction of the
   nominal section, is the criterion.  On this run the cut falls to 0.05% of
   a section, so any sane threshold fires long before increment 599.

2. **Then the specimen separates on its own.**  Once the last hinge is
   recognised, deleting it - or simply stopping - ends the run with the two
   pieces the experiment produces, and `[FRACTURE COMPLETE]` becomes a
   statement about the specimen rather than a lottery on whether the final
   element happened to cross a deletion threshold.

Both are model statements a deck author can make and defend.  Neither is a
solver change.

---

# The cut trajectory, measured

`src/loadcut.c` turns the load path from a boolean into a width.  Run on the
target deck in exact mode - `CCX_FRACTURE_CUT=0.005 CCX_FRACTURE_CUT_EXACT=1`,
shipped configuration otherwise, 4 threads - it produced **154 measurements**
over 66 minutes before the run was killed externally at increment 274.

The reference cut at the first committed batch is **5.570258**, against a bar
whose nominal cross-section is about 5.7.  The measure reproduces the
specimen's own section without being told it, which is the best sanity check
that was available.

| increment | theta | cut | ratio | grip load / peak | topology says |
|---|---|---|---|---|---|
| 91 | 0.17950 | 5.57026 | 1.000 | 0.993 | connected |
| 140 | 0.18962 | 5.00876 | 0.899 | 0.972 | connected |
| 184 | 0.19321 | 4.52006 | 0.811 | 0.924 | connected |
| 209 | 0.19684 | 3.85390 | 0.692 | 0.792 | connected |
| 241 | 0.19816 | 3.66461 | 0.658 | 0.744 | connected |
| 274 | 0.20124 | 3.41864 | 0.614 | 0.695 | connected |

and the end state of the complete run, from the offline analysis above:

| 598 | 0.25502 | 0.0031 | **0.00056** | **0.021** | connected |

**The cut ratio tracks the load fraction.**  Not by construction - the cut is
a property of the mesh and the damage field, and the reaction is a property of
the boundary - but over this range they move together, the cut leading the
load slightly.  That is the evidence that the minimum cut is measuring the
thing that carries the load, and it is what makes a threshold on it a
physical statement rather than a numerical one.

It also settles the choice of a default.  Anything between 1% and 10% of the
original width sits deep in the tail where the specimen is carrying a few
percent of peak and the run is grinding towards `tmin`.  The measured
alternative - a reaction-drop criterion - needs the deck to name a load set
and works only where one exists; the cut needs neither, and on a specimen
loaded through a pressure or a body force it is the only one of the two that
can be stated at all.

## Two wrong death reports, one root cause

The table above stops at increment 274 because that is where the run was when
it was sampled.  It did not stop there.  It was declared dead twice - once as
a hang in the max-flow, once as an external kill - and both times it was alive
and progressing; it passed increment 430 with the cut still falling smoothly.

Two liveness checks produced those readings, and neither looks wrong when you
read it:

- **`pgrep -c <name>` reports zero for a running process whose name exceeds
  fifteen characters.**  Linux truncates the `comm` field at fifteen and
  `ccx_2.23_pardiso` is sixteen.  `pgrep` prints a warning saying precisely
  this, and the check that produced the wrong answer had stderr redirected to
  `/dev/null`.
- **A file's mtime is "it stopped there" only when compared against the
  current time.**  Read on its own it is simply *now*, which is what a file
  being actively written looks like.

The habit that would have caught both: `ps -eo pid,etimes,comm`, or
`pgrep -af` against the full path, and confirm a counter is CHANGING rather
than that a timestamp exists.  Nothing in the measurement was wrong - the cut
trajectory above is real - only the reports of the runs' deaths.

---

# The measure, corrected, and what it then said

The first working version of `src/loadcut.c` gave **every surviving cohesive
facet g=1**, regardless of its damage.  That is the boolean error in weighted
clothing - a separated interface read as a rigid link - and it is the error
this whole page is about.

It was caught the only way it could have been: **two independent
implementations of the same quantity disagreeing by a factor of 380.**  On the
final state of the complete run the offline analysis gave a cut of 0.0031 and
the in-solver measure gave 1.184.  The offline one excluded facets entirely;
the in-solver one let 5316 of them conduct at full strength, and 1765 of those
are fully failed by `damstate_facet_dead`'s own rule.

Facets now carry their own `g = 1 - D`, read from `xstate` index 1 over the
integration points with the worst point governing, floored at `gmin`, and a
facet flagged failed at index 3 drops to the floor.  Three self-test checks
pin it, on a mesh that mirrors the real one - a zero-thickness facet between
two tetrahedra with duplicated nodes, which is how the deck builds its ZrH
interface.

## And then `fast-wrapped` said this

| increment | cut | ratio | what the topology says |
|---|---|---|---|
| first batch | 0.8701933 | 1.000 | connected |
| 36 | **0.00028** | **0.00032** | **connected** |

`2.800000e-04` is exactly what **one face of area 2.8 at the residual-stiffness
floor** `g = 1e-04` is worth.  At increment 36 the specimen is held together by
a single floored face and nothing else.

The deck's own ending is at increment **99**, `rc=201`, 64 deletions.  So on
this deck - a sixteen-second run - the solver spends **63 further increments
and two thirds of its wall clock** driving a specimen that is already in two
pieces, and deletes eighteen more elements from it, before giving up with an
increment-size error.

That is the `s3rad` story reproduced in six seconds instead of sixty-nine
minutes, and it is now two gate cases:

- `fast-wrapped-cut`, threshold `1e-09` and therefore unreachable, proving the
  measurement does not perturb the run it measures: all five output files
  byte-identical to `fast-wrapped`.
- `fast-wrapped-severed`, threshold `0.001`, pinning `rc=0` at increment 36 -
  **a clean ending at the separation instead of `rc=201` sixty-three
  increments later.**

The difference between those two endings is the whole point.  One says the
solver gave up; the other says the specimen broke.
