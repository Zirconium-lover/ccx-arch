# 5. The debt, in priority order

## 1. A dead facet is a load path for ever — and it invalidates late results

`cohesive_uc6.f` pins `g = max(gmin, 1-dvisc)` with `gmin` from the deck
(`1e-5` here), and terminal deletion in `nonlingeo.c` scans `C3D4` **only**.
So a fully failed cohesive facet never disappears and never stops carrying
its residual stiffness.

Consequences, measured on `s3rad`:

| | |
|---|---|
| the metal loses grip-to-grip connectivity at | increment **753**, `theta = 0.3411981` |
| the run continues to | increment **931**, `theta = 0.5575` |
| so it runs past separation for | **+63% of grip displacement** |
| what is being solved there | two separated halves joined by **451 facets**, 322 at the residual floor |
| grip reaction at `theta=0.4064` | **0.056%** of peak |

This is the single worst defect in the tree, because it does not produce a
wrong number — it produces a **plausible** one. It manufactured a "third
wall" that consumed an entire investigation and that nobody needed to pass:
that wall is a convergence failure of a configuration that stopped being a
specimen 178 increments earlier.

**It also cost the same investigation its framing twice.** A wall was treated
as the frontier while the severance number sat in the same branch, and two
arms were compared at their stopping increments — both inside the phantom
regime — instead of at severance, where they agree to 0.19%.

Two levels of fix, and they are not alternatives:

- **Root**: let a fully failed facet actually be removed, or stop counting it
  as connectivity. Then the halves become free bodies, the matrix is
  singular, and the run must stop — the model finds out on its own.
- **Guard**: `CCX_FRACTURE_TERMINATION` / `CCX_FRACTURE_DEADFACET` already
  detect loss of load path. On the fast deck DEADFACET stops the run at
  increment 65 instead of walking to 507 — **442 increments after the
  specimen separated**, the same blindness, visible in 58 seconds.

Note the root fix makes `s3rad` substantially cheaper as a side effect, since
a fifth of its runtime is spent after the specimen broke.

## 2. 141 switches, 123 that no test sets — and what that debt was hiding

Generated, not estimated (`docs/SWITCHES.md`). 54 have no declaration, no
test and no prose anywhere but the line that reads them. They include every
rescue, corridor, backtracking and diagnostic knob — the combinatorial space
where nobody can say what any combination does.

A switch that no test sets and no prose explains **has no defenders**.
Retiring one is cheap to justify: make its behaviour the default, or delete
it. Prefer either to leaving it.

### But read the queue before deleting from it — 2026-09-10

The first pass over that generated retirement queue found, among the
orphans, **the operator check that `02-DIAGNOSTICS.md` listed as missing and
that `07-RESEARCH-AGENDA.md` ranked third in the whole project.**
`CCX_STRUCT_FD_INC` / `CCX_STRUCT_FD_ITER` / `CCX_STRUCT_FD_H` run a
column-by-column comparison of the assembled tangent against a finite
difference of the internal force. It is PETSc's `-snes_test_jacobian` in all
but the name, it was already written, and it had been sitting unusable for
want of one line of documentation.

**This changes what the 123 uncovered switches are.** The working assumption
was that an untested, undocumented switch is dead weight, and that the
retirement queue is a deletion list. It is not. It is a list of things
**nobody can find**, and the difference matters:

- a mechanism nobody can justify is a place for the next bug to hide — that
  reading still holds, and it is why `PROMPT.md` says to delete such things;
- a **capability** nobody can find is worse than an absent one, because the
  project pays twice: once for the code, and again for the effort spent
  concluding it does not exist and planning to build it. That is exactly what
  happened here — the agenda's rank 3 entry is a build plan for something the
  tree already had.

So the first action on the queue is to **read** it, and to sort into
*declare* and *retire* rather than assuming one bucket. Of the first 62
entries examined, the operator check (3 names), the addressed node dump (2),
the stiffness census controls (3) and the load-path negation flag (1) are all
capabilities worth keeping, and are now declared. That is nine of sixty-two —
15% — that a deletion-first pass would have thrown away.

The rest of the queue is untouched, and the genuinely deletable part of it
(the overlapping rescue levels, item 3 below) needs the evidence test
`06-TARGET.md` states — *name the failure it addresses and the gate case that
would go red without it* — not a bulk edit.

### The other half of the same debt: the check could not be armed

Worse than undocumented: the probe's configuration was parsed inside the
block gated by `damage_de12_enabled`, so it could only be switched on for a
deck carrying a progressive **bulk** damage material. On
`test/pathfollow/close.inp` — the one deck in the tree that isolates the
crack-face closure kink, the very discontinuity the check is most needed for
— it could not be armed at all. Every one of its switches came back from
`[SWITCHES LEFT]` as *set and never read*, on that report's first real use.

"A responsibility with no home ends up nested inside whatever code happened
to be nearby" (`08-OBJECT-MODEL.md` §1) is usually said about the connectivity
guard. It applies here word for word. Fixed: the parsing is unconditional
now, and the probe runs on any deck.

## 2b. The build does not track header dependencies

`Makefile.ubuntu2404.mkl` had no `-MMD`, so editing `CalculiX.h` or
`ccxopt_decl.h` rebuilt **nothing** and the link silently reused stale
objects. Found the way these things are found: a declaration table edited
three times and compiled none of them, so the binary under test was not the
binary the sources described.

That is not a tidiness problem in a tree whose central discipline is
bit-identity. A stale object can make an extraction look bit-identical
because the extracted code is not in the binary. Fixed with `-MMD -MP` and an
`-include` of the generated `.d` files; a clean rebuild after the fix
produced 195 of them.

## 3. Six overlapping globalization mechanisms

The adaptive damage line-search ladder, transactional backtracking, Rescue
level 1, Rescue level 2 with an event step, a dogleg trust region as level 3,
and dissipation/crack-control path following. Each was added for one wall.
None was removed when a later diagnosis showed the earlier one had been
incomplete.

Evidence they do not all discriminate: at one wall, **three consecutive
attempts produced bit-identical residual sequences** — two rescue levels ran
and did nothing.

## 4. A 14000-line function

`nonlingeo()` carries the solve, the convergence judgement, the erosion and
topology bookkeeping, the load-path judgement, and every diagnostic, with the
diagnostics interleaved among the physics. Three units have been extracted
(`lsladder.c`, `damstate.c`, `damswitch.c`) and the pattern is proven: one
owner, a self test, and the mechanism refuses to arm if its test fails.

The obvious remaining units: erosion/topology, the convergence judgement, the
solve, the diagnostics.

## 5. Reproducibility is only within a fixed thread count

`MKL_CBWR=COMPATIBLE` buys reproducibility across instruction sets, not
across thread counts — a distinction that was being relied on silently. Two
runs identical for 482 attempts diverged at 483 on a 6-against-5 iteration
count and were on different walls twenty increments later.

Partly irreducible: quasi-brittle fracture with erosion is a sequence of
topology jumps, and a 1e-16 difference decides which side of a threshold an
integration point falls on. But nothing in the tree *states* this at run
time, and an A/B across thread counts is silently meaningless.

## 6. Three discontinuities left

The fourth — crack-face closure — was measured and regularised; the method is
in `02-DIAGNOSTICS.md` §1 and the result is that the far field on both sides
is unchanged and only the transition differs. Remaining:

| switch | jump |
|---|---|
| damage initiation, `deff > d0` | onto the softening branch |
| loading/unloading, `deff` vs `dmax` | the rank-1 softening term appears and disappears |
| element deletion, `D > 1-DEADALL` | **the mesh topology changes** |

The third is partly irreducible. The first two are not, and nobody has yet
measured a wall attributable to either — **do not regularise them on faith**.

## 7. Smaller items

- `src/ccx_2.22`, a 6.5 MB ELF executable, is **tracked in git**, inherited
  from the original-sources import. The working brief says binaries stay out.
- Diagnostic probes are compiled into the solve path and gated by
  environment variables rather than isolated behind one interface.
- `CCX_DAMAGE_AUTOSPC` is silently clamped at `1.e-1`. Legitimate as a safety
  rail, invisible as a behaviour — a deck whose worst node sits at `1.04e-01`
  masks nobody and gives no indication why.
