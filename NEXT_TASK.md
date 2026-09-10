# The work queue

`PROMPT.md` is the brief and the discipline. `handover/08-OBJECT-MODEL.md` is
the design — which objects, why, and the idiom. **This file is the order of
work and how you know each item is done.**

Treat the order as argued, not sacred. If you find a better one, say why and
take it — but do items 0 and 1 first regardless, because everything after
them is guesswork without them.

---

## 0. A build and a recorded baseline

You cannot refactor what you cannot re-run.

- `apt` has `intel-mkl` (2020.4.304-4 worked); `./src/build_mkl.sh <dir>`
  produces `ccx_2.23_pardiso` in about a minute.
- **Five of the nine gate cases need PARDISO** — `mkfast.py` writes
  `*Static, Solver=Pardiso`. The other four run on SPOOLES. A four-green run
  is not a passing gate, and the four that would be missing are exactly the
  ones about bulk damage, deletion and load path.
- Run the gate and **commit its output as the baseline** — every scalar, so a
  later "it still passes" means something.

**Done when**: 9/9 green, the binary's sha256 and the baseline numbers are in
the tree.

## 1. Profile the target deck

Nobody has ever done this. Everything in §4 of the object model is a
hypothesis until it is done, and "optimise the algorithms" is not actionable
without it.

- Build the harness on a 24-second deck (`FAST_VARIANT=wrapped`), not on the
  2.3-hour one.
- Attribute time to: assembly, symbolic factorisation, numeric factorisation,
  triangular solves, residual evaluations (and how many of those the
  line-search ladder consumes), the erosion transaction, and diagnostics.
- Then run it once on `s3rad` and record the breakdown.

**Done when**: a committed breakdown of where the runtime goes, at both
scales, with the method reproducible by someone else.

## 2. Comparison with tolerances — time-boxed

The gate can only compare bytes, so any decomposition that reorders a
summation fails it for the wrong reason. This is the one thing that must
exist before the first extraction.

- Per-quantity absolute and relative tolerances; comparison of fields, not
  only status lines; the tolerance used stated in the output.
- Byte identity stays available as the strictest setting.
- **Demonstrate it failing**, the way `test/regress/run.py` was proven.

**Done when**: a case can assert "equal to 1e-10" and that assertion goes red
when it should. **If this grows past a day, stop and reconsider** — it is a
prerequisite, not the project.

## 3. Options

Mechanical, touches everything, breaks nothing — the right first extraction.

- Declare a switch once: type, default, range, one line of documentation,
  optional deprecation. Parse once. Validate. Report.
- `docs/SWITCHES.md` becomes generated from the declarations rather than
  scraped from `getenv` calls.
- Then list the **122 switches no test sets** and start retiring them: a
  switch with no declaration, no test and no prose has no defenders. Retiring
  means making the behaviour the default or deleting it — either is progress,
  leaving it is not.
- PETSc's options database is the model; `-options_left` is what
  `src/damswitch.c` reinvented at a tenth of the scope.

**Done when**: switches are declared rather than scraped, the generated
documentation comes from the declarations, feature-off is bit-identical, and
the retirement list exists with a first batch actually retired.

## 4. Monitor

Pure extraction, no behaviour change — and it removes a standing embarrassment:
**the same census is printed from three separate sites** in `nonlingeo.c`,
because nobody owns it.

- Separate producing a quantity from presenting it.
- Emit structured records, so `handover/02-DIAGNOSTICS.md` can become a
  program rather than eleven readings a human performs by eye.

**Done when**: one owner computes the census, the three sites are gone, output
is machine-readable, and the run is bit-identical.

## 5. Convergence

The first real interface, and the one with a known live problem: the handover
records `AUTOSPC_FORCE` satisfying its own alarm condition for having become a
fiction, with no structure that could express it.

- A composable tree of named tests, each able to say **why** it fired.
  Trilinos NOX `StatusTest` maps onto this almost exactly.
- AUTOSPC and `AUTOSPC_FORCE` become tests in the tree, not exceptions inside
  the judgement.
- Every increment records its reason, in the structured stream from item 4.

**Done when**: the default tree reproduces current behaviour bit-identically,
and "why did this increment converge" is answerable from the record.

## 6. Topology, and the sparsity decision

- Erosion behind an interface: what counts as eroded, the transaction that
  commits it, and rollback.
- Then the measured experiment item 1 will have made worth doing: **hold the
  sparsity pattern fixed under erosion** — dead elements keep their entries,
  zeroed, with a safe diagonal — so the symbolic factorisation happens once
  per run instead of once per topology change. State the hypothesis, name the
  measurement, and check the answer did not move.

**Done when**: erosion has an owner with a self test; and the sparsity
experiment has an answer, whichever way it goes.

## 7. Globalization

Six mechanisms with no interface: the line-search ladder, transactional
backtracking, Rescue levels 1 and 2, a dogleg trust region, and path
following. Measured evidence they do not all discriminate: at one wall,
**three consecutive attempts produced bit-identical residual sequences.**

- Put them behind one interface; map each onto the PETSc/NOX taxonomy.
- Then the question "what is each for" is answerable, and the test for
  keeping one is: **name the failure it addresses and the gate case that
  would go red without it.** Delete what cannot pass it.

**Done when**: one interface, each implementation justified by a case, and at
least one mechanism either justified or gone.

## 8. Direction and Newton

Last, because everything else moves through it. By this point the loop should
be small enough to read.

---

## Standing rules while you work

- **Run the gate before and after every change.** Nine cases, three minutes.
- **Pin `OMP_NUM_THREADS` and `MKL_NUM_THREADS`** on both arms of any
  comparison. Runs are not reproducible across thread counts — measured.
- **Say in which sense each extraction is equivalent** — bit-identical, or to
  a stated tolerance. Both are honest; silence is not.
- **`s3rad` is the last gate, not the first.** It costs 2.3 hours. Note that
  roughly a fifth of that is spent after the specimen has physically broken,
  which the parallel repository is fixing.
- **Record what you reject**, in `handover/04-REFUTED.md`. Every entry there
  is a run somebody else does not have to spend.

## If you get stuck

The two failure modes this project has actually suffered, both worth naming:

**Designing without measuring.** Four separate mechanisms were built on
plausible diagnoses that measurement later rejected. State the measurement
that would reject your design before you build on it.

**Comparing at the wrong point.** Two arms were once compared at the
increment where each happened to stop — both inside a regime where the
specimen no longer existed — and the conclusion came out backwards. Compare
at a point that means something.
