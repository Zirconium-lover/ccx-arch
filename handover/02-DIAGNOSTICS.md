# 2. What to measure, and how to read it

This is the document to reach for when a run fails. Everything in it was
assembled from what actually resolved a wall; the readings are not
theoretical.

The general shape of the procedure is:

1. **Is the run still physical?** (§6, §7) — if the specimen broke 200
   increments ago, stop and fix that instead.
2. **Is it a kink or a stiffness loss?** (§1) — this single reading splits
   the two big classes and takes one armed run.
3. **Where does it live?** (§2, §3) — which node, and does it have a load
   path.
4. **Is the fix real or a fiction?** (§4) — the number that tells you a fix
   has quietly stopped working.

---

## 1. The line-search ladder census — `CCX_DAMAGE_WALL_THETA=<theta>`

**The most valuable single diagnostic in the tree.** Arms at a load factor so
the probes cost nothing for the first N hundred increments. It walks a ladder
of step fractions `eps` along the Newton direction and prints, for each:

```
pass 2 eps=0.003906250  |res|2/|r0|2=437.4   active-set transitions 18
pass 2 eps=0.000061035  |res|2/|r0|2=  6.67  active-set transitions 19
```

**Read the two columns together:**

| ratio as `eps` → 0 | transitions as `eps` → 0 | what it is |
|---|---|---|
| → 0 | → 0 | the linearisation is right; a step-size or globalization problem |
| **linear in `eps`, never below 1** | **does not decay** | **a kink** — the iterate sits on a switching surface and any step crosses it |
| erratic | → 0 | suspect the operator: assembly, a tangent that is not the differential of the residual |

A transition count that stays flat at, say, 19 down to `eps=6.1e-5` is not
"nearly converged", it is "there is no step, however small, that does not
change the model". Newton cannot converge on a kink it cannot step off.

**The sub-counts name WHICH kink**: `UC6 loading/unloading`, `UC6
initiation`, `UC6 viscous`, `UC6 failure`, `UC6 tension/compression`, `bulk
plastic`, `bulk initiation`, `bulk damage growth`. In the case that was
solved, `tension/compression` was 7 and `loading/unloading` 12 — and fixing
the first took both to zero, because the iterate stopped bouncing.

Also printed at each armed iteration: where the residual, the Newton step and
the linear-model defect *live*, split by whether a node touches a live UC6
facet, softening bulk or plastified bulk. Useful for ruling out a region.

## 2. Residual peak attribution — `Rpeak`, same switch

Per node at the residual peak: `R`, `addiag`, `addiag0`, their `ratio`,
`spc_masked`, and `need_du = |R|/k`.

| reading | meaning |
|---|---|
| `ratio` 0.75–1.0, `need_du` 1e-4…1e-6 | a **healthy** node needing a microscopic displacement it never gets. Not a stiffness problem — look for a kink |
| `ratio < 1e-3` | the node has no load path; this is the mask class |
| `need_du` of order the **grip displacement** | the node cannot be equilibrated by any physically meaningful displacement. This is the dimensionally sound criterion and it needs no tuned threshold |

`need_du` against a length scale is the criterion to prefer over any
stiffness ratio. Tuning a ratio until a particular node is included is the
chain of thresholds this project exists to avoid — at one wall there were 76
nodes below 1e-3, 166 below 1e-2 and 381 below 1e-1, and picking among them
proves nothing.

## 3. The stiffness census — printed whenever AUTOSPC is armed

```
[DAMAGE STIFFNESS] inc=818 below_1e-3=85 below_1e-2=170 below_1e-1=374
                   nonpositive=0 worst=node_495_at_3.6664e-07
```

Counts of nodes whose assembled diagonal has fallen below a fraction of
**their own intact value**, so it is dimensionless and needs no mesh-wide
median. `worst` is the extreme.

Worth knowing: `3.7e-07 ≈ gmin × (Kn·A / K_bulk)`. A node at that ratio has
lost **all** its bulk and is held by **failed** facets. A node at ~0.04 has
lost its bulk but its facets are still intact. A node at 0.1–0.3 still has
live elements. The number tells you the topology.

**`CCX_DAMAGE_AUTOSPC` is clamped at `1.e-1` in the source.** A deck whose
worst node never goes below that cannot exercise the predicate at any legal
setting — which is exactly what made the first fast deck useless for
validating it.

## 4. The exclusion report — `CCX_DAMAGE_AUTOSPC_FORCE=1`

Prints the excluded residual next to the criterion it was excluded from.

**This is the number that tells you the fix has become a fiction.** At the
wall it fixed, the excluded residual peaked at `0.0086 × qam` — 1.7x the
tolerance — on one node for eight increments, then returned to machine zero
once the fragment resolved. **If it stops returning to zero, the mechanism is
hiding a real imbalance and must be revisited.**

## 5. The convergence record — `<job>.cvg`

`RESID` and `CORR` per iteration, per attempt.

| reading | meaning |
|---|---|
| two attempts with **identical** residual sequences | the second attempt did nothing — a rescue level that is not discriminating |
| residual flat while the correction shrinks | stalling: either a kink (§1) or an irreducible component |
| residual halves when `dtime` halves | a **step-proportional** residual Newton cannot remove — cutting back will not help and may hurt |

## 6. Connectivity — `test/s3rad/fragments.py <rundir>`

Connected components of the live bulk, with a cohesive facet counted as a
load path only above a given stiffness fraction. Answers two questions the
per-node diagonal cannot:

- **Is anything adrift?** A component reaching no grip has six rigid-body
  modes with no stiffness, and every node in it looks healthy to a diagonal
  test. This is the diagonal test's structural blind spot.
- **Is the specimen in two pieces?** Sweep the threshold and see where it
  falls apart. On `s3rad` it is two pieces, one at each grip, as soon as only
  facets above `g=0.5` count.

## 7. Severance — bisect the deletion history

For the physical question "when did it break", bisect `m.damage` in time
order for loss of grip-to-grip connectivity through **live bulk only**. On
`s3rad`: increment 753, `theta = 0.3411981`.

**Run this before interpreting any late-run result.** A wall 178 increments
after separation is a wall in a configuration that is not a specimen, and no
amount of convergence work on it means anything.

Corroborate with the reaction from `m.dat` (`total force`): 2676.81 at
`theta=0.1955`, 27.32 at 0.2609 (**1.0%**), 1.51 at 0.4064 (**0.056%**).
Below a per cent of peak, the answer is bookkeeping.

## 8. The deletion history — `m.damage`

`element, step, increment, step_time, total_time, material, damage,
critical_ip, batch`. Cross-reference the element ids against the elsets to
see which phase erodes and where the crack actually goes. On `s3rad`: 3007 ZR
(10.8% of the matrix) and 699 ZRH (7.4% of the hydride).

Two runs' deletion **sets** compared element by element is a far stronger
statement than their counts. The gate does this.

## 9. The configuration report — `[SWITCHES]`, top of every log

Every switch in force, and every `CCX_*` name set that this binary does **not**
read. A misspelt switch produces an A/B that is not wrong but
**uninformative**, and that is the expensive kind — it costs a whole run to
notice. Check this block before believing any comparison.

## 10. The self tests — `DAMAGE TR`, `DAMSTATE`, `LSLADDER`

Run on every job that arms the relevant mechanism, and the mechanism
**refuses to arm** if its test fails. Prefer this to a test that runs
elsewhere: a judgement that is not the one that was tested should not be
allowed to decide anything.

## 11. Constitutive laws — `test/pathfollow/check_close.py`, `check_mixed.py`

Verify a law point by point against its closed form, from printed
separations, damage and tractions, with no bulk model or structural branch in
the way. A failure there is a failure of the constitutive routine and nothing
else. Tolerance `1e-5` is the `.dat` file's seven significant digits, not the
identity being approximate.

---

## What is missing from this list

- ~~Nothing measures the **operator** directly.~~ **Corrected 2026-09-10.**
  It does, and it always did: `CCX_STRUCT_FD_INC` / `CCX_STRUCT_FD_ITER` /
  `CCX_STRUCT_FD_H` run a column-by-column central-difference check of the
  assembled tangent against the internal force, at the most damaged element.
  No test set them and nothing documented them, so they sat in the generated
  retirement queue. Now declared, and run: **elastic control 5.4e-08 relative
  error with 0 bad coefficients; process zone 1e-03 to 9.6e-02 with 8-20 bad
  coefficients per column and sign errors**, flat over `h` from 1e-11 to
  1e-8. See `research/03-OPERATOR.md`. A tool nobody can find is not a tool.
- Nothing reports **why an attempt was abandoned** in a machine-readable
  form. `m.cvg` has to be read by eye.
- The connectivity check (§6) is offline python. It should be a census the
  solver prints, cheaply, every N increments — then §7 would be automatic and
  the phantom-regime defect could not recur.
