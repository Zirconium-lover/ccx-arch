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

## 2a. A wrapper that clobbers the configuration it is handed

`test/fast/run_fast.sh` delegates to `test/s3rad/run_s3rad.sh`, which
`export`s **twelve** `CCX_*` names unconditionally and only afterwards
applies its positional `NAME=VALUE` overrides. Anything set in the
environment by a caller is therefore silently overwritten.

Two things were broken by this, both for months, both silently:

- **`fast-wrapped-nospc`** — a gate case whose entire stated purpose is to run
  *without* the load-path mask — ran with `CCX_DAMAGE_AUTOSPC=1.e-3`. It was a
  second copy of `fast-wrapped`, passed every time, and proved nothing. One of
  nine cases in the three-minute gate was decoration.
- an **A/B of `CCX_PARDISO_REUSE_SYMBOLIC`** taken in this session had the
  switch on in both arms, and produced a confident published refutation that
  was the exact opposite of the truth (`04-REFUTED.md`).

Fixed on the caller side — `run.py` and `tools/profile.py` pass overrides
positionally, and both now verify against the run's own `[SWITCHES]` banner
that the configuration asked for is the configuration that ran. The wrapper
itself is unchanged and still has the hazard: `export X=1` before processing
`"$@"` means the environment is the weakest input, which is the opposite of
what every caller expects. Worth turning into `: ${X:=1}` per name, which is
a behaviour change and so needs its own measurement.

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

### The retroactive audit, because that sentence demands one — 2026-09-11

Every bit-identity claim made in this session before the fix was made with an
incrementally built binary. Each was re-checked against **one** reference: the
gate artefacts of the pre-everything baseline, built clean in a fresh
directory (`d874dcc…`).

| claim | how it was built then | artefacts vs the clean baseline |
|---|---|---|
| logview | incremental | identical |
| comparison layer | incremental | identical |
| ccxopt / Options | clean at the time | identical |
| census + monitor | incremental | identical |
| opcheck | clean at the `-MMD` fix | identical |
| **logview, re-built CLEAN now** | clean | **identical** |
| **census, re-built CLEAN now** | clean | **identical** |
| HEAD | clean | identical except `fast-wrapped-nospc/m.cvg`, which is the intended effect of §2a's fix |

**Every claim holds.** The clean rebuild of the census commit reproduces, byte
for byte, what the incremental binary produced at the time; the clean rebuild
of the logview commit reproduces the pre-logview baseline.

Two facts found while doing it, both worth keeping:

- **This build is reproducible.** The same sources built in place and in a
  separate worktree give a byte-identical executable. That makes `sha256` of
  the binary a usable identity for a source state.
- **But it did not match at one point in history.** A clean build of the
  logview commit gives `d3df6015…`, while the binary used to make that
  commit's claim was `1ca894e4…`. The build directory is long gone, so the
  cause is not recoverable, and it is not a small difference to wave at:
  *something* in that binary was not what its sources describe. The claim
  survives only because the behaviour was re-verified, not because the
  bytes agreed.

The lesson for the next session is the narrow one: **binary identity is not
a substitute for re-running the gate off a clean build, and an incremental
build's sha means nothing.** The `-MMD` fix removes the mechanism; the audit
is what closes the claims made before it.

## 2c. The damage tangent is missing the term that makes it consistent

Measured 2026-09-11, `research/03-OPERATOR.md`. `mafilldamas.f` states the
consistent tangent in its own header:

    C = g(D)*C_ep - sigma_eff (x) dD/d(eps)

The rank-1 correction is `dσ/dD · dD/dε`. Three facts, in order of how much
they cost:

1. **`mafilldamas` runs only under `CCX_DAMAGE_TANGENT=UNSYM`.** The stock
   symmetric path and `FD_SYM` have no such term at all.
2. **The assembled operator is not the differential of the residual anywhere
   in the damaging phase**, in every tangent mode: 151, 157 and 161 wrong
   coefficients of 20604 for stock, `UNSYM` and `FD_SYM`, on a residual
   verified locally smooth.
3. **Corrected 2026-09-11 — it is not the missing rank-1 term.**
   `research/05-TANGENT-POPULATIONS.md` cross-checked the counters against the
   elements the probe reports wrong, per element rather than per count:
   - every *defect* population — terminal hole, advancing-below-terminal,
     degenerate — is **empty** on this deck at every increment probed;
   - the element measuring wrong is the one element that **has** the term;
   - turning the term off with `CCX_DAMAGE_UNSYM_SCALE=0` changes the worst
     coefficient by 10%.

   What the discrepancy is instead: **two** errors. A plasticity floor of
   about `1e-3` relative, which survives **removing the damage cards from the
   deck** (identical to four significant figures, same element, same
   increment), and a damage contribution about **ten times larger** after
   initiation. The next suspect for the second is the `damageg*stiff` scaling
   in `resultsmech.f` rather than the assembly in `mafilldamas.f`.

What this costs is in `02-DIAGNOSTICS.md` §1: not the converged answer, but
quadratic convergence and the guarantee that the Newton direction descends —
which makes every globalization mechanism look worse than it is and
contaminates the ladder census in the process zone. Six globalization
mechanisms accumulated in exactly that regime.

That reading has been done. `mafilldamas` now records a per-element category
instead of four counts, two of its populations had no name before, and
`CCX_STRUCT_FD_ELEM` aims the probe at a chosen one. The classification and
its verdicts are in `research/05-TANGENT-POPULATIONS.md` §1.

`CCX_DAMAGE_TANGENT_CENSUS`, the switch that prints the per-iteration
version, was in the generated retirement queue.

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
