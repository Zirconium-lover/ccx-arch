# CCX crack-propagation COD workbench

This repository is a public experimental workbench for continuing a
CalculiX 2.23 fracture calculation through a snap-back wall.  It is not a
production release of CalculiX.

The current code contains a bordered path-following driver and a proven
Mode-I crack-opening-control (COD) benchmark.  The next engineering target is
the real mixed-mode `s3rad` model included in this repository.

## Current evidence

On the clean cohesive benchmark, stock displacement control stops at the
limit point.  COD follows 798 consecutive accepted post-peak increments,
with the load factor and loaded-end displacement both turning backwards and
the reaction matching the closed-form bilinear branch.

That result proves the continuation mechanism on a small Mode-I problem.  It
does **not** prove the mixed-mode `s3rad` calculation.

## Repository map

- `src/` — CalculiX 2.23 sources and the experimental path-following code.
- `src/Makefile.ubuntu2404` — reproducible open build using SPOOLES.
- `test/pathfollow/` — analytical bar and cohesive benchmarks.
- `test/s3rad/` — the self-contained target deck and its baseline provenance.
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

See `test/pathfollow/README.md` for the complete evidence and diagnostics.

## Important solver distinction

The historical `s3rad` wall was measured with PARDISO and the target deck
contains `*Static, Solver=Pardiso`.  The open Ubuntu makefile builds SPOOLES,
which is useful for unit tests and exploratory comparisons but is not a
silent substitute for the recorded PARDISO baseline.  A coding session must
either establish a reproducible PARDISO/MKL build or clearly label a copied
SPOOLES deck as exploratory.

Generated binaries, solver outputs, logs and plots must remain untracked.
