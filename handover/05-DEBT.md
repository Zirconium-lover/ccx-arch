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

## 2. 139 switches, 122 that no test sets

Generated, not estimated (`docs/SWITCHES.md`). 63 have no prose anywhere but
the line that reads them. The 122 include every rescue, corridor,
backtracking and diagnostic knob — the combinatorial space where nobody can
say what any combination does.

A switch that no test sets and no prose explains **has no defenders**.
Retiring one is now cheap to justify: make its behaviour the default, or
delete it. Prefer either to leaving it.

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
