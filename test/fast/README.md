# Fast fracture regression specimen

The point of this deck is stated in the architecture audit
(`docs/ARCHITECTURE_AUDIT.md`, section 6): **validation currently costs 2.5
hours**, so no refactor of the 14000-line damage module can be checked
cheaply, and that single fact blocks every structural improvement.

| deck | elements | wall clock | exercises |
|---|---|---|---|
| `test/s3rad` | 42807 | **~2.5 h** | everything, far too slowly |
| `test/pathfollow/*.inp` | 1-D chains | seconds | cohesive law, continuation |
| **this** | **2196** | **58 s** | bulk damage, deletion, cutbacks, 3-D topology |

The 1-D chains cannot stand in for the wall classes: a chain has no node that
can end up hanging by one tetrahedron, which is the mechanism behind the
second `s3rad` wall.  This is the smallest thing that can.

## Build and run

```sh
python3 test/fast/mkfast.py -o fast.inp --from-deck test/s3rad/m12_s3rad_gc24_w.inp
```

Then run it with the same environment `test/s3rad/run_s3rad.sh` sets.

## What it is

A bar of tetrahedra, split at mid-span by a **partial** cohesive plane - a
pre-crack over the first half of the section - with a brittle band ahead of
the crack tip to localise it, pulled in displacement control.

The partial plane matters and was found by measurement.  A full-section
interface makes the wrong specimen: it fails first, the bulk unloads and
never damages, and the run reaches `theta=1` with **zero deletions**.  With a
pre-crack the remaining ligament has to tear through the bulk, which is what
produces deletion and fragments.

Material and section cards are **lifted verbatim from the target deck** by
the generator rather than restated, so the physics cannot drift from what
`s3rad` uses.  (Restating them by hand was the first attempt and was wrong -
the real cards are `*Damage Initiation, Criterion=Ductile,
Evolution=Displacement, Npoints=6` and a `*Depvar 4` cohesive material.)

## Measured behaviour, at `4394403`

| | |
|---|---|
| return code | 0, reaches `theta=1.0` |
| increments / attempts | 507 / 544 |
| elements terminally deleted | **90** |
| `too slow convergence` events | 2 |
| wall clock, 1 thread | **58 s** |

So it fractures with real deletion and exercises the cutback machinery, in
under a minute.  That makes it usable as a characterization baseline: any
refactor must reproduce these numbers exactly before it is allowed near
`s3rad`.

## What it does NOT yet do

It completes rather than walling, so it does not yet reproduce a
collapsed-support deadlock or a limit point.  A harder variant should be
derived - a longer ligament, or a tougher interface - and pinned once it
reproduces a wall in the same minute-scale budget.  Until then this deck
proves that a change is *harmless*, not that it *fixes* anything.
