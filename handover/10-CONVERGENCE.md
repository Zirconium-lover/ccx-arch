# 10. Convergence: the design, and the first extraction

*Written 2026-09-13, before the code.  `08-OBJECT-MODEL.md` §5 puts
Convergence third in the migration order, after Options and Monitor, and
calls it "the first real interface".  This is that interface, argued.*

## 1. Where the judgement lives today

Ask this tree "what counts as converged here" and you arrive at five places.

| site | what it decides |
|---|---|
| `calcresidual.c`, `resultsforc` | `qa[]`, `cam[]` — the load and correction measures |
| `nonlingeo.c` ~12509–12601 | `ram[]`, `ram1[]`, `ram2[]`, `uam[]`, `qam[]` — **the numbers the judgement is made from**, including the AUTOSPC-FORCE mask and the `qam` floor |
| `checkconvergence.c` 155–200 | the boolean itself, as one unnamed nested expression |
| `checkconvergence.c` 545–600 | divergence, cutback, and the increment control |
| `nonlingeo.c`, scattered | a slow-Newton extension (`NC1`), rescue arming, the dissipation-path acceptance |

Two structural facts follow, and both are decomposition arguments rather
than bugs:

- **The mask is inside the reduction loop.**  `CCX_DAMAGE_AUTOSPC_FORCE`
  excludes dofs from `ram[0]` by a `continue` in the middle of the loop that
  computes it.  There is no object whose answer that is, so it lives where
  the loop happened to be.  This is why the mask could be measured for two
  days (`13`–`15`) before anyone noticed it changed nothing: the thing it
  modifies has no name.
- **No clause of the criterion has a name.**  The mechanical test is eight
  conditions in one expression.  When a run stops, "which clause failed" is
  not a question the code can answer — and at the s3rad wall that is exactly
  the question (`16-TRAP-ANATOMY.md`).

## 2. Survey: what the settled solutions look like

**Trilinos NOX `StatusTest`** — read from the headers, not a description.
[`NOX_StatusTest_Generic.H`](https://raw.githubusercontent.com/trilinos/Trilinos/master/packages/nox/src/NOX_StatusTest_Generic.H),
[`NOX_StatusTest_Combo.H`](https://raw.githubusercontent.com/trilinos/Trilinos/master/packages/nox/src/NOX_StatusTest_Combo.H)
(Trilinos `master`, fetched 2026-09-13).

```cpp
enum StatusType { Unevaluated = -2, Unconverged = 0, Converged = 1, Failed = -1 };

class Generic {
  virtual StatusType checkStatus(const Solver::Generic&, CheckType) = 0;
  virtual StatusType getStatus() const = 0;
  virtual std::ostream& print(std::ostream&, int indent = 0) const = 0;
};

enum ComboType { AND, OR };
```

Three things are worth taking:

1. **Every test is the same shape** — evaluate, remember, print yourself.
   `print()` is part of the interface, not a debugging afterthought: a test
   that cannot say what it is is not a test.
2. **A test has four states, not two.**  `Unevaluated` is distinct from
   `Unconverged`.  This tree conflates them constantly — a criterion that
   was skipped and a criterion that failed both read as "not converged".
3. **Composition is explicit.**  `Combo(AND, a, b)` is Unconverged if any
   component is Unconverged; otherwise it returns the status of *the first
   test that is Converged or Failed*.  `Combo(OR, …)` is Unconverged only if
   all are, "making it sensible to place failure-type tests at the end".
   The eight-clause expression in `checkconvergence.c` is exactly a `Combo`
   tree written out by hand, with the names thrown away.

**PETSc `SNESConvergedReason`** — read from
[`include/petscsnes.h`](https://raw.githubusercontent.com/petsc/petsc/main/include/petscsnes.h)
(PETSc `main`, fetched 2026-09-13).  Twenty-one enumerators; positive means
converged, negative means diverged, `SNES_CONVERGED_ITERATING = 0` means
still going:

```c
SNES_CONVERGED_FNORM_ABS = 2,       /* ||F|| < atol                        */
SNES_CONVERGED_FNORM_RELATIVE = 3,  /* ||F|| < rtol*||F_initial||          */
SNES_CONVERGED_SNORM_RELATIVE = 4,  /* || delta x || < stol || x ||        */
...
SNES_DIVERGED_LINE_SEARCH = -6,     /* the line search failed              */
SNES_DIVERGED_LOCAL_MIN = -8,       /* converged to a local minimum of F   */
SNES_DIVERGED_DTOL = -9,            /* || F || > divtol*||F_initial||      */
```

The lesson is the *granularity*.  PETSc does not report "it failed"; it
reports which criterion produced the verdict.  This tree reports `rc=201`,
"increment size smaller than minimum", for at least three physically
different situations: a residual that diverges under a deletion shock
(`17-DDELETE.md`), a residual that creeps while a node hangs on one element
at the stiffness floor (`16`), and a specimen that has genuinely separated
(`09-SEVERANCE.md`).  One number for three states is the whole problem.

**What NOT to take.**  The full PETSc apparatus — registration,
`SetFromOptions`, options prefixes, runtime type selection — buys pluggable
implementations this code does not need: there is one convergence test and
there will be one.  `08-OBJECT-MODEL.md` §2 says the same: "the opaque
handle, the ops table, the uniform lifecycle and the self test are probably
the right subset".  I take less than that for Convergence: a plain struct,
named tests, a reason enum, and a self test.  An ops table with one
implementation in it is ceremony.

## 3. The object

**Convergence owns three questions, and it is being built in that order.**

| | question | status |
|---|---|---|
| **A. Norms** | *what are the numbers the judgement is made from?* | **built** — `converge_norms()` |
| **B. Verdict** | *given those numbers, is this converged, and by which clause?* | designed below, not built |
| **C. Reason** | *when the run stops, why?* | designed below, not built |

### A. Norms — `converge_norms()`

The reduction of the solution vector into `ram[]`, the `ram1`/`ram2`
history, the `uam` high-water mark, the `qam` running average and its floor,
the AUTOSPC-FORCE exclusion, and the `1e-6` cut-off.  One function, one
call site, and the exclusion is now a named property of the object rather
than a `continue` inside somebody else's loop:

```c
typedef struct{
  double qam_floor;   ITG mask_force;   const ITG *mask;  ITG mask_nk;
  double qam_peak;
  double excl_max;    ITG excl_node;    ITG excl_count;   /* what it excluded */
}converge;
```

`excl_*` is deliberately part of the object's state and not a local: the
declaration of `CCX_DAMAGE_AUTOSPC_FORCE` promises that the excluded
residual is reported next to the criterion it was excluded from, and a
promise like that belongs to the thing that does the excluding.

### B. Verdict — BUILT, 2026-09-13

The mechanical criterion is eight clauses.  Written as a NOX-shaped tree
they are:

```
AND( IterationsAtLeast(2),
     ForceResidual(ram[0] <= c1*qam[0]),
     ContactSetStable(iflagact == 0),
     CreepTolerance(nmethod != -1 || qa[3] <= cetol),
     OR( SolutionChange(cam[0] <= c2*uam[0]),
         AND( OR( Projected(ram[0]*cam[0] < c2*uam[0]*ram2[0]),
                  ResidualWellBelow(ram[0] <= ral*qam[0]),
                  LoadIncrementSmall(qa[0] <= ea*qam[0])),
              NoGasNetwork(ntg == 0)),
         CorrectionFloor(cam[0] < 1e-8)))
```

Each leaf gets a name, a value, its threshold, and a status.  The tree is
evaluated in the same order and short-circuits the same way, so the verdict
is bit-identical; what is added is that `getStatus()` per leaf makes "which
clause is holding this increment back" answerable for the first time.

`checkconvergence.c` is shared with `electromagnetics.c`, which was the
reason to hold this back; it was done anyway because the shared file is
exactly where the duplication lives, and leaving the tree alone would have
meant leaving the three copies alone too.  Seventy-five lines of `&&` and
`||` are now one call.

**One deliberate deviation from the sketch above.**  The sketch
short-circuited like the original, so a leaf past the first failure was
never evaluated and could not be reported — and a table with holes in it
does not answer the question the object exists for.  Every leaf is a
comparison of two doubles with no side effect, so evaluating all of them
cannot change the combination.  All ten are evaluated and recorded; the
boolean is assembled from the same expression in the same shape.

**What naming them found.**  The three forms are not the same criterion:
`iflagact` gates the mechanical form and not the thermomechanical one, and
the `ral` leaf carries an extra `iit>1` in the pure-thermal form only.
Neither is visible in the expression as written and neither is touched
here; both are pinned by the self test and recorded in `05-DEBT.md` §8.

**Evidence.**  Gate 16 of 16 with every output file byte-identical at
rtol=0 atol=0; stdout byte-identical across all sixteen cases outside the
self-test block; target deck byte-identical on all five files.
`CCX_CONVERGE_EXPLAIN` on is byte-identical to off on a controlled pair.
Twenty-five self-test checks, and the test was shown to go red on the
plausible tidy-up — removing 8b's `iit>1` to make the two thermal forms
agree.

### C. Reason — designed, not built

A `converge_reason` enum on the PETSc pattern, positive converged, negative
diverged, zero iterating.  The value of it here is specific and measured:
`rc=201` currently covers at least a deletion shock, a stalled node at the
stiffness floor, and a separated specimen, and those want three different
answers from the person reading the log.

## 4. What is NOT in this object

- **Increment control.**  Cutback, `dtheta`, attempt counting — that is the
  Increment object, and `checkconvergence.c` owns it today.  Convergence
  answers a question; it does not decide what to do about the answer.
- **The load-path judgement.**  `damstate.c` owns "has this node lost its
  load path".  Convergence consumes it as a mask.  Today it consumes the raw
  `damage_spc_mask` pointer, which is literally `damage_dstate.dead`; making
  that call `damstate_dead()` is a one-line change held back deliberately so
  that the extraction is provably pure movement and nothing else.
- **Printing.**  Monitor's job.  The eleven `printf` calls around the norm
  block are left exactly where they were; moving them is a separate step
  with its own proof.

## 5. Evidence for step A

Pure code movement, so the standard is **bit-identity**, and it is checked
rather than assumed, on both the gate and the deck the change can affect:

- gate 16 of 16, and all 74 output files byte-identical to the binary before
  the extraction, at rtol=0 atol=0, `OMP=MKL=2` on both arms;
- the target deck run end to end on the default recipe, all five output
  files byte-identical (`.frd` clock excepted);
- `converge_selftest()` — fourteen checks — runs before the object is used
  and the object refuses to arm if it fails, the discipline `lsladder.c`
  set; broken deliberately, it goes red and the run says so.
