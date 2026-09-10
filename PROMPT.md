# Task: rebuild the architecture of the CalculiX fracture branch

The goal is **architecture**: an object model, clean interfaces, and
algorithms chosen and measured rather than accreted.

This code works and is unmaintainable at the same time. `nonlingeo()` is a
single function body of **~14381 lines** carrying the increment loop, the
Newton loop, assembly, the solve, the convergence judgement, six
globalization mechanisms, erosion and topology bookkeeping, path following,
and every diagnostic — interleaved. Around it sit **139 environment
switches**, 63 documented nowhere and 122 set by no test. Nothing here was
designed; it was added, one wall at a time, for eight months.

**Decomposing that is the job.** Not patching it, not building tools beside
it, and not hunting the next bug — a parallel session in another repository is
doing that, well.

Read `handover/08-OBJECT-MODEL.md` first: it is the concrete target — which
objects, what each owns, the idiom to implement them in, and the migration
strategy that has already worked twice in this tree.
`handover/07-RESEARCH-AGENDA.md` is the survey material that should inform
those decisions. `handover/01`–`06` are the measured state of the code.

## The shape of the work

**Design, survey, then build — incrementally.**

1. **Design on paper first.** Name the objects, their responsibilities, their
   interfaces, their lifecycles, and who owns what state. Write it down and
   argue it before writing code. A decomposition nobody can state in one page
   is a decomposition that will not survive contact.
2. **Survey before inventing.** Nearly every mechanism here was invented on
   the spot, and every one of them has a settled solution in PETSc, Trilinos,
   MOOSE, Code_Aster or deal.II. PETSc in particular is the reference
   implementation of object orientation *in C* — opaque handles, ops tables,
   `XXXSetType`, registration, options prefixes — which is exactly the idiom
   this code needs. Use web search; cite with a link and a version.
3. **Migrate by strangling, not by rewriting.** Extract one responsibility at
   a time, leave the call at the original site, prove bit-identity, repeat.
   This tree has done it twice already (`src/lsladder.c`, `src/damstate.c`)
   and both times it worked.

## Algorithms are part of this, and start with a measurement

"Optimise the algorithms" has a precondition nobody has met: **nobody has
profiled this code.** The target deck takes 2.3 hours and no one knows where
it goes — assembly, factorisation, residual evaluations in the line-search
ladder, or the census recomputed at three separate sites.

Measure that first. Then the candidates in `08-OBJECT-MODEL.md` §4 stop being
speculation: a sparsity pattern held fixed under erosion so the symbolic
factorisation is done once instead of per topology change; an interpolating
line search instead of a fixed ladder of full residual evaluations; a tangent
reuse policy. Each is an *architecture* decision as much as a numerical one,
which is why they belong here.

## One enabling step before the first extraction

The gate can only compare **bytes**. It pins cases by scalars and
byte-for-byte identity, so it can prove a change is a no-op and cannot prove a
change is *correct to a tolerance*. The moment a decomposition reorders a
summation — which most do — it fails for the wrong reason.

So build a tolerance-aware comparison first. **Time-box it.** It is a
prerequisite, not the project; if it grows past a day's work you have lost the
thread. `handover/07-RESEARCH-AGENDA.md` rank 1 says where to look.

## Latitude

Wide, deliberately. This is git and everything is revertible, so prefer the
bold, well-founded change:

- **`src/nonlingeo.c` is yours to take apart.** That is the assignment. An
  earlier draft of this brief told you to keep edits there minimal, which
  would have made the goal impossible; ignore any such instruction if you find
  it elsewhere in the tree.
- replace a home-grown mechanism with an established one, and say what you
  replaced;
- delete a mechanism nobody can justify — the test for keeping one is *name
  the failure it addresses and the gate case that would go red without it*;
- retire a switch with no test and no prose;
- change a default when the evidence supports it, and say so.

A parallel session works from `ccx-crack-prop-arch`. Separate repository,
separate history, deliberate merge later. Do not contort your design to avoid
it — but do read `handover/05-DEBT.md` and `06-TARGET.md`, and design so that
its `loadpath.c` (the "is this still a specimen" owner) drops into your object
model rather than fighting it. If you find a defect in the damage module,
write it in `05-DEBT.md` and move on; it is not your turn.

## What is not negotiable

The evidence discipline. It is the only reason anything here is trustworthy,
and it applies to a refactor more strictly than to a bug fix, because a
refactor is supposed to change nothing.

- **Every extraction is bit-identical, or it is not an extraction.** If it
  changes behaviour, it is a change: state a falsifiable hypothesis and name
  the one measurement that can reject it.
- **A tool you cannot demonstrate failing is not a tool.** Break it and show
  it goes red.
- **Pin `OMP_NUM_THREADS` and `MKL_NUM_THREADS` on both arms of any
  comparison.** Runs are not reproducible across thread counts — measured.
- **Compare arms at a physically meaningful event, not at whichever increment
  the solver gave up on.** Breaking this rule once here inverted a conclusion.
- **Report faithfully.** A half-built object is reported as half built.

## Delivery

Report separately: the design and its rationale, what you surveyed and where
it came from, what you extracted and what proves it bit-identical, what you
measured and optimised, and what is still open.

Success is not a green suite and not a document. It is that the next person
can find where a thing is decided, change it, and find out in three minutes
whether they were right.
