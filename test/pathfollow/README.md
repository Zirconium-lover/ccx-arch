# Dissipation path following — build and reproduction

Everything below was run in the container this work was done in.  Numbers
quoted in the commit messages come from these exact commands.

## Environment

| item | value |
|---|---|
| OS | Ubuntu 24.04.4 LTS (Noble), x86_64, kernel 6.18.44 |
| C compiler | gcc 13.3.0 (Ubuntu 13.3.0-6ubuntu2~24.04.1) |
| Fortran compiler | gfortran 13.2.0 (Ubuntu 4:13.2.0-7ubuntu1) |
| make | GNU Make 4.3 |
| linear solver | **SPOOLES 2.2** (`libspooles-dev` 2.2-14.1build2), `*SOLVER=SPOOLES` / default `isolver=0` |
| eigensolver | ARPACK 3.9.1 (`libarpack2-dev` 3.9.1-1.1build2) |
| BLAS/LAPACK | Ubuntu reference `libblas-dev` / `liblapack-dev` |
| PARDISO/MKL | **not linked.** See below. |
| python (plots) | python3 + matplotlib 3.11.1 (`pip3 install matplotlib`) |

### Dependencies

```sh
apt-get update
apt-get install -y gcc gfortran make libblas-dev liblapack-dev \
                   libarpack2-dev libspooles-dev
```

### Build

```sh
cd src
make -f Makefile.ubuntu2404 -j4        # produces src/ccx_2.23
```

`Makefile.ubuntu2404` deliberately avoids `-march=native` so the binary is
reproducible across machines, and uses `-O2`.  Two flags are required and
are not in the stock makefile:

* `-fallow-argument-mismatch` — CalculiX passes scalars where arrays are
  declared in several legacy interfaces (gfortran ≥ 10 rejects this).
* `-cpp` — `move.f` is the only fixed-form source carrying `#if`
  directives.

The snapshot did **not** compile before this branch; see the first commit.

### PARDISO / MKL variant

Not built here, on purpose: the open path had to build and run without a
proprietary component, and the path following is solver-agnostic (it
dispatches on `isolver` exactly like the stock solve).  To add it:
`apt-get install intel-mkl` (universe/multiverse, 2020.4.304-4), then add
`-DPARDISO` to `CFLAGS`, `pardiso.c` is already in `Makefile.inc`, and link
`-lmkl_intel_lp64 -lmkl_core -lmkl_gnu_thread`.  `isolver=7` then selects
it and the second (path-following) solve uses `pardiso_main` instead of
`spooles`.

Note that the pre-existing `CCX_DAMAGE_CONTINUATION` / `CCX_DISSIPATION_CONTROL=2`
paths are PARDISO-only and cannot run in the open build at all.

## The benchmark

```sh
cd test/pathfollow
python3 mkbar.py -o snapback.inp          # regenerate (already committed)
```

A bar of 20 unit slices, each split into 6 C3D4 tetrahedra (DE1 damage
evolution is C3D4-only).  Every slice shares one elastic-plastic law so the
bar yields uniformly and contracts laterally without mismatch — with `nu=0`
the field is uniformly uniaxial, which linear tetrahedra represent exactly.
One slice may damage.  Closed form for the defaults:

```
snap-back indicator r = L_e*sigma_0/(E*u_f) = 3.80   (>1 required)
peak      u = 0.040402   (lambda = 0.73458),  sigma_0 = 400.016
failure   u = 0.010400
the controlled displacement must travel BACKWARDS by 0.030
```

The element-level crack-band indicator CalculiX prints is 0.2, i.e. below
one: the snap-back is *structural*, from the elastic release of the other
19 slices, so a local indicator cannot see it.  That is deliberate.

## Runs

```sh
# 1. stock displacement control -> the wall
./../../src/ccx_2.23 -i snapback

# 2. path following
CCX_PATHFOLLOW=2e-3 CCX_PATHFOLLOW_CLIP=1e-3 ../../src/ccx_2.23 -i snapback

# 3. plot
python3 plot_run.py --dat off_new/snapback.dat:stock \
                    --dat on_run/snapback.dat:pathfollow \
                    --log on_run/run.log:pathfollow \
                    --upeak 0.040402 --ufail 0.010400 --out curve.png
```

Measured:

| | stock | path following |
|---|---|---|
| peak reaction | 400.0073 (analytic 400.016) | 400.0073 |
| peak lambda | 0.737442 (analytic 0.73458) | 0.735964 |
| u monotone? | yes, max 0.040559 | **no**, largest backward step −1.598e-04 |
| last state | u=0.040559 F=390.67 | u=0.040318 F=398.37, lambda=0.733058 |
| exit | 201, "too many cutbacks" | 201, "too many cutbacks" |

## Environment variables

| variable | effect |
|---|---|
| `CCX_PATHFOLLOW=<tau>` | arm with dissipation increment `tau` per increment.  Unset = feature off = byte-identical to the stock binary. |
| `CCX_PATHFOLLOW_CLIP=<x>` | cap on \|dlambda\| per Newton iteration (default 0.05; 1e-3 used above) |
| `CCX_PATHFOLLOW_DTHETA=<x>` | step time per increment once engaged (default 1e-3) |
| `CCX_PATHFOLLOW_PROBE` | per-iteration lambda, dG, g, refusal reason |
| `CCX_PATHFOLLOW_ACCUMCHECK` | `f_hat^T(vold-vini)` from the model vs the driver's view, plus `\|vold-vini\|` at the top of an attempt and before the Newton loop |
| `CCX_PATHFOLLOW_SOLVECHECK` | `\|K*du_R-rhs\|/\|rhs\|` and `\|K*du_F-fhat\|/\|fhat\|` by sparse mat-vec |
| `CCX_PATHFOLLOW_PROJECT` | put lambda exactly on the constraint at iteration 1 (off; measured worse) |

## Self tests

The bordered algebra is checked at arm time and the run refuses to arm if
it fails.  To run it standalone:

```sh
cd src
cat > /tmp/pf_main.c <<'EOF'
#include <stdio.h>
#include "CalculiX.h"
ITG pathfollow_selftest(void);
ITG pathfollow_legacycheck(void);
int main(void){ ITG a=pathfollow_selftest(); printf("\n");
                ITG b=pathfollow_legacycheck(); return ((a==0)&&(b==0))?0:1; }
EOF
gcc -Wall -O2 -DARCH="Linux" -I. -o /tmp/pf_test /tmp/pf_main.c pathfollow.c -lm
/tmp/pf_test
```

`pathfollow_selftest` verifies dG against its definition, row 2 of the
extended system against a finite-difference directional derivative taken in
`u` and `lambda` simultaneously, and BOTH rows of the bordered system on an
SPD tangent and on an **indefinite** one (the only case the method exists
for).  `pathfollow_legacycheck` measures the pre-existing
`CCX_DISSIPATION_CONTROL=2` scalar row on the same data and shows it leaves
an O(0.5) residual in the constraint it is supposed to enforce.

## Artifacts

`.dat`, `.sta`, `.frd`, `.log`, `.png` are gitignored on purpose — only the
inputs and the generators are committed.
