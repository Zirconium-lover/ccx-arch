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

## The hypothesis this was supposed to refute, and the retraction

`08-OBJECT-MODEL.md` §4, first candidate:

> **A fixed sparsity pattern under erosion.** Element death changes the
> matrix structure, so the symbolic factorisation is redone. […] Potentially
> a large fraction of the runtime.

**The first version of this document said it buys nothing. That was wrong,
and it was wrong for the most instructive reason available here.**

The A/B was run as "stock, against `CCX_PARDISO_REUSE_SYMBOLIC=1`". But
`test/fast/run_fast.sh` delegates to `test/s3rad/run_s3rad.sh`, which
`export`s that switch unconditionally. **Both arms had it on.** The 1%
difference measured was noise between two runs of the same configuration, and
the `[SWITCHES]` banner said so in both logs — `02-DIAGNOSTICS.md` §9 exists
to catch exactly this and depends on a human remembering to look.

What found it was not a human. Splitting the profiler's factorisation event
by the PARDISO **phase** actually executed made the "control" arm report 606
phase-22 calls, which is only possible with the analysis being reused.

### The measurement, with a control that is one

Three interleaved repeats per arm, one thread, `fast-wrapped`. Absolute times
are inflated because the target deck was running on two other cores; both
arms shared that load and were interleaved, so the ratio is the reading.

| arm | pardiso factor, inclusive (s) | whole run (s) |
|---|---|---|
| reuse **off** | 16.938 / 17.202 / 17.021 | 30.59 / 31.20 / 31.04 |
| reuse **on** | 11.655 / 11.974 / 12.547 | 24.79 / 25.51 / 26.57 |
| | **17.05 → 12.06, −29%** | **30.94 → 25.62, −17%** |

Spread within an arm is 2–7%; the gap is 29%. And the phase split gives the
analysis cost directly, without a second experiment:

| phase | calls | mean |
|---|---|---|
| 12, analyse + numeric | 606 (off) / 3 (on) | **25.2 ms** |
| 22, numeric against a retained analysis | 603 (on) | **16.5 ms** |

**The symbolic analysis costs 8.7 ms — 35% of a full factorisation** at this
size. The two arms are **bit-identical** (`m.sta`, `m.damage`, `m.cvg`,
`m.dat`), so this is a pure cost measurement.

### So what is left of the candidate

Split it in two, because the original wording conflates them.

- **"Stop re-analysing when the pattern has not changed."** Already built,
  already on in the gate and in `run_s3rad.sh`, and worth **17% of runtime**.
  That is the large fraction the candidate predicted. It was being collected
  the whole time, which is why it was invisible.
- **"Hold the pattern fixed under erosion so the analysis happens *once*."**
  This is the part still open, and it is small. With reuse on, 3 of 606
  factorisations re-analyse on `fast-wrapped` and **73 of 1600** on `s3rad`.
  At 35% of a factorisation each that is at most **1.6% of factorisation
  time, about 1% of runtime** — and it is an upper bound, because the numeric
  phase grows like `N^1.5` while the analysis does not, so the 35% share
  shrinks at `s3rad` size. The running profile will give the real share.

### What the reuse mechanism charges, and where the 6% actually goes

`pardiso_factor`'s **self** time — everything it does outside the solver call
— is 6.1–6.3% of the `s3rad` run, about 80 ms on every factorisation. Two
things live there: the structure hash that decides whether the analysis can
be skipped, and the fill of MKL's CSR arrays from CalculiX's own `(ad, au,
icol, irow)` storage. The first is the price of the reuse mechanism; the
second is a storage-format mismatch.

Instrumented separately, 300 s into a target-deck run:

| | calls | total (s) | % run | ms/call |
|---|---|---|---|---|
| `pardiso factor` self | 235 | 18.47 | **6.15%** | 78.6 |
| of which **structure hash** | 235 | 0.22 | **0.07%** | **0.94** |
| of which **CSR fill** (by difference) | 235 | 18.25 | **6.08%** | **77.7** |

**The hash is free and the suspicion about it was wrong.** It costs 0.94 ms
to decide whether to skip a 202 ms analysis, and it is what makes phase 22
possible at all. A topology-change counter in its place would recover 0.07%
of the run: nothing.

**The 6% is the format conversion.** `aupardiso[k]=au[l]` — a
permutation-copy of the whole matrix into CSR — runs on every factorisation
because the values change every time, even when the pattern does not. That is
not a solver cost and not a physics cost; it is the price of assembling into
one storage convention and factorising in another, and on the target deck it
is of the order of **eight minutes of the 2.3 hours**.

It is a *representation* decision, which `08-OBJECT-MODEL.md` §3 puts with
the **Operator** object. Two ways to take it, neither attempted here:
assemble directly into CSR, or precompute the permutation once per pattern
(it is already known to be stable for ~70 factorisations at a time) and make
the fill a straight gather. The second is much the smaller change and the
cache already exists to hang it on.

For scale, against the candidate it displaces: the remaining headroom in
"hold the pattern fixed under erosion" is **0.8–1% of runtime** (4.6% of
factorisations re-analysing, at 202 ms against a 774 ms factorisation). The
conversion is six to eight times bigger and nobody had noticed it, because
until the factorisation event was split by phase it was hidden inside a
single 838 ms/call row.

### At `s3rad` scale

From the interim tables of the target-deck run (2 threads,
`MKL_CBWR=COMPATIBLE`, 601 s in, 472 factorisations):

| event | calls | self (s) | % run | ms/call |
|---|---|---|---|---|
| pardiso factor | 472 | 395.7 | **65.8%** | 838 |
| assembly (`mafillsmmain`) | 472 | 104.0 | **17.3%** | 220 |
| residual (`results`) | 765 | 42.6 | **7.1%** | 55.7 |
| pardiso solve | 471 | 13.7 | 2.3% | 29.1 |
| not instrumented | | | 7.1% | |

Against the fast deck (47% / 32% / 12% / 1.6%), the factorisation takes over
as the problem grows, exactly as `N^1.5` against `O(N)` predicts. Two
consequences:

- **assembly falls from a third of the run to a sixth.** Still worth
  understanding at 220 ms a call, but no longer the second-biggest thing.
- **globalization is 2.2% of `s3rad`, not 4.6%.** 765 residual evaluations
  against 472 Newton iterations is 1.62 per iteration, the same ratio as the
  fast deck, but residual evaluation is now only 7.1% of the run.

## The cost split at scale: `s3rad`, finally measured

Item 1 of `NEXT_TASK.md` has been open since the beginning because nobody had
a profile of the deck the project is aimed at - two 2.3-hour runs were killed
and produced nothing.  This is that measurement, from the run at
`test/s3rad/_runs/prof2`: shipped configuration (so `CCX_DAMAGE_TANGENT=UNSYM`
among eleven other exports from `run_s3rad.sh`), `OMP_NUM_THREADS =
MKL_NUM_THREADS = 4`, nothing else on the machine, interim tables every 120 s.

Self time as a share of wall clock, one row per interim table:

| at | phase 22 | assembly | **CSR fill** | solve | residual | phase 12 | analyses / factorisations |
|---|---|---|---|---|---|---|---|
| 120 s | 49.26% | 17.68% | **9.75%** | 3.38% | 8.38% | 0.70% | 1 / 152 |
| 721 s | 47.26% | 17.55% | **9.33%** | 3.54% | 7.59% | 1.93% | 23 / 913 |
| 1323 s | 45.10% | 17.38% | **9.26%** | 5.07% | 7.11% | 3.37% | 76 / 1659 |
| 1924 s | 45.03% | 17.06% | **9.18%** | 6.82% | 6.76% | 3.12% | 102 / 2397 |
| 2525 s | 45.19% | 17.05% | **9.22%** | 7.84% | 6.45% | 2.97% | 128 / 3165 |
| 3006 s | 45.34% | 16.98% | **9.21%** | 8.36% | 6.45% | 2.91% | 149 / 3767 |

At 3006 s the run is at increment 477, `theta=0.2261`, with **3361 elements
deleted** of the roughly 3790 the deck ends with.

Instrumented total 87-88%; the remaining 12% is everything not named by an
event - deck I/O, the damage bookkeeping, the `.frd` and `.dat` writing.

**Three of the six shares are flat to a tenth of a percent across fifty
minutes**: assembly 17.7 -> 17.0, CSR fill 9.75 -> 9.21, structure hash 0.12
throughout.  That is what makes them decidable now rather than after the run.

**Two move, and they move against each other.**  Numeric factorisation falls
49.3 -> 45.3 while the triangular solve rises 3.4 -> 8.4 - and per call the
factorisation is FLAT the whole time.  That is one event, not two, and it has
its own section below.

**One rises on its own**: symbolic analysis, 0.70% -> 2.91%, from 1 re-analysis
in 152 factorisations to 149 in 3767.  The sparsity pattern is being rebuilt
four times as often as at the start, which is erosion doing exactly what was
predicted of it - and at 4% of factorisations it is still cheaper than not
reusing at all.

Three things to take from it.

**Numeric factorisation is half the run, and that is the asymmetric one.**
48% in phase 22 at 388 ms a call.  `research/06-TANGENT-VERDICT.md` measures
what buys that on the fast deck: `CCX_DAMAGE_TANGENT=UNSYM` costs +37% of
wall time for 0.2% fewer Newton iterations and a byte-identical fracture.
Here it is 48% of a 2.3-hour run being paid for the same thing, and the
asymmetric factorisation's share grows with `N` faster than the symmetric
one's.

**The CSR repacking is 9.4% and is not going away on its own.**  74 ms every
factorisation, 606 factorisations in eight minutes - about **13 minutes of a
2.3-hour run** spent rewriting CalculiX's `(ad,au,icol,irow)` into PARDISO's
row-by-row form.  On the fast deck it was 6.1%; it is larger here because
`UNSYM` makes the fill 54 times bigger.  It is `pardiso factor`'s self time -
work done around the solver call, not by it - which is why it was invisible
until the call tree existed.  Note that the per-call cost *falls* slightly as
the run goes on (77.1 -> 74.3 ms) while the number of calls climbs: this is
not a leak, it is a fixed cost per factorisation.

**Symbolic reuse is working.**  Ten analyses in 606 factorisations, 1.3% of
the run.  The mechanism retracted and then re-measured earlier on this page
is carrying its weight at scale: without it those 606 calls would each pay
the 632 ms phase 12 rather than the 388 ms phase 22.

### The triangular solve steps 3x, and the factorisation does not

The interim tables are every 120 s, so differencing consecutive ones gives
the *instantaneous* cost per call rather than a cumulative average.  Doing
that turns a gentle-looking drift into a step:

| window | solve ms/call | phase 22 ms/call | assembly ms/call |
|---|---|---|---|
| 120-962 s | 27.3 - 29.2 | 369 - 395 | 137 - 145 |
| **962-1082 s** | **48.9** | 361.6 | 136.2 |
| **1082-1203 s** | **85.7** | 355.7 | 134.3 |
| 1203-2525 s | 85.6 - 88.2 | 350 - 394 | 130 - 139 |

The back substitution goes from ~28 ms to ~87 ms across two windows and then
sits there.  **The numeric factorisation that produced those factors does not
move at all** - 375 ms a call before, during and after.  Neither does
assembly.  The residual drifts *down* (42.7 -> 32.3 ms).

What it is not:

- **not the system size.**  `neq` falls from 29501 to 29414 over the whole
  run, 0.3%.
- **not extra solves.**  Solves per factorisation is exactly 1.00 in every
  one of the twenty windows, and residual evaluations per factorisation is
  1.6 throughout.  Nothing in the globalization is firing more often.
- **not fill-in.**  Growing fill would make the factorisation more expensive
  too, and it is flat.
- **not the CGS path.**  `CCX_PARDISO_CGS` is unset in this run.
- **not a damage-state threshold, in any obvious reading.**  `D>0.9` peaks at
  285 elements at t=1082 and falls back to ~180; `Dfull` peaks at 140 and
  falls to ~35.  Nothing steps 3x.

What it does coincide with: the window `t = 962-1203 s` is increments 195-222,
and increment 222 is where the count of live damage-bearing elements turns
over for the first time - 7882, 7837, then 7692 and falling - i.e. the onset
of sustained element removal.  By increment 407 the run has deleted 2998
elements.

**The hypothesis, and the measurement that decides it.**  A triangular solve
that gets three times more expensive while its own factorisation does not is
doing more passes, and the obvious candidate is PARDISO's **iterative
refinement**: `iparm(8)` bounds the refinement steps and PARDISO performs them
only when the computed solution is poor, which is exactly what a system full
of elements pinned at the residual-stiffness floor `damggmin=1e-4` would
produce.  `resultsmech.f` says this in prose already - "a notch process zone
holds thousands of them at once and the operator becomes badly scaled: that is
the regime where the DHC runs stall" - and this would be the first time it has
a number.

PARDISO reports the refinement steps it actually performed in **`iparm(7)`**,
which `src/pardiso.c` currently never reads.  Reading it and reporting it per
solve settles the question in one line; if it is refinement, then the cost of
the floor is measurable and `damggmin` becomes a knob with a price on it
rather than a comment.  **Stated before the measurement.**

#### Confirmed

`iparm(7)` is now read and reported.  Both arms:

| deck | solves | refinement steps | mean | max |
|---|---|---|---|---|
| `fast-plain` (control) | 1283 | **0** | 0.000 | 0 |
| `s3rad`, solves 1-1300 | 1300 | **0** | 0.000 | 0 |
| `s3rad`, solves 1300-1400 | 100 | 174 | **1.74** | 2 |
| `s3rad`, solves 1400-1500 | 100 | 200 | **2.00** | 2 |

Zero for thirteen hundred consecutive solves, and then two steps a solve -
PARDISO's maximum - from there on.  A refinement step is another forward and
back substitution, so two of them turn one triangular solve into three.  The
measured step was **28 ms to 87 ms, a factor of 3.1**.

The control arm was stated first and came back zero, the prediction named the
size and the arrival, and both landed.  The triangular solve did not get
slower; it started being performed three times.

What it buys: `damggmin`, the residual-stiffness floor, has a price now.  Its
own comment in `resultsmech.f` says a notch process zone holds thousands of
floored elements at once and "the operator becomes badly scaled: that is the
regime where the DHC runs stall".  That is no longer prose.

#### The whole-run number

The instrumented arm (`_runs/refine`) ran to the same ending as the
uninstrumented one - increment 599, `theta=0.2550244` - in 4142 s against
4164 s, and its **deletion record is byte-identical to `prof2`'s**, all 3741
of them.  That is the equivalence proof for the `iparm(7)` instrumentation at
scale, not just on the fast decks.

    [PARDISO REFINE] solves=5344 steps=8062 mean=1.509 max=2

**8062 refinement steps over 5344 solves.**  A refinement step is another
forward and back substitution plus a residual, so the triangular solve does
`5344 + 8062 = 13406` substitutions where it would otherwise do 5344 - **2.5
times the work**.  The solve is 382.3 s of the run, so the refinement part of
it is about `382.3 * 8062/13406 =` **230 seconds, 5.6% of a 69-minute run**,
spent entirely on the conditioning of an operator held together by elements
pinned at `g = 1e-4`.

The mean of 1.509 over the whole run, against a locked 2.00 after solve 1300,
is just the zero-refinement first quarter averaging in.

Whether that 5.6% is worth paying is now a question with two measurable
sides, which it was not before: the floor's *cost* is this number, and the
floor's *value* - how much worse the conditioning would be without it - has
still never been measured.  `CCX_DAMAGE_GMIN` is settable, and the arm that
would answer it is one run.

## The CSR repacking, removed

`pardiso.c`'s `mtype=1` branch - structurally symmetric, numerically
asymmetric, which is what `CCX_DAMAGE_TANGENT=UNSYM` makes every run of this
branch take - rebuilt its whole CSR on every factorisation: four allocations,
a full sort of the lower triangle by row over `nzs` entries, a sort of every
row by column, and an interleave into `neq + 2*nzs` slots.

The permutation depends only on the sparsity pattern.  The hash that detects
a pattern change already ran on every call.  `CCX_PARDISO_REPACK` caches the
permutation and refills only the values.

Same deck, same machine, four threads, shipped configuration:

| | `pardiso factor` self | per call | share of run | wall clock |
|---|---|---|---|---|
| before (`_runs/prof2`) | 391.1 s | 73.7 ms | **9.4%** | 4170 s |
| after (`_runs/repack`) | **33.3 s** | **6.2 ms** | **0.93%** | **3588 s** |

**The repacking is 11.7 times cheaper and the run is 582 seconds shorter -
fourteen percent of a sixty-nine minute deck.**

`[PARDISO REPACK] rebuilt 165 time(s), refilled from the cached permutation
5179 time(s)`, and 165 is exactly the number of phase-12 analyses in the same
run.  The two mechanisms agree on when the pattern changed without being told
about each other, which is the cross-check that matters here.

**Acceptance was bit identity and it holds**: the deletion record is
byte-identical to `prof2`'s, all 3741 of them, and the run ends at the same
increment 599, the same `theta=0.2550244`.

One honest caveat about the 582 s.  The repacking event itself accounts for
358 s of it.  The other events also came out 3-5% faster - phase 22 1804.8 s
against 1898.7, assembly 675.6 against 714.2, solve 356.4 against 386.4 -
which is consistent with removing ~50 MB of allocation and a large sort from
every factorisation, but it is a single pair of runs and run-to-run variation
is not excluded.  The 358 s is measured directly; the rest is a reasonable
attribution, not a measurement.

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
