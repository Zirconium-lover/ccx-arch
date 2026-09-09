# CCX crack-propagation COD workbench

For the current second-wall investigation, start with [`NEXT_TASK.md`](NEXT_TASK.md).

This repository is a public experimental workbench for continuing a
CalculiX 2.23 fracture calculation through a snap-back wall.  It is not a
production release of CalculiX.

The current code contains a bordered path-following driver, a proven
Mode-I crack-opening-control (COD) benchmark, a mixed-mode control
coordinate built on the effective separation the UC6 law itself advances
along, and a mixed-mode benchmark with a closed form.  The engineering
target is the real mixed-mode `s3rad` model included in this repository.

## Current evidence

On the clean cohesive benchmark, stock displacement control stops at the
limit point.  COD follows 798 consecutive accepted post-peak increments,
with the load factor and loaded-end displacement both turning backwards and
the reaction matching the closed-form bilinear branch.

That result proves the continuation mechanism on a small Mode-I problem.  It
does **not** prove the mixed-mode `s3rad` calculation.

On the mixed-mode benchmark - a bar cut by an INCLINED cohesive plane, so
a fixed non-trivial mode mix is loaded through the whole branch - stock
control stops after 62 increments and the mixed-mode control follows 2400
consecutive post-peak increments with the reaction on the closed form to
5e-07 and the constraint residual at 2e-18.  The same passes at 36% and
at 90% shear.

On `s3rad` the front was measured, not assumed: 63% of the process-zone
integration points carry more than half of `deff^2` in shear and 39%
carry more than 90%, so normal opening is not the coordinate there.

**The `s3rad` wall itself turned out not to be a continuation problem at
all.**  It is the adaptive damage line search: its backtracking ladder is
`{1.0, 0.5, 0.1}`, in 46.5% of its activations the best step lies below
that floor, and when nothing contracts it takes the last rung anyway - at
the wall that rung raises the residual for five iterations running while a
step of 0.03 lowers it every time.  With the ladder deepened and the best
rung taken, the same binary on the same deck with `DEADALL=1.e-2`
unchanged goes from `theta=0.212177` to `theta=0.255574`, deleting 3734
elements in 415 batches against 2416 in 234.  Three topological
explanations - orphan degrees of freedom, a detached component, and
amplification of the near-null space - were each refuted by measurement
first.  See `test/s3rad/README.md`.

## Repository map

- `src/` — CalculiX 2.23 sources and the experimental path-following code.
- `src/pathfollow.c` — the bordered algebra and its self test.
- `src/crackcontrol.c` — the mixed-mode control coordinate and its self test.
- `src/lsladder.c` — the backtracking ladder of the damage line search,
  extracted with a regression test that pins the defect it used to have.
- `src/topodiag.c` — connectivity, orphan-dof, rank and residual-projection
  measurements; diagnostic only, nothing on a solution path.
- `src/Makefile.ubuntu2404` — reproducible open build using SPOOLES.
- `src/Makefile.ubuntu2404.mkl`, `src/build_mkl.sh` — reproducible
  PARDISO/MKL build, out of tree so its objects can never be mixed with
  the SPOOLES ones.
- `test/pathfollow/` — analytical bar, Mode-I cohesive and mixed-mode
  benchmarks, with `check_mixed.py` and `plot_control.py`.
- `test/s3rad/` — the self-contained target deck, its baseline
  provenance, `uc6census.py` (front census from the deck's own output)
  and `mkpilotdeck.py` (reduced-output derivation with a printed diff).
- `CLAUDE.md` — working context and experimental gates for a new coding
  session.

## Quick start

On Ubuntu 24.04:

```sh
sudo apt-get update
sudo apt-get install -y gcc gfortran make libblas-dev liblapack-dev \
    libarpack2-dev libspooles-dev python3
make -C src -f Makefile.ubuntu2404 -j4
```

Reproduce the small COD benchmark before changing the implementation:

```sh
cd test/pathfollow
../../src/ccx_2.23 -i cohesive
CCX_PATHFOLLOW=1e30 CCX_PATHFOLLOW_COD=2e-6 \
    ../../src/ccx_2.23 -i cohesive
```

Then the mixed-mode gate:

```sh
../../src/ccx_2.23 -i mixed
CCX_PATHFOLLOW=1e30 CCX_CRACK_CONTROL=4e-6 CCX_CRACK_CONTROL_ENGAGE=1 \
    CCX_PATHFOLLOW_DTHETA=2e-4 ../../src/ccx_2.23 -i mixed
python3 check_mixed.py .
```

See `test/pathfollow/README.md` for the complete evidence and diagnostics.

## Important solver distinction

The historical `s3rad` wall was measured with PARDISO and the target deck
contains `*Static, Solver=Pardiso`.  The open Ubuntu makefile builds SPOOLES,
which is useful for unit tests and exploratory comparisons but is not a
silent substitute for the recorded PARDISO baseline.  A coding session must
either establish a reproducible PARDISO/MKL build or clearly label a copied
SPOOLES deck as exploratory.  That build now exists:

```sh
sudo apt-get install -y intel-mkl        # universe/multiverse, 2020.4.304-4
./src/build_mkl.sh                       # -> build-mkl/ccx_2.23_pardiso
```

Every `s3rad` number in this repository was produced with it.  No SPOOLES
run is quoted against the PARDISO baseline.

Generated binaries, solver outputs, logs and plots must remain untracked.
