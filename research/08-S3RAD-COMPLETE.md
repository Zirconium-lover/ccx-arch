# 8. The target deck, run to its own verdict

`test/s3rad` had never been seen to reach an ending.  Two runs were killed,
and a third (`research/07-THE-DECK-DEATHS.md`) was killed nine minutes in.
This is the fourth, and the first that stopped because the **solver** decided
to stop.

    run       test/s3rad/_runs/prof2
    started   2026-09-11 19:13 UTC
    config    run_s3rad.sh as committed, so CCX_DAMAGE_TANGENT=UNSYM and
              eleven other exports; OMP_NUM_THREADS=MKL_NUM_THREADS=4
    ended     rc=201, "increment size smaller than minimum"
    wall      4164 s - 69 minutes, not 2.3 hours
    reached   increment 599, theta = 0.2550244
    deleted   3741 elements

So the deck's own ending is a **wall**, at `theta=0.255`, with 3741 of about
3790 elements gone - 98.7% of the fracture it ever performs.  It is not a
crash and not a termination criterion being met; the cutback ran out of room:
`dtime=1.46e-06` against `tmin=1.00e-06`.

## The complete profile

First full `[LOGVIEW]` report of this deck, 4163 s, 8 events:

| event | calls | self (s) | % run | per call |
|---|---|---|---|---|
| pardiso phase 22 (numeric only) | 5179 | 1898.7 | **45.61%** | 367 ms |
| assembly (`mafillsmmain`) | 5344 | 714.2 | **17.16%** | 134 ms |
| `pardiso factor` self - the **CSR repacking** | 5344 | 391.1 | **9.39%** | 73 ms |
| pardiso solve | 5344 | 386.4 | **9.28%** | 72 ms |
| residual (`results`) | 9438 | 269.3 | 6.47% | 29 ms |
| pardiso phase 12 (analyse+numeric) | 165 | 96.3 | 2.31% | 584 ms |
| pardiso structure hash | 5344 | 4.9 | 0.12% | 0.9 ms |
| pardiso cleanup | 164 | 1.7 | 0.04% | 10 ms |

Instrumented 90.4%.  5344 factorisations, 5344 solves, 9438 residuals - 1.77
residual evaluations per factorisation over the whole run.

**The linear algebra is 64.4% of the run** (45.6 + 9.4 + 9.3 + 2.3 + 0.1) and
**the CSR repacking alone is 6.5 minutes of the 69**.  The repacking is not
the solver: it is the cost of handing CalculiX's `(ad,au,icol,irow)` storage
to PARDISO in the form PARDISO wants, 73 ms every single factorisation.

## The rescue apparatus, on the deck it was built for

`09-STEERING` §5 cites "three consecutive attempts produced bit-identical
residual sequences - two mechanisms ran and did nothing".  Here is that,
counted, over a whole run:

| firing | count |
|---|---|
| `[DAMAGE RESCUE] WALL` events in 69 minutes | **15** |
| of which `reason=too-slow-cutback-below-tmin` | 12 |
| of which `reason=divergence-cutback-below-tmin` | 3 |

and where they landed:

| increment | iter | firings at the identical (inc, iter, time) |
|---|---|---|
| 167 | 5 | 1 |
| 231 | 5, 6 | 1 each |
| **596** | 16 | **3** |
| **597** | 16 | **3** |
| **598** | 16 | **3** |
| **599** | 8 | **3** |

Read it honestly, because it does **not** say the apparatus is useless.

At increments 167 and 231 the rescue fired, the rollback was taken, and the
run **continued** - 368 more increments and 3000 more deletions followed.
Three firings bought the majority of the run.

At 596 through 599 it fired **three times per increment at bit-identically
the same increment, iteration and time**, twelve firings in total, and then
the run stopped anyway.  That is the STEERING observation reproduced at
scale: once the deck reaches its terminal wall, the mechanism runs, restores
the increment-start baseline, and arrives back at exactly the same place.

So the fast-deck leave-one-out (`06-TANGENT-VERDICT`, appendix) and this run
say different things, and both are true: on the fast decks the apparatus is
inert, and here it converts three walls into 368 increments of progress and
then fails twelve times at the last one.  **The case that would go red
without it exists, and it is on this deck.**  What it cannot do is the thing
it keeps being asked to do at the end.

## What is now measurable that was not

- The wall has a name printed by the code itself:
  `reason=too-slow-cutback-below-tmin`, `dtime=1.46e-06`, `tmin=1.00e-06`.
  That is a **deck** statement - the minimum increment - not a solver mystery.
- The deck runs in 69 minutes, not 2.3 hours.  A leave-one-out at scale is
  therefore affordable: six arms is seven hours, not fourteen.

## An options defect this run exposed

The run ended with

    [SWITCHES LEFT] *WARNING: 1 option(s) were set and never read by this run.
    [SWITCHES LEFT]   CCX_LOG_VIEW_EVERY

while producing **34 interim reports at 122 s apart**, which is precisely
what `CCX_LOG_VIEW_EVERY=120` asks for.  The option was in force and was
reported as never read.

`[SWITCHES LEFT]` exists to tell a person that what they set did not reach
the binary - it is the mechanism that caught the probe silently not arming on
`close.inp`.  A false positive in it is worse than no warning, because it
teaches the reader to ignore the true ones.  `src/logview.c` reads this one
through plain `getenv` instead of `ccxopt_getenv`, so the registry never sees
the read.
