# Fracture animation from a run

Two products, both XML VTU so ParaView opens them directly.

## 1. The whole crack history in one 49 KB file

```sh
python3 crack_history.py <deck>.inp <run>/m.damage crack_growth.vtu
```

Every bulk element the run destroyed, as a cell, carrying:

| cell array | meaning |
|---|---|
| `theta_deleted` | grip displacement at which the element died |
| `increment` | increment at which it died |
| `material_id` | 1 = ZR matrix, 2 = ZRH hydride |

In ParaView: open, apply **Threshold** on `theta_deleted`, and drag the upper
bound.  The crack grows as you drag.  Colouring by `material_id` shows the
hydride going first and the matrix afterwards.

This needs no special run - `m.damage` is written by every run.

## 2. The deformed animation

The solver already writes a frame per accepted increment when asked:

```sh
CCX_DAMAGE_VTK_SERIES=1 ...        # -> <job>.de1.NNNNN.vtk, one per increment
python3 vtk2vtu.py <run> <outdir> <stride>
```

Each frame carries the DEFORMED geometry, `DE1_D` (damage, straight from the
integration-point history, no extrapolation or nodal averaging) and
`MATERIAL_ID`, and holds only the LIVE cells - so terminal deletion shows as
material disappearing.  The converter writes `s3rad_NNNN.vtu` plus
`s3rad.pvd`, whose timesteps are the grip displacement, so ParaView plays it
as an animation on opening the `.pvd`.

`stride` subsamples: the legacy frames are about 3.2 MB each and a full run
writes hundreds, while a converted frame is about 0.4 MB with zlib.

## Checking the output

```sh
python3 check_vtu.py frame.vtu source.vtk
```

Reads the compressed appended data back and compares points, cells,
connectivity, offsets, cell types and the first cell array against the legacy
source.  Measured on frame 0: coordinates agree to 1.2e-07 (float32
round-off) and `DE1_D` to 5.2e-11.
