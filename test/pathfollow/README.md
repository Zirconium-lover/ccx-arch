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


## Crack-opening control (the method that works)

`CCX_PATHFOLLOW_COD=<d(phi) per increment>` controls the **mean normal
separation of the cohesive facets** instead of the dissipation.  Two
measurements forced this choice; both are reproducible with the probes
below.

**The dissipation constraint is structurally blind to this crack.**  On the
cohesive benchmark `dG` is identically zero: `P = f_hat^T u` tracked
`lambda*ff` to every printed digit (lambda=0.81486456, ff=4.999972e+02,
P=4.074302e+02) while the interface was failing.  Under displacement
control `f_hat = -dR/dlambda` lives on the dofs next to the loaded face,
and once the interface fails that whole block moves rigidly with the
prescribed face, so `f_hat^T u` stays proportional to lambda whatever the
crack does.  The earlier plastic bar hid this, because distributed plastic
work does register.

**The residual is not differentiable where the solver lands.**  The pure
tangent test converges to 8.7e-07 in the elastic range but sits at a
constant 4.23 relative error over five decades of eps at the limit point.
The UC6 tangent is not at fault (dumped `ctan(1,1) = -4.1667e+04`, exactly
the analytic `Kn*(1-df/(df-d0))`); the interface sits exactly at
`delta = d0 = 4.0e-04`, the initiation kink, where the tangent jumps from
+1e6 to -4.17e4.

Crack-opening control has no blind spot and its constraint is linear in u,
so both derivatives are exact by construction:

```
phi(u) = c^T u        c built once from the reference geometry
g      = phi - target        dg/du = c        dg/dlambda = 0
dlambda = -( g + c^T du_R ) / ( c^T du_F )
```

### Result

```sh
python3 mkcohesive.py -o cohesive.inp
CCX_PATHFOLLOW=1e30 CCX_PATHFOLLOW_COD=2e-6 ../../src/ccx_2.23 -i cohesive
```

| | stock | crack-opening control |
|---|---|---|
| accepted increments | 47, then "too many cutbacks" | **999** |
| lambda at the limit point | 0.805615 | 0.805613 (closed form 0.808) |
| post-peak accepted increments | 0 | **798** |
| lambda after the peak | — | descends to 0.705259 |
| end displacement | monotone | **runs backwards**, 0.040281 -> 0.035263 |
| constraint residual | — | `g ~ 1e-19` every increment |

and the branch is quantitatively right, not merely stable:

| phi | F measured | F closed form | error |
|---|---|---|---|
| 4.000e-04 | 400.0000 | 400.0000 | 0.000 % |
| 8.000e-04 | 383.3333 | 383.3333 | 0.000 % |
| 1.200e-03 | 366.6667 | 366.6667 | 0.000 % |
| 1.600e-03 | 350.0000 | 350.0000 | 0.000 % |
| 1.996e-03 | 333.5000 | 333.5000 | 0.000 % |

**The control increment matters, and the reason is the kink.**
`dphi=5e-5` stalls exactly at `phi = d0`; `dphi=2e-6` walks through.  A
step-time cutback cannot do this, because the obstruction is a tangent
discontinuity rather than a step that is too large in energy.

### The linearisation gate

`CCX_PATHFOLLOW_LINCHECK=<increment>` verifies, at the REAL base point
(after `prediction()` has extrapolated and the operator has been
assembled), with the full trial state restored before every evaluation:

* `q_FD = [R(u,lambda+eps) - R(u,lambda)]/eps`
* row 1: `[R(u+eps*p, lambda+eps*dl) - R(u,lambda)]/eps` against `K*p + q_FD*dl`
* a second sweep with `dlambda = 0`, which tests the tangent alone
* the frozen `f_hat` against `-q_FD`
* and its own idempotency, which is what caught two instrumentation bugs
  before they could produce a wrong conclusion

Measured: `cos(f_hat, -q_FD) = +1.0000000000` and `|f_hat|/|q_FD| = 0.994`
to `0.997` at BOTH the elastic state and the limit point, so the constant
reference vector is legitimate here - measured, not assumed.

### State lifecycle

`vold != vini` before the Newton loop is not corruption: `prediction.c`
extrapolates `v = vold + dtime*veold` for `iinc>1` with `idiscon==0`
(static branch), after which `results()` runs and `v` is copied into
`vold`.  Confirmed in the source and by the probe, which reports
`|vold-vini| = 0` at the top of an attempt and non-zero one statement
before the loop.

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
| `CCX_PATHFOLLOW_COD=<dphi>` | **crack-opening control** - the method that follows the branch |
| `CCX_PATHFOLLOW_LINCHECK=<inc>` | full linearisation gate at that increment |

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

All five self tests can be run together, outside CalculiX:

```sh
cd src
cat > /tmp/all_main.c <<'EOF'
#include <stdio.h>
#include "CalculiX.h"
int main(void){ ITG a=pathfollow_selftest(); printf("\n");
                pathfollow_legacycheck();    printf("\n");
                ITG c=crackcontrol_selftest(); printf("\n");
                ITG d=topodiag_selftest();     printf("\n");
                ITG e=lsladder_selftest();     printf("\n");
                ITG f=damstate_selftest();
                return ((a==0)&&(c==0)&&(d==0)&&(e==0)&&(f==0))?0:1; }
EOF
gcc -Wall -O2 -DARCH="Linux" -I. -o /tmp/all_test /tmp/all_main.c \
    pathfollow.c crackcontrol.c topodiag.c lsladder.c damstate.c \
    u_calloc.c u_free.c -lm
/tmp/all_test
```

Measured here: all five PASSED, 0 failures.  Each also runs inside
CalculiX at arm time, and each refuses to arm its feature on a failure.

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

---

# Mixed-mode crack control

`CCX_PATHFOLLOW_COD` controls the mean NORMAL separation.  That is the
right coordinate for the benchmark above and the wrong one for a real
mixed-mode front, so `CCX_CRACK_CONTROL` generalises it.  The two are
mutually exclusive and the old one is left untouched, so everything above
remains a regression test.

## What is controlled

The UC6 law advances along one scalar per integration point,

    deff^2 = max(dn,0)^2 + beta*(ds1^2 + ds2^2) ,   beta = (Ts0/Tn0)^2

which is not linear in `u` but IS positively homogeneous of degree one.
Freezing

    q = ( max(dn,0), beta*ds1, beta*ds2 ) ,   m = q/deff

at a state gives `m . d == deff` exactly (Euler) and `m` is the exact
gradient of `deff`, so

    phi(u) = sum_ip w_ip * m_ip . [[u]]_ip

is affine during one corrector, its value at the linearisation point is
the weighted mean of `deff`, and both derivatives are exact.

The functional is rebuilt from the COMMITTED state at the top of every
attempt and the constraint is written incrementally,
`g = c^T(u - u_n) - dphi`, so redefining it between increments carries
nothing over.  That is also what repairs the absolute form's stale-vector
defect when element deletion changes the equation count.

## Weights

For the bilinear law the dissipated energy per unit advance of `deff` is
the constant `1/2*Tn0*df/(df-d0)`, so weighting by `area*that` over the
loading part of the process zone makes `phi` the cohesive dissipation -
the Gutierrez constraint written for the crack instead of for the loading
boundary.

| `CCX_CRACK_CONTROL_MODE` | selection |
|---|---|
| `MEAN` | every live UC6 point |
| `ZONE` | the process zone `d0 < dmax < df` |
| `DISS` (default) | the process zone **where it is loading**, `deff >= dmax` |

All three are normalised by the weight they carry, so `dphi` is a length
in every mode.  That cannot change the step: the bordered row is
invariant under `c -> s*c` with `dphi -> s*dphi`.

The loading restriction is not a tuning knob.  Measured on `s3rad` at
stock increments 160-175:

| | 160 | 175 |
|---|---|---|
| process zone | 11002 | 10874 |
| of those, loading | 4716 | 4105 |
| mean `deff` over the zone | 4.007e-03 | 3.924e-03 |
| mean `deff` over the loading part | 6.215e-03 | 6.347e-03 |

The zone mean runs *backwards*, because points leave the zone into
failure faster than the survivors open.  The loading mean is monotone and
its rate (+8.8e-06 per increment) is the natural control increment.

## The benchmark

```sh
cd test/pathfollow
python3 mkmixed.py -o mixed.inp            # already committed
../../src/ccx_2.23 -i mixed                # stock -> the wall
CCX_PATHFOLLOW=1e30 CCX_CRACK_CONTROL=4e-6 CCX_CRACK_CONTROL_ENGAGE=1 \
    CCX_PATHFOLLOW_DTHETA=2e-4 ../../src/ccx_2.23 -i mixed
python3 check_mixed.py <rundir>
```

A bar cut by an INCLINED cohesive plane.  Every node is held in y and z,
so the model is still an exact 1-D chain, but the axial jump resolves
onto the tilted facet frame and loads a fixed, non-trivial mode mix:

    dn = D*c , |ds| = D*sqrt(1-c^2) , deff = D*kappa ,
    kappa = sqrt(c^2 + beta*(1-c^2)) , c = n_x

The closed form is exact including the St-Venant-Kirchhoff bulk -
CalculiX runs UC6 only on the NLGEOM path, so the small-strain form
`u = D + sigma*L/E` is wrong by 4e-03, which is what the first version of
`check_mixed.py` measured before it was corrected.

Measured, on this container:

| | 45 deg (36% shear) | 76 deg (90% shear) |
|---|---|---|
| stock accepted increments | 62, rc=201 | 62, rc=201 |
| crack control accepted | **4999** | **4950** |
| of those, post-peak | **2400** | **2400** |
| recomputed `deff` vs printed | 1.2e-07 | 4.5e-07 |
| shear fraction | 0.3600 as predicted, 2.5e-07 | 0.9000, 8.8e-07 |
| reaction vs closed form | 5.0e-07 | 6.7e-05 |
| displacement vs closed form | 4.4e-07 | 3.1e-06 |
| `|g|` at an accepted state | 2.2e-18 | 5.6e-18 |
| end displacement | 0.05026 -> 0.02263, backwards | 0.12583 -> 0.02584 |

Both worst cases sit at the increment that crosses the initiation kink,
which is the same non-differentiability the pure-tangent probe found on
the Mode-I benchmark.

## Environment variables

| variable | effect |
|---|---|
| `CCX_CRACK_CONTROL=<dphi>` | arm mixed-mode control with that control increment |
| `CCX_CRACK_CONTROL_MODE=MEAN\|ZONE\|DISS` | which points carry weight (default `DISS`) |
| `CCX_CRACK_CONTROL_ENGAGE=<inc>` | engage at that increment; `0`/unset engages as soon as the process zone is non-empty |
| `CCX_CRACK_CONTROL_EPS=<eps>` | probe jump used to capture `-dR/dlambda` (default 1e-5) |
| `CCX_CRACK_CONTROL_GROW=<f>` | factor `dphi` is grown by after an accepted increment (default 1.1) |

## Two lifecycle points that had to be right

**The stock predictor had to go.**  `prediction()` extrapolates
`v = vold + dtime*veold`, which is correct when the step time IS the load
parameter.  Here it moved the control coordinate before the corrector
ran - measured, `phi` extrapolated to 1.65e-04 against a target of
1.0e-05 - so every corrector opened by having to undo 94% of it, and a
cutback made that worse because it shrinks `dphi` faster than the
predictor.  `prediction()` skips the extrapolation when `idiscon != 0`,
so suppressing it is one assignment.  The method keeps its own predictor.

**The reference load has to be a tangent.**  `f_hat` is captured as
`b/dlamjump` at the first iteration.  Over the previous accepted step
(2e-03 on the target) that is a secant, and an honest finite difference
of the residual put `|f_hat|/|dR/dlambda|` at 1.43.  A scale error there
leaves the displacement half of the corrector right and the load half a
factor `1/s` short, so the prescribed dofs end up where the free dofs did
not assume: the constraint row converged to 1e-19 while the equilibrium
row stalled at 0.17, on nodes next to the loaded face.  Capturing over a
small probe jump instead costs nothing and puts the ratio at 1.0000.
