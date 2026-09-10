# Comparison with tolerances: the prerequisite, time-boxed

Area: `07-RESEARCH-AGENDA.md` **rank 1**, `NEXT_TASK.md` item 2. It is the one
thing that had to exist before the first extraction, and it is explicitly not
the project — this is one working day's worth of it, deliberately.

---

## 1. The problem as it exists here

`test/regress/run.py` pins a case two ways: by scalars (increment, `theta`,
deletion count) and by **byte-for-byte identity** of `m.sta`, `m.damage` and
`mixed.sta`.

Byte identity is a strong check and a terrible foundation for refactoring.
It can prove a change is a no-op. It cannot express *"the answer is the same
to 1e-10"* — so the moment a decomposition reorders a summation, which most
do, the gate goes red for the wrong reason and the person doing the work has
no way to tell a correct extraction from a broken one.

That is not a hypothetical. `08-OBJECT-MODEL.md` §5 requires every extraction
to prove bit-identity *or* equivalence to a stated tolerance, and until now
only the first of those could be stated at all.

## 2. What established codes do

- **MOOSE `CSVDiff` / `Exodiff` testers** — per-test `rel_err` (default
  5.5e-6) and `abs_zero` (default 1e-10), with `override_columns`,
  `override_rel_err` and `override_abs_zero` for per-quantity tolerances, and
  a gold directory holding the reference.
  <https://mooseframework.inl.gov/python/testers/CSVDiff_tester.html>,
  <https://mooseframework.inl.gov/python/testers/Exodiff.html>.
- **Code_Aster `TEST_RESU`** — a reference *value* rather than a reference
  *file*, with `PRECISION` and `CRITERE='RELATIF'|'ABSOLU'` chosen per
  assertion.
- **`exodiff`** (SEACAS) — field-by-field, per-timestep tolerance semantics.
- **deal.II** — numdiff-based comparison.

Fetched through a search index; the environment's egress proxy blocks direct
retrieval of several of these, so treat the numbers above as cited from the
indexed page text rather than read in full.

## 3. Adopt / adapt / reject

**Adopted from MOOSE**: the pair of tolerances, the per-quantity override,
and the default that a comparison states which tolerance it used.

**Adopted from Code_Aster**: that the *criterion* is a choice, and that the
choice belongs in the test rather than in the tool.

**Adapted, and this is a deliberate divergence.** MOOSE's documentation says
both tolerances must be met. Here a pair `(a,b)` agrees when

    |a-b| <= atol   OR   |a-b| <= rtol * max(|a|,|b|)

`atol` is the escape for quantities whose true value is machine zero — of
which this tree is full, because `fn` components on masked degrees of freedom
and separations on intact facets are exactly that. Under an AND rule every
such quantity fails on relative error forever. The OR rule is what numdiff
and Code_Aster's `ABSOLU` criterion give, and it is the right one here.

**Added, and not found in any of them**: two rules that come straight from
this tree's own history.

- **Labels are never compared with a tolerance.** Node and element numbers,
  step and increment counters, material ids, batch numbers — exact or fail.
  A tolerance on an identity is a way of not noticing that two runs did
  different things, and `04-REFUTED.md` records what that costs.
- **The tool states the file's own resolution.** `m.sta` prints seven
  significant digits. Asserting agreement to `1e-10` on it is asking a
  question the bytes cannot answer, and getting `equal` back would be exactly
  the "not wrong but uninformative" failure that `damswitch.c` exists to
  prevent one level up. `ccxdiff` measures the digits actually present and
  says so:

      m.sta   equal to rtol=1e-10 atol=1e-12  (393 numbers; worst rel 0)
              [this file carries about 1e-05 relative precision,
               so rtol=1e-10 is below what its digits can resolve]

  That note is a finding in itself: **`m.cvg` carries only about three
  significant digits**, so the convergence record cannot support a tight
  comparison at all in its present format.

**Rejected: replacing byte identity.** It stays, as `--exact`, and it stays
the strictest setting. The self test proves it is strictly stronger: a
trailing-space change is invisible to the tolerant comparison and red under
`--exact`.

## 4. What was built, and what tests it

`tools/ccxdiff.py`, plus an `equal_to` clause in `test/regress/cases.json`
and the wiring in `run.py`.

Readers for every artefact a run produces, so the comparison is of **fields**
and not only of status lines:

| file | what is compared |
|---|---|
| `m.sta` | the accepted increment trajectory, keyed on (step, increment, attempt) |
| `m.damage` | the deletion history: the element **set** element by element, and the deletion times numerically |
| `m.cvg` | residual and correction per Newton iteration, per attempt |
| `m.dat` | whatever the deck printed, keyed on (section heading incl. its time, node) |
| `m.frd` | every nodal block (`DISP`, `STRESS`, `TOSTRAIN`, …) per node and per component |

`m.frd` is 19 392 numbers on `fast-wrapped` against the 393 in `m.sta`. The
field data was never compared by anything before.

### It was demonstrated failing

`tools/ccxdiff.py --selftest` — eleven checks, run as a **gate preflight**
so no case can be decided by a comparison that has not just certified itself:

```
  ok   identical runs agree                                           green
  ok   a 1e-8 relative change is caught at rtol=1e-10                 red
  ok   the same change passes at rtol=1e-6                            green
  ok   a 1e-8 change m.sta cannot represent is reported as agreement   green
  ok   1e-13 against zero passes at atol=1e-12                        green
  ok   the same passes nothing at atol=1e-14                          red
  ok   a changed material id is caught at any tolerance               red
  ok   a missing deletion record is caught                            red
  ok   a substituted element id is caught                             red
  ok   trailing whitespace is invisible to the tolerant comparison    green
  ok   trailing whitespace IS visible to --exact                      red
```

The dial is turned in **both** directions on the same perturbation, which is
the check that matters: a comparison that only ever goes red is as useless as
one that only ever goes green.

### And it was demonstrated failing in the gate, not only in isolation

`close-blend` was temporarily made to assert equality with `close-sharp` at
`rtol=1e-10`. The gate went red and named the physics:

```
FAIL not equal to close-sharp: close.sta DIFFERS; close.dat DIFFERS
FAIL    (step,inc,att) (2, 19, 1): label ITRS differs exactly: 2 vs 5
FAIL    (section,node) ('displacements ... time 0.1095000E+01', '10'):
          c0 -0.00025714519999999998 vs -0.0002058424  (abs 5.13e-05, rel 0.2)
FAIL    ... and 17 more differing value(s)
```

That is the regularised contact branch penetrating less than the sharp law
inside the blend band — the intended difference, found by the tool, localised
to the node and the time. The assertion was then reverted.

### What the gate asserts now

Two relations that were previously either scalar-only or `m.sta`-only are now
stated over the whole field data at `rtol=atol=0` — every printed digit:

- `fast-plain-smooth` ≡ `fast-plain` over `m.sta`, `m.damage`, `m.cvg`,
  `m.dat`, `m.frd`;
- `mixed-analytic-smooth` ≡ `mixed-analytic` over `mixed.sta`, `mixed.cvg`,
  `mixed.dat`.

Both hold. 9 of 9 green.

## What is still open

- **`m.cvg` prints three significant digits.** The convergence record is the
  one artefact a decomposition of the Newton loop will need to compare
  tightly, and in its present format it cannot be. That is a format problem,
  and it belongs with the Monitor object (item 4), not here.
- No comparison of the `[LOGVIEW]` profile between arms yet — the JSON line
  exists for it, nothing consumes it.
- `--selftest` builds its fixtures in a temp directory; it does not exercise
  the `m.frd` reader, because manufacturing a plausible `frd` by hand is more
  fixture than the check is worth. The reader is exercised by the real gate
  comparisons instead.
