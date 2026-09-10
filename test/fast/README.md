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

## It also demonstrates the severance blindness, in 58 seconds

With `CCX_FRACTURE_DEADFACET=1` the same deck stops itself:

```
[FRACTURE COMPLETE] inc=65 step_time=1.175000000000e-01
                    no surviving load path between FACE_X0_NSET and FACE_XL_NSET
```

Without it, the identical deck walks on to `theta=1.0` at increment 507 -
**442 increments after the specimen has actually separated**.  That is the
same blindness measured on `s3rad`, where the metal severs at `theta=0.3412`
and the run continues to `0.5575`, and it is the consequence of
`cohesive_uc6.f` pinning `g` at `gmin` while terminal deletion scans `C3D4`
only: a dead facet reads as a load path for ever.

Here it costs a minute to see instead of two and a half hours.

## What it does NOT yet do

It completes rather than walling, so it does not yet reproduce a
collapsed-support deadlock.  **Until it does, this deck proves that a change
is *harmless*, not that it *fixes* anything.**

A geometry sweep was run to try to make it wall, and it failed - which is
worth recording, because it says where the mechanism actually lives:

| variant | rc | wall clock | deleted | walls? |
|---|---|---|---|---|
| `--nx 10 --ny 6 --nz 6 --seed 0.5` | 0 | 58 s | 90 | no |
| `--seed 0.3` | 0 | 54 s | 126 | no |
| `--nx 14 --ny 8 --nz 8 --seed 0.4` | 0 | 208 s | 216 | no |
| `--nx 16 --seed 0.35` | 0 | 115 s | 126 | no |

More mesh and more deletion do not produce the wall.  The reason is
structural, and it is a difference from the target deck that this generator
does not yet reproduce: in `s3rad` the cohesive facets **wrap the eroding
phase** - `MATRIX` (ZR) and `PLATETANGENTIAL` (ZRH) with `INTERFACE` between
them - so when the hydride erodes, the facets around it fail and a node is
left without support.  Here the interface merely cuts the section, so the
crack runs through it cleanly and never isolates anything.

**Next step for this deck: put a ZRH inclusion inside the bar and wrap its
boundary in cohesive facets**, rather than making the mesh bigger.  That is
the geometry that manufactures fragments.
