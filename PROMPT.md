# Task: architecture and tooling for the CalculiX fracture branch

This code works and is untrustworthy at the same time. It produces correct
physics, it has passed several hard walls, and it did so by accumulating
point fixes for eight months: 139 environment switches, six overlapping
globalization mechanisms, a 14000-line function, and a validation story that
until recently cost two and a half hours per question.

**Your job is not to find the next bug.** A parallel session is doing that,
well, on another branch. Your job is the thing that has never been done here:
**look outward, find how established codes solve these problems, and build the
instruments this one is missing.**

Read `handover/` for the measured state of the code — it is shared with the
other line of work and it is accurate. Then read
`handover/07-RESEARCH-AGENDA.md`, which is *your* brief.

## The character of this work

Roughly: **survey, decide, build.** Not patch.

For each area in the research agenda:

1. **Survey.** Find out how PETSc, Trilinos/NOX, MOOSE, Code_Aster, deal.II,
   Abaqus, LS-DYNA, Kratos, Akantu, OOFEM and the fracture literature solve
   it. Use web search aggressively; this is explicitly wanted. Cite what you
   find — a link and a version, not a recollection.
2. **Decide.** Adopt, adapt, or reject, with the reason written down. A
   rejection with a reason is a real deliverable; a survey with no verdict is
   not.
3. **Build.** Every adoption lands as a working tool with a test. This is the
   part that must not be skipped.

**The failure mode to avoid is a beautiful literature review and no working
code.** By the end of this work there must be at least **three tools in the
tree that someone else can run**, each with a test that fails when the tool
breaks. If you find yourself on the fifth document without a runnable
artefact, stop and build something.

## Where to start, and why

The agenda ranks the areas. The first is not negotiable, because it blocks
everything else you might want to do:

**The gate can only compare bytes.** `test/regress/run.py` pins outcomes with
scalars and byte-for-byte identity. Byte identity is a wonderful check and a
terrible foundation for refactoring: any restructuring that changes a
summation order — which is most of them — fails it, and there is currently no
way to say *"the answer is the same to 1e-10"*. Until a tolerance-aware field
comparison exists, **no bold refactor can be validated at all**, which is
exactly the constraint that produced the mess you are here to fix.

Build that first. Then the options database, then operator verification, then
convergence as composable objects.

## What you may change, and what to leave alone

Wide latitude on everything you build. This is git; everything is revertible.
Prefer the bold, well-founded change.

Two coexistence rules, because a parallel session is working on the same code:

- **Keep `src/nonlingeo.c` edits minimal and additive.** Your work is
  scaffolding around the physics, not inside it. The other line of work is
  moving judgements out of that file; design so its results land in your
  structures rather than colliding with them.
- **Do not chase defects in the damage module.** If you find one, write it in
  `handover/05-DEBT.md` and move on. Someone else's turn.

## What is not negotiable

The evidence discipline, because it is the only reason anything here is
trustworthy — and it applies to tools as much as to physics:

- **Before a material change, state a falsifiable hypothesis and name the one
  measurement that can reject it.**
- **A tool you cannot demonstrate failing is not a tool.** Break it
  deliberately and show it goes red, the way `test/regress/run.py` was proven.
- **Feature off must be bit-identical**, and you must check it.
- **Pin `OMP_NUM_THREADS` and `MKL_NUM_THREADS` on both arms of any
  comparison.** Runs are not reproducible across thread counts — measured.
- **Report faithfully.** A tool that half works is reported as half working.

## Delivery

Report separately: what you surveyed and where it came from, what you adopted
and rejected and why, what you built, what it is tested by, and what is still
open.

The measure of success is not a green suite. It is whether the next person
can make a bold change to this code and find out in three minutes whether it
was right.
