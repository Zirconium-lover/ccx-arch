# 7. Why the target deck keeps dying

`test/s3rad` is the 2.3-hour deck the whole project is aimed at, and no one
has ever seen it finish.  Three deaths are now on record.  This page is about
the third, which is the first one that was instrumented.

## What happened

Started 2026-09-11 18:40 UTC, shipped configuration (`run_s3rad.sh` as
committed, so `CCX_DAMAGE_TANGENT=UNSYM` among eleven other exports),
`OMP_NUM_THREADS=MKL_NUM_THREADS=4` on a four-core machine, **nothing else
running** - no build, no gate, no second solver.

It stopped at **18:49, increment 102**, having just written

    [DAMAGE COMMIT] batch=8 inc=102 time=1.826875e-01 deleted=3 active_passes=1
    [DAMAGE DE1.3 COMMIT] inc=102 ... terminal_deleted=3 active_passes=1
    [DAMAGE DE1.2 COMMIT] inc=102 ... active=6933 D>0.1=5163 ... Dmax=1.0

and then nothing.  No `*ERROR`, no diagnostic, no exit message, no partial
line - the log ends on a complete newline after a **successful convergence
and a successful commit**.  Every output file carries the same 18:49
timestamp.  That is the signature of a process that was killed, not one that
failed.

## What it was not

**Not memory.**  The cgroup accounting says peak usage 459 MB, `failcnt` 0,
`oom_kill` 0, and 15.5 GB of the machine's 16 GB was free when the corpse was
examined.  There is no OOM record anywhere.

**Not disk.**  23 GB free; the run directory had reached 232 MB - of which
`m.dat` is 128 MB and `m.frd` 108 MB after 102 increments, about 2.3 MB an
increment, which is worth knowing separately but is nowhere near a limit.

**Not contention with another job.**  This is the hypothesis carried into the
run - the two earlier deaths were attributed to a `-j4` build competing for
four cores.  This run had the machine to itself.  **Refuted.**

**Not the arithmetic.**  It died immediately after a converged increment and
a clean transactional commit, not inside a factorisation or a line search.

## What is left, and the test that separates it

What remains is how the run was *launched*.  It was started with
`nohup … &` inside a shell that then exited, so the agent session that
started it owned nothing: the launcher reported success within a second and
the solver ran on unattended.  It stopped at about the time the turn that
launched it ended.

That is one correlation and one data point, so here is the discriminator,
**stated before the re-run rather than after it**:

> The deck is deterministic.  If the death is the deck - an arithmetic
> fault, a corrupt state at a particular batch - a second run reaches
> **increment 102** and dies there again.  If the death is the environment,
> the second run dies at a *different* increment, or does not die.

The re-run (`_runs/prof2`) is launched as a tracked background command
belonging to the session rather than as a detached `nohup`, and is the test.
The corpse of the first is kept at `_runs/prof-full-died-18h49`.

## A second defect, found by the same death

`CCX_LOG_VIEW_EVERY` defaults to **600 seconds**.  The interim profile
tables exist for exactly one reason - two earlier 2.3-hour runs were killed
and produced no profile at all - and this run died at **nine minutes**, one
minute before the first table was due.  It produced **zero** `[LOGVIEW]`
output.

A mechanism built for runs that get killed, whose default guarantees it
produces nothing from a run killed early, is not doing its job.  The default
is wrong, not the mechanism.
