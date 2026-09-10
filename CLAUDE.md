# Working brief for coding agents

**Start with [`PROMPT.md`](PROMPT.md), then
[`handover/07-RESEARCH-AGENDA.md`](handover/07-RESEARCH-AGENDA.md).**
`handover/01`–`06` are the measured state of the code and are shared with a
parallel line of work; they are accurate, read them for context.

This branch is for **architecture and tooling**. Survey how established codes
solve these problems, decide, and build the instruments. Not patching.

## The shape of the work

**Survey → decide → build.** Web research is explicitly wanted here: find how
PETSc, Trilinos/NOX, MOOSE, Code_Aster, deal.II, Abaqus, LS-DYNA, Kratos,
Akantu, OOFEM and the fracture literature handle each problem, cite what you
find with a link and a version, and state a verdict.

**Every adoption lands as a working tool with a test.** The failure mode to
avoid is a beautiful literature review and no working code. At least three
runnable tools by the end, or the brief has not been met. A rejection with a
written reason is a real deliverable; a survey without a verdict is not.

## What you may change

Wide latitude on everything you build. This is git and everything is
revertible, so prefer the bold, well-founded change:

- design new subsystems rather than extending old ones;
- replace a home-grown mechanism with an established one, and say what you
  replaced;
- retire a switch that has no declaration, no test and no prose — it has no
  defenders;
- build new test infrastructure and retire what has stopped earning its
  runtime.

## Two coexistence rules

A parallel session is working on the same code, on `arch/rebuild-base`.

- **Keep `src/nonlingeo.c` edits minimal and additive.** Your work is
  scaffolding around the physics, not inside it.
- **Do not chase defects in the damage module.** Find one, write it in
  `handover/05-DEBT.md`, move on.

## What is not negotiable

The evidence discipline — it applies to tools as much as to physics.

- **Before a material change, state a falsifiable hypothesis and name the one
  measurement that can reject it.**
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

Note what this gate can and cannot do, because rank 1 of the agenda exists
for it: it pins outcomes by scalars and by **byte identity**. It can prove a
change is a no-op. It cannot yet prove a change is *correct to a tolerance* —
so until you build that, any refactor that reorders a summation will fail it
for the wrong reason.

## Delivery

Report separately: what you surveyed and where it came from, what you adopted
and rejected and why, what you built, what tests it, and what is still open.

Success is not a green suite. It is whether the next person can make a bold
change to this code and find out in three minutes whether it was right.
