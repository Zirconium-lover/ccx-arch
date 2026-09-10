# Working brief for coding agents

**Start with [`PROMPT.md`](PROMPT.md), then
[`handover/08-OBJECT-MODEL.md`](handover/08-OBJECT-MODEL.md).**
`handover/07` is survey material; `handover/01`–`06` are the measured state of
the code and are shared with a parallel line of work.

This repository is for **architecture**: an object model, clean interfaces,
and algorithms chosen and measured rather than accreted. `nonlingeo()` is a
single function body of ~14381 lines — **decomposing it is the job**.

## The shape of the work

**Design, survey, then migrate incrementally.**

Design the object model on paper and argue it before writing code. Survey
before inventing — PETSc is the reference implementation of object
orientation *in C* (opaque handles, ops tables, `SetType`, registration,
options prefixes) and is the idiom this code needs; Trilinos NOX
`StatusTest` maps onto the convergence problem almost exactly. Cite with a
link and a version.

Then prefer **strangling to rewriting**: extract one responsibility at a
time, leave the call at the original site, prove equivalence, repeat. This
tree has done it twice (`src/lsladder.c`, `src/damstate.c`), so it is the
default — but a clean-sheet subsystem is allowed where you can argue the old
one is not worth carrying.

**Optimising algorithms starts with a measurement.** Nobody has profiled
this code; the target deck takes 2.3 hours and no one knows where it goes.
Profile first, then decide.

## What you may change

Wide latitude. This is git and everything is revertible, so prefer the bold,
well-founded change:

- **`src/nonlingeo.c` is yours to take apart** — that is the assignment;
- replace a home-grown mechanism with an established one, and say what you
  replaced;
- delete a mechanism nobody can justify — the test for keeping one is *name
  the failure it addresses and the gate case that would go red without it*;
- retire a switch with no test and no prose;
- change a default when the evidence supports it, and say so.

## The parallel line of work

A separate session works from the `ccx-crack-prop-arch` repository —
different history, deliberate merge later. It is building `loadpath.c`, the
owner of "is this still a specimen"; worth knowing, but design for what you
judge right rather than for what you guess it will produce.

Do not go hunting defects in the damage module. If one blocks a
decomposition, fix it and record it in `handover/05-DEBT.md`.

## What is not negotiable

The evidence discipline — it applies to tools as much as to physics.

- **An extraction must be provably equivalent, and you must say in which
  sense** — bit-identical for pure code movement, equivalent to a stated
  tolerance where arithmetic is necessarily reordered. What is not allowed is
  a behaviour change reported as a refactor; that needs a falsifiable
  hypothesis and the measurement that can reject it.
- **A tool you cannot demonstrate failing is not a tool.** Break it
  deliberately and show it goes red.
- **Feature off must be bit-identical**, and you must check it.
- **Pin `OMP_NUM_THREADS` and `MKL_NUM_THREADS` on both arms of any
  comparison.** Runs are not reproducible across thread counts — measured.
- **Compare arms at a physically meaningful event, not at whichever increment
  the solver gave up on.** Breaking this rule once here inverted a conclusion.
- **Keep binaries and generated solver output out of git.**

## Before and after every change

    CCX_EXE=/path/to/ccx_2.23_pardiso test/regress/run.py -j 2

Nine cases, about three minutes, exit status is the number of failures.
Regenerate the switch registry with `tools/mkswitches.py` if you add or
remove a `getenv`; the gate's preflight fails if it is stale.

Note what this gate can and cannot do: it pins outcomes by scalars and by
**byte identity**. It can prove a change is a no-op. It cannot yet prove a
change is *correct to a tolerance* — so any refactor that reorders a
summation fails it for the wrong reason. Building tolerance-aware comparison
is the one prerequisite before the first extraction (`07`, rank 1).
**Time-box it**: it is a prerequisite, not the project.

## Delivery

Report separately: what you surveyed and where it came from, what you adopted
and rejected and why, what you built, what tests it, and what is still open.

Success is not a green suite and not a design document. It is that a reader
can find where a thing is decided — ask "what counts as converged here" and
arrive at one file with a self test, instead of five places and an
environment variable.
