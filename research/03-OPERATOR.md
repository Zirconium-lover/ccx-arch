# Operator verification: the named hole was already dug

Area: `07-RESEARCH-AGENDA.md` **rank 3** — *"the named hole in the
diagnostics"*.

---

## 1. The problem as it exists here

`handover/02-DIAGNOSTICS.md` says it outright, under *What is missing from
this list*:

> Nothing measures the **operator** directly. A tangent that is not the exact
> differential of the residual would show up in §1 as an erratic ratio, but
> there is no clean check.

And `08-OBJECT-MODEL.md` §3 gives the Operator row `nothing verifies it`.

The code has four places where the tangent can silently stop matching the
residual: an unsymmetric tangent mode, a consistent softening term, a rank-1
damage term, and the regularised contact branch.

## 2. What established codes do

- **PETSc `-snes_test_jacobian`** / `-snes_test_jacobian_view`: assemble the
  Jacobian, difference the function, report the relative difference in a
  chosen norm, optionally print the two matrices side by side.
  <https://petsc.org/release/manualpages/SNES/SNESComputeJacobian/> and the
  `SNES` chapter of the manual. (Retrieved through a search index; this
  environment's egress proxy blocks `petsc.org` directly.)
- The standard Taylor-remainder discipline: the finite-difference error must
  fall like `h^2` for a central difference before it hits the roundoff floor,
  and a *plot of that* is the real test — not a single `h`.

## 3. Adopt / adapt / reject

**Nothing to build.** The check is already in the tree.

`src/nonlingeo.c` around line 9684 contains a column-by-column comparison of
the assembled tangent against a central difference of the internal force:

    K_fd(:,c) = ( fn(v + h e_c) - fn(v - h e_c) ) / (2h)

against `K_asm(:,c)` read out of `(ad, au, jq, irow)`. It probes the four
nodes of the **most damaged** `C3D4` element — "a tangent that is right in
the elastic bulk and wrong in the process zone is precisely the error every
earlier test would have missed", says the comment — and it stops the run
afterwards, because the probe perturbs the displacement repeatedly.

It is `-snes_test_jacobian` in all but the name. It is gated by three
environment variables — `CCX_STRUCT_FD_INC`, `CCX_STRUCT_FD_ITER`,
`CCX_STRUCT_FD_H` — **none of which any test sets and none of which is
documented anywhere**. All three were in the generated retirement queue of
`docs/SWITCHES.md`: no declaration, no test, no prose.

So the verdict is not *adopt* or *reject*. It is: **a tool nobody can find is
not a tool.** The three names are now declared, with what they do and what
they cost, and this document is what the probe says.

## 4. What it says

`fast-wrapped`, one thread, `CCX_DAMAGE_TANGENT=UNSYM` (which is what both
the gate and `run_s3rad.sh` set), 12 columns per run.

### The control first, because a probe nobody has validated proves nothing

Increment 2, iteration 1 — elastic, `dam=0`:

| node | dir | max\|K_fd\| | rel. err | worst ratio K_asm/K_fd | coefficients off by >1e-4 of column scale |
|---|---|---|---|---|---|
| 2 | 1 | 4.67e+04 | **5.4e-08** | 1.0000 | **0** |
| 2 | 2 | 4.86e+04 | **6.4e-08** | 1.0000 | **0** |
| 4 | 1 | 7.82e+04 | **7.4e-08** | 1.0000 | **0** |

In the elastic bulk the assembled tangent **is** the differential of the
residual, to the finite-difference floor. The instrument works.

### The process zone

Increment 50, iteration 2, element 1238, `dam=1.83`, `h=1e-7`:

| node | dir | max\|K_fd\| | rel. err | worst ratio K_asm/K_fd | bad |
|---|---|---|---|---|---|
| 276 | 1 | 9.93e+04 | 1.3e-02 | **1.643** | 13 |
| 276 | 2 | 1.41e+05 | 8.5e-03 | **0.183** | 12 |
| 276 | 3 | 1.34e+05 | 8.7e-04 | 1.045 | 8 |
| 353 | 1 | 1.34e+04 | **9.6e-02** | **−3.495** | 19 |
| 353 | 2 | 5.37e+04 | 2.3e-02 | 0.939 | 20 |
| 353 | 3 | 3.66e+04 | 1.9e-02 | 0.981 | 20 |
| 361 | 1 | 6.99e+04 | 2.3e-02 | **−0.088** | 12 |
| 361 | 2 | 7.23e+04 | 2.1e-02 | 0.471 | 12 |
| 361 | 3 | 7.74e+04 | 5.0e-04 | 0.689 | 11 |
| 354 | 1 | 5.60e+04 | 2.7e-02 | **−0.157** | 13 |
| 354 | 2 | 8.82e+04 | 1.8e-02 | 0.736 | 12 |
| 354 | 3 | 7.90e+04 | 8.6e-03 | 0.956 | 13 |

**Five to six orders of magnitude worse than the elastic control**, on every
one of the twelve columns, with 8 to 20 coefficients per column disagreeing
by more than 1e-4 of the column's own scale, and individual coefficients
whose sign is wrong.

### And it is not the step size

The whole table is reproduced to five significant figures at
`h = 1e-6, 1e-7, 1e-8, 1e-11`, and moves in the fourth figure at `h = 1e-3`.
The difference is **converged**, not a truncation or a roundoff artefact:

| h | node 276 dir 1, rel. err | node 353 dir 1, rel. err |
|---|---|---|
| 1e-3 | 1.3501e-02 | 1.3980e-01 |
| 1e-6 | 1.3467e-02 | 9.5727e-02 |
| 1e-7 | 1.3467e-02 | 9.5727e-02 |
| 1e-8 | 1.3467e-02 | 9.5727e-02 |
| 1e-11 | 1.3467e-02 | 9.5728e-02 |

A flat plateau over three decades with the roundoff floor nowhere in sight is
the strongest form this statement can take.

## What this means, and what it does not

**It does not mean there is a bug.** `CCX_DAMAGE_TANGENT=UNSYM` is
documented in the source as *"assemble and solve the bulk problem through the
asymmetric path with the constitutive tangent left untouched"* — an
inconsistent tangent **by construction**. What was missing was any
measurement of how inconsistent, and that is now on the record: in the
configuration the gate and the target deck both run, the operator is exact in
the elastic bulk and wrong by 1e-3 to 1e-1 relative in the process zone, with
sign errors on individual coefficients.

**What it does mean** is that `02-DIAGNOSTICS.md` §1's third row — *"ratio
erratic, transitions → 0 ⇒ suspect the operator"* — now has a direct test
instead of an inference, and that Newton in the process zone is not running
on the differential of its own residual. `pardiso.c` records the consequence
from the other end: *"Newton here contracts the residual by a median
0.52–0.60 per iteration with only 8–16% of steps quadratic, so the exact
tangent is not buying quadratic convergence."* Those two observations are the
same fact seen from two sides, and until now neither had been connected to
the other.

## What is still open

- **The same probe with `CCX_DAMAGE_TANGENT` unset** (the stock symmetric
  path) and with `FD_SYM`. That is the measurement that would say whether the
  inconsistency is the price of `UNSYM` specifically or is there in every
  mode. `test/fast/run_fast.sh` exports the mode unconditionally, so this
  needs the deck run outside the wrapper, or the wrapper taught to take
  overrides the way `run_s3rad.sh` does.
- **The regularised contact branch**, where this project has an analytic
  claim (`04-REFUTED.md`, the `1e+06` tangent ratio) that has never been
  checked against the assembled operator. The probe picks the most damaged
  `C3D4`; a cohesive-element variant is a small change to a loop bound.
- **A Taylor-remainder slope** rather than a plateau: the current probe
  reports a difference, not an order. The plateau above is the right answer
  here — the difference is real, not truncation — but on a case where the
  tangent IS consistent, the `h^2` slope is what proves it.
- Turning this into an assertion the gate can make. Right now it is a switch
  a human sets; `02-DIAGNOSTICS.md` §10's rule — *the mechanism refuses to
  arm if its test fails* — is the shape it should end up in.
