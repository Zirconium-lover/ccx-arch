# Operator verification: a kink and a wrong tangent are different things

Area: `07-RESEARCH-AGENDA.md` **rank 3** — *"the named hole in the
diagnostics"*.

> **This document replaces an earlier version whose central claim was not
> supported by its own evidence.** That version reported the assembled
> tangent wrong by up to 9.6e-02 in the process zone and offered, as proof
> that the difference was real, a plateau of the central difference over `h`
> from 1e-11 to 1e-8. A plateau does not prove that. **At a point of
> non-smoothness the central difference converges to the mean of the
> one-sided derivatives** — a stable, converged value, different from both
> branches, whose plateau in `h` is indistinguishable from that of a
> genuinely wrong tangent. The conclusion happens to survive, but it did not
> follow, and it is restated below from a test that discriminates.

---

## 1. The problem as it exists here

`handover/02-DIAGNOSTICS.md` said, under *What is missing from this list*:

> Nothing measures the **operator** directly.

That was wrong, and finding out why is the second half of `04-OPTIONS`: a
column-by-column finite-difference check had been in `nonlingeo.c` all along,
behind three environment names that no test set and no document mentioned.
All three sat in the generated retirement queue.

## 2. What established codes do

- **PETSc `-snes_test_jacobian`** — assemble, difference, report the relative
  difference; `-snes_test_jacobian_view` prints both matrices.
- The **Taylor-remainder discipline**: for a smooth function a central
  difference has `O(h^2)` truncation error, so the honest test is a *slope*,
  not a single `h`.
- Neither of those separates a kink from a wrong tangent, because both are
  built on the central difference alone. The discriminator below is not from
  the survey.

## 3. The discriminating test

Take the two one-sided differences **separately**:

    K_fwd = ( f(u + h e) − f(u) ) / h      K_bwd = ( f(u) − f(u − h e) ) / h

| | K_fwd vs K_bwd | best of them vs K_asm | verdict |
|---|---|---|---|
| smooth, tangent right | agree | agree | **OK** |
| **kink** | **disagree** | **one branch agrees** | **KINK** — the tangent is right, on one branch, and Newton is standing on the switching surface |
| **wrong tangent** | **agree** | **neither agrees** | **WRONG** — nothing is non-smooth here and the operator is not the differential |
| both | disagree | neither agrees | **BOTH** |

Everything is measured against the **scale of the column** — the largest
`|K|` in it — because an absolute error on a coefficient that is a millionth
of the column means nothing.

One property of the test has to be stated because it bounds every conclusion
below: **a one-sided pair only sees a kink the perturbation actually
crosses.** At an accepted iterate the switching surface is generally not
within `h`. So the reading is not "there is no kink", it is "there is no kink
within `h` of this state", and the distance to the nearest one is measured by
sweeping `h` until the pair separates.

## 4. What was built

`src/opcheck.c` — the classifier, the column comparison, the coefficient
reader, and a self test. `src/monitor.c` presents. `nonlingeo.c` keeps only
the three residual evaluations that feed it, and lost the coefficient reader
and eleven scratch variables.

Three evaluations per column now, not two: `f(u+h)`, `f(u−h)` and `f(u)`.

### It was demonstrated failing — three times, two of them for real

| break | what happened |
|---|---|
| deliberately: `<` → `<=` on the census threshold (`04-MONITOR`) | 3 of 3 gate cases red |
| **accidentally**: the "exactly at the threshold" self-test case written as `500. + 1e-4*1000.`, which is `500.1` and not representable — `500.1-500.` is `0.10000000000002274` | the self test failed, the probe refused to report, and the check was rewritten in powers of two |
| **accidentally**: the coefficient reader assumed `au` always carries an upper triangle at offset `nzs[2]` | see below |

The third is the one that matters, because it was silently producing
*numbers*. On the stock symmetric path there is no upper triangle, and every
above-diagonal read ran off the end of `au`. The first run of this probe on
`test/pathfollow/close.inp` reported the **cohesive tangent wrong by up to
1.0 of the column scale** — a spectacular finding, entirely fictitious. A
self test for the reader was written against a hand-built four-equation
matrix; it caught the first fix as *also* wrong (the mirror column index was
swapped) before any physics was read out of it. With the reader correct, the
same run reports **3.2e-14**.

That is the whole argument for a self test per object, made at this project's
own expense inside one afternoon.

## 5. What it says

`h = 1e-7` unless stated. Binary
`c97d46f2…`, one thread.

### The instrument, validated at both ends

| case | rel. err `|ctr − asm|` | one-sided `|fwd − bwd|` | coefficients not OK |
|---|---|---|---|
| `fast-wrapped` inc 2, elastic, `dam=0` | **5.4e-08** | 1.2e-07 | **0 of 15453** |
| `close` step 2 inc 18, cohesive facet, symmetric tangent | **3.2e-14** | 1.5e-07 | **0 of 96** |

And on the `close` facet the Taylor slope is textbook — `×100` per decade of
`h`, exactly `O(h^2)`:

| h | 1e-7 | 1e-6 | 1e-5 | 1e-4 |
|---|---|---|---|---|
| `|ctr − asm|` | 3.2e-14 | 7.5e-13 | 7.5e-11 | 7.5e-09 |

**The cohesive tangent is the differential of the cohesive residual.** So is
the elastic bulk tangent. The instrument is not biased toward finding fault.

### The kink is real, and it is at a distance

Same `close` facet, same state, sweeping `h`:

| h | `|fwd − bwd|` | linear in h? | classification |
|---|---|---|---|
| 1e-7 | 1.500e-07 | — | 16 OK |
| 1e-6 | 1.500e-06 | yes | 16 OK |
| 1e-5 | 1.500e-05 | yes | 16 OK |
| 1e-4 | 1.500e-04 | yes | 14 OK, **2 KINK** |
| 1e-3 | **6.451e-01** | **no — 430× the linear value** | 4 OK, **8 KINK**, 4 BOTH |

`|fwd − bwd|` proportional to `h` is the signature of a **smooth** function
(it is `h·f''` plus rounding). The departure between 1e-4 and 1e-3 is the
switching surface, and when the step crosses it the disagreement is **44% of
the column scale** while `min|side − asm|` stays at 5.1e-04 — the tangent is
still exactly right, on one branch. That is the crack-face closure kink,
measured on the operator for the first time; the tree had it as a factor of
`1e+06` in the *law* (`check_close.py`), never in the assembled matrix.

### The process zone: none of the discrepancy is a kink

`fast-wrapped` increment 50, iteration 2, element 1238 at `dam=1.83`, node
353 direction 1:

| h | `|ctr − asm|` | `|fwd − bwd|` | `min|side − asm|` | verdict |
|---|---|---|---|---|
| 1e-8 | 9.573e-02 | 7.41e-07 | 9.573e-02 | 18 WRONG |
| 1e-7 | 9.573e-02 | 7.47e-06 | 9.572e-02 | 18 WRONG |
| 1e-6 | 9.573e-02 | 7.47e-05 | 9.569e-02 | 18 WRONG |
| 1e-5 | 9.573e-02 | 7.47e-04 | 9.535e-02 | 10 WRONG, 8 BOTH |
| 1e-4 | 9.572e-02 | 7.47e-03 | 9.199e-02 | 4 WRONG, 6 KINK, 13 BOTH |

Read the three columns together:

- `|fwd − bwd|` is **exactly linear in `h` over four decades** — the residual
  is smooth here; no switching surface within about `1e-5` of the iterate.
- `|ctr − asm|` is **constant at 9.57e-02 over the same four decades** — not
  `O(h^2)`, so not truncation. A converged, real discrepancy.
- `min|side − asm|` equals it — **neither** one-sided derivative matches the
  tangent, which is the WRONG signature and the opposite of the KINK one.

Totals over 12 columns, 20604 coefficients: **0 KINK, 0 BOTH, 157 WRONG.**

**So: at this state, none of the process-zone discrepancy is a legitimate
kink.** The kinks exist and are enormous — the sweep above shows one at a
distance of `1e-5`–`1e-4` — and they are not what the tangent is getting
wrong.

### And it is not the unsymmetric mode

The earlier version attributed the discrepancy to
`CCX_DAMAGE_TANGENT=UNSYM`, documented in the source as *"the constitutive
tangent left untouched"*. **Measured, and wrong.** The same state with the
switch unset, on the stock symmetric path:

| arm | `|ctr − asm|` at node 353/1 | WRONG coefficients |
|---|---|---|
| `CCX_DAMAGE_TANGENT=UNSYM` | 9.573e-02 | 157 |
| stock symmetric | 9.622e-02 | 151 |

Identical to within the difference between two nearby iterates. The
inconsistency is in the bulk progressive-damage tangent in **both** modes.

### Where it is, precisely

The symmetric run probes the most damaged cohesive facet as well as the most
damaged bulk element:

| columns | coefficients | OK | WRONG |
|---|---|---|---|
| **bulk** (element 1238, `dam=1.83`) | 20604 | 20453 | **151** |
| **cohesive** (element 2161) | 30906 | **30906** | **0** |

The cohesive tangent is exact to 2.3e-07…8.6e-06. The bulk one is not. The
missing piece is the term that appears only while damage is *evolving* — the
consistent softening contribution — which is why the elastic control and the
walled state at increment 99 (element 1227, `dam=1.0067`, 20604 of 20604
coefficients OK) both come back clean.

## 6. What this means

`src/pardiso.c` records, from the other end and without knowing it was the
same fact: *"Newton here contracts the residual by a median 0.52–0.60 per
iteration with only 8–16% of steps quadratic, so the exact tangent is not
buying quadratic convergence."* It is not buying quadratic convergence
because **it is not the exact tangent**, and now that is measured rather than
inferred.

Two consequences for the work queue:

- **Tangent reuse (`08-OBJECT-MODEL.md` §4) now has a precondition.** Reusing
  a tangent that is already not the differential in the process zone is a
  different proposition from reusing a correct one, and the argument for it
  ("the exact tangent is not buying quadratic convergence anyway") is really
  an argument that the tangent is wrong. Fix the operator first, then measure
  reuse against a correct one.
- **The globalization mechanisms have a candidate root cause.** Six of them
  accumulated around walls in the process zone. An operator that is not the
  differential produces exactly the symptom `02-DIAGNOSTICS.md` §1 attributes
  to it — an erratic ladder — and none of the six addresses it.

## 7. What is still open

- **Which term is missing.** The measurement localises it to the bulk
  progressive-damage tangent while damage evolves; it does not name the term.
  `resultsmech.f` / `mafilldamas.f` is where to look, and the probe is now
  the instrument that would confirm a fix in one 25-second run.
- **Whether it is a defect or a design choice.** A secant tangent is a
  legitimate choice; an *undocumented* one that nobody could measure is not.
  This is recorded in `05-DEBT.md` rather than fixed here, because
  `PROMPT.md` says not to go hunting in the damage module.
- **`s3rad`.** Everything above is the fast deck. The probe costs three
  residual evaluations per column and stops the run, so it is affordable
  there too.
- **A gate case.** The check runs when a human sets three switches. The
  discipline of `02-DIAGNOSTICS.md` §10 — the mechanism refuses to arm if its
  test fails — is met for the *classifier*; what is missing is a case in the
  three-minute gate that would go red if the operator got worse.
