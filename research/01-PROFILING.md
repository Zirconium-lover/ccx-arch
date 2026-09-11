# Where the time goes, and the first hypothesis it kills

Area: `07-RESEARCH-AGENDA.md` rank 5 (diagnostics as data) applied to the
precondition `NEXT_TASK.md` item 1 and `08-OBJECT-MODEL.md` §4 both name:
**nobody has profiled this code.**

---

## 1. The problem as it exists here

`08-OBJECT-MODEL.md` §4 lists four optimisation candidates and says outright
that each is a hypothesis until the code is profiled. It never had been. The
target deck takes 2.3 hours, and the guesses on record — assembly,
factorisation, the residual evaluations the line-search ladder consumes, the
census computed at three sites — were guesses.

There was no instrument either. The tree measures physics carefully
(`02-DIAGNOSTICS.md` is eleven readings) and measures its own cost not at
all: one `Total CalculiX Time` at the end of `main`, and nothing under it.

## 2. What established codes do

- **PETSc profiling** — `PetscLogEventRegister` / `PetscLogEventBegin` /
  `PetscLogEventEnd`, with `-log_view` printing a table of registered events
  (calls, time, flops, percentages) and `PetscLogStageRegister` grouping them
  into phases. The idea being borrowed is the one that matters: *the code
  declares its own events*, so the profile is expressed in the vocabulary of
  the solver rather than of the symbol table.
  <https://petsc.org/release/manual/profiling/> (Profiling chapter, release
  documentation, 3.25.x at the time of writing);
  <https://petsc.org/release/manualpages/Profiling/PetscLogEventGetPerfInfo/>.
  Fetched through a search index only — this environment's egress proxy
  blocks `petsc.org`, so these are pointers, not quotations.
- **`gprof`** (in this image, GNU binutils 2.42) — rejected below.
- **`perf`** — not installed in this image, and it would not have helped; see
  below.

## 3. Adopt / adapt / reject

**Adopted**: PETSc's explicit named events, with two additions PETSc's table
already has in substance and this tree needs stated loudly — a **self** time
(the event with its children removed) and a **who-called-whom** row, so
"973 residual evaluations, 606 of them one per Newton iteration" is legible
without arithmetic on the reader's part.

**Rejected: sampling profilers**, for two reasons specific to this code and
not a matter of taste.

- The questions here are about **calls**, not lines. *How many full residual
  evaluations does the ladder consume per Newton step* is a count, and no PC
  histogram answers it.
- The expensive work is inside **MKL**, linked statically from
  `libmkl_*.a` with nothing worth sampling. `gprof` instruments only objects
  compiled with `-pg`, so the factorisation — which turns out to be **47%**
  of the run — would have been invisible or misattributed. That is not a
  small error at the margin; it is the whole answer.

**Rejected: a start-up hook that always measures.** Off by default, because a
profile that is always on is a behaviour nobody can rule out of a comparison.

## 4. What was built, and what tests it

`src/logview.c` (~310 lines) and `tools/profile.py`.

- Named events with call count, **inclusive** and **self** time, and an
  immediate-caller attribution matrix.
- Off unless `CCX_LOG_VIEW` is set. Off means one predictable branch per
  instrumented call and nothing else; no arithmetic on a physical quantity is
  touched in either state.
- Reported from `atexit`, not only from the end of `main` — **the runs worth
  profiling are the ones that wall**, and those leave through the `*ERROR`
  path. The `fast-wrapped` gate case is exactly such a run.
- One `[LOGVIEW_JSON]` line per run, so two arms can be diffed rather than
  read by eye.
- A self test that the report **refuses to print past**.

### It was demonstrated failing, twice

A tool that cannot be shown going red is not a tool.

| break | what the run printed |
|---|---|
| `logview_self[id]+=dt` instead of `dt - children` | `*ERROR: self+child is not inclusive: 7.825299e-03 + 5.263503e-03 != 7.825299e-03`; `*ERROR: the child did not dominate its parent`; **`the self test failed; reporting nothing rather than reporting a timing table that may be wrong`** |
| the `logview_end_named` in `results()` deleted | `*ERROR: 32 event(s) still open at the report`; `the event nesting was violated 9433 time(s); reporting nothing rather than reporting a table that may be wrong` |

Both reverted; the binary profiled below is the restored one.

### Equivalence

**Bit-identical, both with the instrument off and with it on.** Not assumed —
checked, on all nine gate cases (feature off) and on `fast-wrapped` (feature
on): `m.sta`, `m.damage`, `m.dat`, `m.cvg`, `m.12d` and the body of `m.frd`
compare equal byte for byte against the pre-change binary's runs.

The only bytes that differ anywhere are `m.frd` offsets 179–185 — the
`1UTIME` header. **Two runs of the same binary differ there too**, which is
how that was established rather than argued.

---

## The measurement

`OMP_NUM_THREADS=MKL_NUM_THREADS=1`, one case at a time, gcc 13.3.0,
intel-mkl 2020.4.304-4, binary
`1ca894e476afc04a16f32a85e5d385c97916279a9bcefad3728aad5100bb97af`.

Reproduce with

    CCX_EXE=build-mkl/ccx_2.23_pardiso tools/profile.py fast-wrapped fast-plain

### `fast-wrapped` — 24.7 s, walls at increment 99

| event | calls | self (s) | % run | ms/call |
|---|---|---|---|---|
| pardiso factor | 606 | 11.53 | **46.7%** | 19.0 |
| assembly (`mafillsmmain`) | 606 | 7.97 | **32.3%** | 13.1 |
| residual (`results`) | 973 | 2.96 | **12.0%** | 3.0 |
| pardiso solve | 606 | 0.39 | 1.6% | 0.6 |
| not instrumented | | 1.82 | 7.4% | |

### `fast-plain` — 48.1 s, runs to `theta=1`

| event | calls | self (s) | % run | ms/call |
|---|---|---|---|---|
| pardiso factor | 1283 | 22.90 | **47.6%** | 17.9 |
| assembly (`mafillsmmain`) | 1283 | 14.85 | **30.9%** | 11.6 |
| residual (`results`) | 1898 | 5.80 | **12.1%** | 3.1 |
| pardiso solve | 1283 | 0.78 | 1.6% | 0.6 |
| not instrumented | | 3.77 | 7.8% | |

The two 3-D decks agree to within a percentage point on every row, which is
the first reason to believe the numbers.

### Four things that are now facts rather than guesses

1. **Factorise + assemble is 79% of the run.** Everything else competes for
   the remaining fifth.
2. **Assembly costs two thirds of a factorisation** (13.1 ms against 19.0).
   That is far more than a finite element code normally spends assembling,
   and it is the number this project did not have.
3. **`calls(assembly) == calls(factor)` exactly**, in both decks. There is no
   tangent reuse of any kind: every Newton iteration reassembles and
   refactorises. The *Direction/Operator policy* in `08-OBJECT-MODEL.md` §4
   therefore addresses 79% of the runtime, not a corner of it.
4. **The line-search ladder and the rescues cost 4.6%, not a fifth.**
   973 residual evaluations against 606 Newton iterations: 367 extra, 38% of
   all residual evaluations, but residual evaluation is only 12% of the run,
   so the whole globalization apparatus consumes about **4.6%** of it. An
   interpolating line search (Dennis & Schnabel; Nocedal & Wright) could at
   best halve that. **It is not where the time is.** Keep the argument for
   deleting mechanisms — it is an argument about comprehensibility, and a
   good one — but do not make it on speed.

The 7.4–7.8% not instrumented is the increment and Newton bookkeeping, the
erosion transaction and the diagnostics, all still inside `nonlingeo()`. It
is small, and it is the next thing to name.

---

## The hypothesis this refutes

`08-OBJECT-MODEL.md` §4, first candidate:

> **A fixed sparsity pattern under erosion.** Element death changes the
> matrix structure, so the symbolic factorisation is redone. […] Potentially
> a large fraction of the runtime.

**Measured: it is not.** `CCX_PARDISO_REUSE_SYMBOLIC=1` already does exactly
this, keyed on a hash of `(icol,irow)`, and on `fast-wrapped` it retains the
analysis for **603 of 606** factorisations — the pattern is recomputed three
times in the whole run.

| arm | pardiso factor, self (s) |
|---|---|
| stock, run 1 | 11.480 |
| stock, run 2 | 11.528 |
| stock, run 3 | 11.380 |
| `CCX_PARDISO_REUSE_SYMBOLIC=1` | 11.649 |

Three control runs span 1.3%. The reuse arm is at the top of that spread, not
below it — the symbolic phase is smaller than the cost of the structure hash
that decides whether to skip it. And the arms are **bit-identical**
(`m.sta`, `m.damage`, `m.cvg`), so this is a clean cost measurement and not a
trajectory comparison.

This corroborates the note already in `pardiso.c` from another branch — 2.3%
at 84 000 elements — and it scales the right way: the numerical factorisation
grows like `N^1.5` while the analysis is nearly fixed, so at `s3rad`'s 42 807
elements and beyond the symbolic share can only be *smaller*.

**What would change the verdict**: a deck where erosion invalidates the
pattern often enough that the analysis runs on a large fraction of
factorisations. `fast-wrapped` deletes 64 elements and re-analyses 3 times;
`s3rad` deletes 3790 in 455 batches, which is a different regime and the
right place to be sceptical of a fast-deck refutation.

**First data at `s3rad` scale: the churn is nine times higher.** From a run
killed at increment 231 (see *what is still open*), with 1330 deletions
committed in 115 batches:

| | factorisations | fresh symbolic analyses | share |
|---|---|---|---|
| `fast-wrapped`, 64 deletions | 606 | 3 | **0.50%** |
| `s3rad` partial, 1330 deletions | 1600 | 73 | **4.6%** |

So the pattern does churn an order of magnitude more at scale, exactly as
scepticism predicted. What that is *worth* still needs the cost split — 4.6%
of factorisations paying an analysis that is itself some fraction of a
factorisation — and that number needs the profile the killed run never
printed. **`NEXT_TASK.md` item 1 stays open until it does.**

73 re-analyses against 115 committed batches, because a batch that is rolled
back (`[DAMAGE DE1.3 ROLLBACK]`) leaves the pattern where it was.

## What is still open

- **The `s3rad` breakdown at scale.** Two attempts were killed part way
  through — increments 340 and 231, both with the log ending mid-sentence, no
  `*ERROR`, no `Job finished`, no `[LOGVIEW]` table, and no OOM or cgroup
  limit to explain it. Both produced **no profile at all**, because the table
  was printed from `atexit`. That is a defect in the instrument, not bad
  luck, and it is fixed: `CCX_LOG_VIEW_EVERY` (default 600 s) prints an
  `INTERIM` table at the top level between events. A third attempt runs from
  a private copy of the binary so a rebuild cannot disturb it; 2 threads,
  `MKL_CBWR=COMPATIBLE`.
- **Why assembly costs 13 ms** on a deck this small. Nobody has looked
  inside `mafillsmmain`, and it is a third of the run.
- `CCX_PARDISO_CGS` reuses the LU across Newton iterations and nobody has
  measured it either. It attacks the 47%, so it is the next A/B — but it is
  an *inexact* solve, so unlike the reuse above it changes the trajectory and
  has to be compared at a physically meaningful event, not at a stopping
  increment.
