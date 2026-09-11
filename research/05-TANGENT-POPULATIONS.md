# Which population is the defect — and it is not the one we were sent for

`handover/09-STEERING.md` item 1. The brief says the root cause is located,
names the rank-1 term `−σ_eff ⊗ dD/dε` in `mafilldamas.f`, and says **do not
re-derive it**. The cross-check it asks for was run. It does not support that
attribution, and this document is that result.

---

## 1. What was built to answer it

`mafilldamas` counted four numbers (`nskip`, `nadv`, `nhole`, `nfloor`) and
could not say **which element** was in which. Two of its populations had no
name at all. It now writes a per-element category:

| cat | meaning | verdict |
|---|---|---|
| 1 | **ASSEMBLED** — the rank-1 term is in the operator | — |
| 2 | not past initiation; `damjac` empty | **legitimate** |
| 3 | past initiation, damage **not advancing**; `dD/dε = 0` | **legitimate** |
| 4 | advancing, on the residual-stiffness floor; `dg/dD = 0` | **legitimate** |
| 5 | advancing, past the terminal threshold, `resultsmech` refuses the fill | **defect** — the hole the source already named |
| 6 | advancing **below** the terminal threshold, still no term | **defect** — *newly named*; was inside `nadv`, mixed with the points that advanced earlier in the increment and unload now |
| 7 | `damjac` filled and the element dropped anyway, for a degenerate Jacobian | **defect** — *newly named*; counted nowhere, because the skip test is upstream of `xsj < 1e-20` |

`CCX_STRUCT_FD_ELEM` was added so the probe can be aimed at a chosen element
instead of the most damaged one — without that, the cross-check is a
coincidence.

## 2. The cross-check, and the first surprise

`fast-wrapped`, increment 50, iteration 2, `CCX_DAMAGE_TANGENT=UNSYM`:

```
[OPCHECK] rank-1 term by population: assembled=1 pre-initiation=2099
          not-advancing=1 on-floor=0 terminal-hole=0 advancing-live=0
          degenerate=0
[OPCHECK]   category 1 elements: 1238
[OPCHECK]   bulk element 1238 dam=1.829354 population=1 (ASSEMBLED)
[OPCHECK]   157 WRONG of 20604
```

**Every defect population is empty, and the element measuring wrong is the one
element that HAS the term.** The same holds at every increment probed: `hole`,
`advancing-live` and `degenerate` are zero throughout.

So the literal answer to item 1 is: **no counter accounts for the WRONG
coefficients.** All three defect populations are legitimate-by-absence on this
deck; the three legitimate ones are legitimate by construction, with the
reasons the source already gives.

## 3. Where the discrepancy actually comes from

### It starts with plasticity, before any damage exists

Probing the most damaged bulk element at iteration 2, with the full
population census:

| increment | probed `dam` | assembled | any defect population | WRONG of 20604 |
|---|---|---|---|---|
| 3 | 0.000000 | 0 | 0 | **0** |
| 5 | 0.000000 | 0 | 0 | **0** |
| 8 | 0.000000 | 0 | 0 | **0** |
| 10 | **0.118410** | **0** | 0 | **345** |
| 15 | 1.838125 | 7 | 0 | 413 |
| 20 | 2.000000 | 14 | 0 | 278 |

At increment 10 the worst element is at `dam = 0.118` — **below initiation**,
so no element anywhere carries a rank-1 term — and 345 coefficients are
already wrong. The discrepancy switches on with the first plastic strain, not
with damage.

### Proved by removing the damage module from the deck

The generated deck with its three `*Damage Initiation` cards stripped —
same mesh, same plasticity, no damage model at all — probed on the **same
element at the same increment**:

| | `|ctr−asm|` node 276 dir 1 | WRONG |
|---|---|---|
| full deck | 2.291e-03 / 3.452e-03 / 3.688e-03 | 391 |
| **damage cards removed** | **2.291e-03 / 3.452e-03 / 3.688e-03** | **391** |

Identical to four significant figures. At that increment the damage module
contributes **nothing**.

### But after initiation it does dominate — by an order of magnitude

Same element 1238, same column, both decks:

| increment | with damage | without damage | ratio |
|---|---|---|---|
| 10 (`dam=0.011`, pre-initiation) | 2.291e-03 | 2.291e-03 | 1.0 |
| 20 | 1.420e-02 | 9.409e-04 | **15×** |
| 35 | 1.159e-02 | 1.179e-03 | **10×** |
| 50 | 1.347e-02 | 1.284e-03 | **10×** |

So there are **two** errors, not one:

- a **plasticity floor** of about `1e-3` relative, from first yield onward,
  present with the damage model physically absent;
- a **damage contribution about ten times larger** once damage initiates.

### And the damage contribution is not the missing rank-1 term

Three independent reasons:

1. The element carrying it is `ASSEMBLED` — it *has* the term.
2. Every defect population is zero.
3. `CCX_DAMAGE_UNSYM_SCALE`, the knob the source added "for measuring rather
   than guessing", moves it by about 1%:

   | scale | `|ctr−asm|` | WRONG |
   |---|---|---|
   | 0 (term off) | 1.218e-02 | 151 |
   | 1 (as shipped) | 1.347e-02 | 157 |
   | 5 | 9.349e-03 | 166 |
   | 10 | **7.635e-03** | 215 |
   | 30 | 4.917e-02 | 304 |
   | 100 | 8.401e-02 | 224 |

   There is a minimum near 10 — the term is roughly **an order of magnitude
   too small** — but even there it reaches only `7.6e-03` against the
   plasticity floor of `1.3e-03`, and the *number* of wrong coefficients
   **rises** from 151 to 215. A uniform scale improves the worst coefficient
   and damages others, which says the term's **structure** is off, not merely
   its magnitude.

   **Caveat, stated rather than buried**: changing the scale changes the
   iteration path, so these rows are not the same state. Treat the column as
   indicative of a trend, not as a fit. Rows 0–10 reach increment 50 by
   similar paths; 30 and 100 almost certainly do not.

## 4. What this means for the brief

The brief's premise — *"the root cause, already located, do not re-derive
it"* — is **half right, and the half that is wrong matters**.

- Right: the damage tangent is the dominant term after initiation, ten times
  the plasticity floor.
- Wrong: it is **not** the missing rank-1 term. The term is present on the
  element that measures wrong, every hole population is empty, and turning the
  term off changes the answer by 10%.

So item 2, "close the hole", has no hole to close on this deck. What it should
become:

1. **The plasticity floor (~1e-3).** Present with the damage module removed —
   so it is CalculiX's own tangent under `*PLASTIC` as this path uses it, and
   it is not this project's to fix without a decision. Small, but it is the
   floor everything else is measured against.
2. **The damage contribution (~1.2e-2).** Not the rank-1 hole. The next
   suspect is the `damageg*stiff` scaling in `resultsmech.f`: the residual
   carries `g(D)·σ`, whose exact differential is
   `g·dσ/dε + σ ⊗ dg/dD · dD/dε` — and the second piece is exactly what the
   rank-1 term is supposed to be, which brings the scale sweep's factor of ten
   back into view as a question about `damageq` (`dD/dε`) rather than about
   the assembly.

Both are one probe run each to test, and the probe now names the element.

## 5. Equivalence

`mafilldamas` gained two counters and a per-element category array; the
assembled matrix is untouched. **Bit-identical on every artefact of all nine
gate cases** against the pre-change binary. 9 of 9 green.

The stripped deck is **derived, not committed**: it is the generated
`fast.inp` with the three `*Damage Initiation` blocks removed, produced by a
six-line filter, and it exists to answer one question. Its provenance is this
paragraph.
