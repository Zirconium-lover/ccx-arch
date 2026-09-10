# 7. Research agenda: what to survey, and what to build from it

Eleven areas, ranked. The first five are expected to produce **tools**; the
rest may legitimately end in a written verdict. Everything named below is a
starting point for a search, not a citation — **verify it, version it, link
it.** Some of it will be misremembered; treat a name here as a lead.

---

## Rank 1 — Tolerance-aware result comparison. **This blocks everything else.**

**The problem here.** `test/regress/run.py` pins a case by scalars
(increment, `theta`, deletion count) and by **byte-for-byte identity** of
`m.sta`, `m.damage` and `mixed.sta`. Byte identity is a strong check and a
terrible foundation for refactoring: any restructuring that changes a
summation order fails it, and there is no way to express *"the answer is the
same to 1e-10"*. So the suite can prove a change is a no-op and cannot prove a
change is correct — which is precisely the constraint that produced the code
you are here to fix.

**Where to look.**
- **MOOSE test harness** — `Exodiff` and `CSVDiff` testers, absolute and
  relative tolerances per field, gold files, `rel_err`/`abs_zero`, and the
  `tests` spec-file format.
- **Code_Aster** test cases — `TEST_RESU` with reference values, `PRECISION`,
  `CRITERE='RELATIF'/'ABSOLU'`, and the discipline of a reference *value*
  rather than a reference *file*.
- **deal.II testsuite** — numdiff-based comparison and how it picks
  tolerances.
- **`exodiff`** itself (SEACAS) — field-by-field, per-timestep tolerance
  semantics worth copying even without Exodus.
- **CTest** — labels, cost-based scheduling, timeouts, `PASS_REGULAR_EXPRESSION`.

**Build.** A comparison layer with: per-quantity absolute and relative
tolerances; comparison of *fields* (`.frd`/`.dat`/`.vtk`) and not just status
lines; a declarative spec per case; and a clear statement in the output of
which tolerance each comparison used. Byte identity stays available as the
strictest setting, not as the only one.

---

## Rank 2 — A real options system, replacing 139 environment switches.

**The problem here.** 139 `CCX_*` names, 63 explained nowhere, 122 set by no
test. `src/damswitch.c` reports what is set and names unknown ones — that is
about 10% of what an options system should do, and it was written here from
scratch because nobody looked at how it is normally done.

**Where to look.**
- **PETSc options database** — `PetscOptionsBegin/End`, `PetscOptionsReal`,
  prefixes, `-help` producing the complete documented list, and
  **`-options_left`**, which warns about options that were set and never
  queried. That last one is exactly what `damswitch.c` reinvented, and PETSc's
  version is better.
- **MOOSE `InputParameters`** — typed, documented, range-checked,
  deprecatable, with the documentation generated from the declaration.
- **Abaqus keyword/parameter parsing** for how a solver-facing input language
  handles defaults and validation.

**Build.** One registry where a switch is *declared* with type, default,
range, one-line documentation and optional deprecation; parsed once;
validated; reported. `docs/SWITCHES.md` becomes generated from the
declarations rather than scraped from `getenv` calls. Then retire the
orphans: a switch with no declaration, no test and no prose has no defenders.

---

## Rank 3 — Operator verification. **The named hole in the diagnostics.**

**The problem here.** `handover/02-DIAGNOSTICS.md` says it outright: *nothing
measures the operator directly.* A tangent that is not the exact differential
of the residual would show up only indirectly, as an erratic ladder. The code
has an unsymmetric tangent mode, a consistent softening term, a rank-1 damage
term and a regularised contact branch — four places where the tangent can
silently stop matching the residual.

**Where to look.**
- **PETSc `-snes_test_jacobian`** and `-snes_test_jacobian_view` — the
  finite-difference comparison, what norm it reports, and how it picks the
  perturbation.
- **MOOSE** `-snes_type test` and its Jacobian debugging guidance.
- The standard directional-derivative check and its step-size selection
  (Taylor-remainder convergence: the error must fall like `h^2`, and a plot of
  that is the real test).

**Build.** A switch that, at an armed increment, computes `(R(u+h·d) −
R(u−h·d))/2h` for a few directions `d` and compares against `K·d`, reporting a
relative error and the Taylor-remainder slope. Then run it on every gate case
and record what it says — including on the regularised contact branch, where
the answer is a claim this project has made analytically but never measured
against the assembled operator.

---

## Rank 4 — Convergence as composable objects.

**The problem here.** "What counts as converged" is spread across
`checkconvergence.c`, the AUTOSPC displacement mask, `AUTOSPC_FORCE`'s
exclusion from the force residual, a reference-force floor, and a
slow-Newton extension. It is a judgement with no owner — and the handover
records a case where `AUTOSPC_FORCE` satisfies its own alarm condition for
having become a fiction.

**Where to look.**
- **Trilinos NOX `StatusTest`** — `NormF`, `NormUpdate`, `NormWRMS`,
  `MaxIters`, `FiniteValue`, `Stagnation`, `Divergence`, and `Combo` with
  AND/OR. A composable tree of tests, each of which reports *why* it fired.
  This maps onto the problem almost exactly.
- **PETSc `SNESConvergedReason`** — a single enumerated reason per solve, and
  the discipline of always having one.

**Build.** Convergence as a small tree of named tests, each with an owner and
a self test, each able to say why it passed or failed, with the reason
recorded per increment in machine-readable form. `AUTOSPC` and
`AUTOSPC_FORCE` become tests in that tree rather than special cases inside it.

---

## Rank 5 — Diagnostics as data, and the playbook as a program.

**The problem here.** Diagnostics are `printf` in ad-hoc formats, interleaved
with the solve, gated by environment variables. `02-DIAGNOSTICS.md` describes
eleven readings that a human performs by eye.

**Where to look.**
- **PETSc monitors and viewers** — the separation of *producing* a quantity
  from *presenting* it, and `-log_view` for structured summaries.
- **JSON Lines** as a log format, and any of the structured-logging
  conventions around it.
- How **MOOSE** and **Code_Aster** emit per-step scalars for post-processing
  rather than for reading.

**Build.** One diagnostics interface emitting structured records, and a
reader that implements the playbook: given a run, it says which of the eleven
readings apply and what they say. A wall should be able to explain itself.

---

## Rank 6 — Globalization: six mechanisms, one taxonomy.

Six ways of getting an increment to converge accumulated here, one per wall:
the adaptive damage line-search ladder, transactional backtracking, Rescue
levels 1 and 2, a dogleg trust region, and dissipation/crack-control path
following. Measured evidence they do not all discriminate: at one wall,
**three consecutive attempts produced bit-identical residual sequences**.

Survey **PETSc SNES** line searches (`basic`, `bt`, `l2`, `cp`, `nleqerr`),
`SNESNEWTONTR`, pseudo-transient continuation, nonlinear preconditioning
(NASM, NGMRES), and `SNESSetFunctionDomainError`; and **NOX**'s
Direction/LineSearch separation. Map each existing mechanism onto a category.
Verdict expected: which are duplicates, which have no category, and what a
single composable structure would look like.

## Rank 7 — Path following, against the settled formulations.

The tree has dissipation control and crack control, developed here. Compare
against Riks and Crisfield arc length, and the dissipation-based arc-length
method (Gutiérrez 2004; Verhoosel, Remmers & Gutiérrez 2009, aimed
specifically at delamination). Verdict expected: whether what is here is one
of these, a special case, or something with a defect the literature already
solved.

## Rank 8 — Regularisation of softening, and what this model actually is.

The model has viscous damage, a crack-band-scaled dissipation, element
erosion and now a regularised contact branch. Survey crack band (Bažant &
Oh), nonlocal and gradient damage (Pijaudier-Cabot & Bažant; Peerlings),
viscous regularisation (Duvaut–Lions; Gao & Bower), and phase-field fracture
(Bourdin; Miehe; Borden; Ambati) — the last being the modern answer to
"element erosion is a topology jump". Verdict expected: a statement of which
regularisation this model is relying on, whether it is mesh-objective, and
the measurement that would decide.

Note the tree already records a relevant measurement: three uniform bar
meshes agree to 0.94% before the peak and spread to **73–89%** after it. That
is a mesh-objectivity failure, and it is written down but not acted on.

## Rank 9 — Reproducibility as a contract.

Runs are not reproducible across thread counts; a 1e-16 difference decides
which side of a threshold an integration point falls on, and twenty
increments later the runs are on different walls. Survey Intel MKL's
conditional numerical reproducibility, reproducible reductions (ReproBLAS;
Demmel & Nguyen), and — the more interesting direction — making the
*decisions* robust rather than the arithmetic bit-exact: hysteresis on active
sets, and thresholds with a dead band. Verdict expected: a stated
reproducibility contract, and a test that measures sensitivity rather than
asserting it.

## Rank 10 — Verification methodology.

The tree has analytical benchmarks for the cohesive law and the mixed-mode
branch. It has no manufactured solution and no order-of-accuracy study.
Survey the Method of Manufactured Solutions (Roache; Salari & Knupp),
Oberkampf & Roy, and ASME V&V 10/20 for the vocabulary. Verdict expected: a
short statement of what each existing test class actually proves, and whether
one MMS-verified path is worth building.

## Rank 11 — Run provenance and experiment management.

`run_s3rad.sh` writes a text provenance file; claims like "at two threads it
reached increment 930" live in commit messages and in human memory. Survey
Sumatra, DVC, and plain structured run registries. Verdict expected: whether a
machine-readable run registry is worth it, so such a claim becomes a query.

---

## How to report each area

One document per area you touch, in `research/`, with four sections:

1. **The problem as it exists here** — with numbers from this tree, not
   generalities.
2. **What established codes do** — with links and versions.
3. **Adopt / adapt / reject, and why.**
4. **What was built, and what tests it** — or, for a rejection, what would
   change the verdict.

A rejection with a reason is a real deliverable. A survey with no verdict is
not.
