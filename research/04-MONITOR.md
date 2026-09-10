# The census gets an owner, and the playbook gets a reader

Areas: `07-RESEARCH-AGENDA.md` **rank 5** (diagnostics as data),
`NEXT_TASK.md` item 4.

---

## 1. The problem as it exists here

`08-OBJECT-MODEL.md` §1 names it as one of three structural symptoms:

> **The same census is printed from three separate sites** in `nonlingeo.c`.
> Nobody owns it, so it was written three times.

Measured: three blocks of **29 lines each**, at `nonlingeo.c` lines 13076,
14074 and 14572 — **character for character identical**. They had not drifted
yet. The fourth copy would have.

And the wider problem behind it: diagnostics here are `printf` interleaved
with the solve, in ad-hoc formats. `02-DIAGNOSTICS.md` is eleven readings a
human performs by eye, and it cannot become anything else while the output is
prose.

## 2. What established codes do

- **PETSc monitors and viewers** — the separation this is copied from: a
  monitor *produces* nothing and a viewer *decides* nothing.
  `SNESMonitorSet` registers a callback that is handed the quantity; the
  viewer decides where it goes.
- **JSON Lines** as the log format, so a run's diagnostics are a data set
  rather than a transcript.
- MOOSE and Code_Aster both emit per-step scalars for post-processing rather
  than for reading.

## 3. Adopt / adapt / reject

**Adopted**: the producer/presenter split, as two files rather than one, so
that the split is visible rather than asserted.

- `src/census.c` — the **quantity**. Pure: reads three arrays, writes one
  struct, decides nothing, prints nothing.
- `src/monitor.c` — the **presentation**. Prints the line the tree already
  had, byte for byte, and a `[MON] {...}` JSON record beside it.

**Adapted**: the legacy line is preserved *exactly*, and that is a
requirement rather than politeness. `test/regress/run.py` parses
`below_1e-3=` and `worst=node_N_at_R` out of it to pin two of the nine gate
cases. Changing the format would have made this extraction a behaviour change
wearing a refactor's clothes.

**Rejected**: a switch to turn the JSON off. The census only prints at all
when the stiffness probe is armed, which is once per accepted increment; one
more line there is not worth a knob, and a knob is what this project has too
many of.

**Kept from this tree, not from the survey**: the object carries a self test
and **refuses to arm if it fails** (`lsladder.c`, `damstate.c`). With one
correction, learned the hard way — see below.

## 4. What was built, and what tests it

`src/census.c` (110 lines), `src/monitor.c` (50), `tools/readmon.py`.

The three sites in `nonlingeo.c` become five lines each:

```c
if((damage_stiff_probe>0)&&(damage_addiag!=NULL)){
  stiffcensus damage_census;
  stiffcensus_take(&damage_census,*nk,damage_addiag,damage_addiag0,damage_addok);
  if(damage_census_ok) monitor_stiffness(&damage_census,iinc,theta**tper);
}
```

`nonlingeo.c`: **16094 → 16056** lines, and seven scratch variables that only
existed to carry that loop's intermediate state are gone from a declaration
block that is 200 lines long.

### The self test, and the mistake it caught — in the right direction

`stiffcensus_selftest()` builds six nodes: one healthy, one *exactly* at each
threshold, one below all three, one with a non-positive diagonal, one with no
intact reference. It asserts that the thresholds are **strict** (a node at
exactly `1e-3` is not *below* `1e-3` — that node has repeatedly been the one
under discussion), that a non-positive diagonal is counted and not compared,
that a masked node is absent from every count *and* hands the extreme to the
next node down, and that an empty census is empty rather than wrong.

The first version of that test asserted the wrong thing about masking. It
failed, the census refused to print, and **five of nine gate cases went red**.
Which is the correct behaviour of the whole arrangement — but it exposed a
design error worth recording:

> **The first version cleared `damage_stiff_probe` on failure. That flag also
> gates the assembly of `damage_addiag`, which the load-path judgement reads.
> Clearing it changed `m.cvg` on three gate cases.**

A self test must never smuggle in a behaviour change. The suppression is now
on a separate flag that gates **the report and only the report**; the solve is
untouched whatever the self test says. That distinction — measurement refuses
to speak, judgement carries on — is the one this file exists to draw, and it
was nearly got backwards.

### Demonstrated failing

`stiffcensus_take`'s `<` changed to `<=` on the `1e-3` threshold, rebuilt, gate:

```
FAIL CENSUS reported an error:: below_1e-3 is 2, want 1 (is the comparison strict?)
FAIL CENSUS reported an error:: a masked node still reached the census (...)
FAIL CENSUS reported an error:: the stiffness census self test failed; not printing...
3 of 3 case(s) failed
```

Reverted.

### Equivalence

**Bit-identical**, checked and not assumed. All nine gate cases against the
pre-extraction binary: `m.sta`, `m.damage`, `m.cvg`, `m.dat`, `mixed.*`,
`close.*` compare equal byte for byte. The `[DAMAGE STIFFNESS]` lines
themselves — 33 of them on `fast-wrapped` — are byte-identical too. 9 of 9
green.

## 5. The playbook becomes a program

`tools/readmon.py` reads the `[MON]` records and applies
`02-DIAGNOSTICS.md` §3, printing a line only when the *band* changes:

```
$ tools/readmon.py test/regress/_runs/.../fast-wrapped
33 stiffness census record(s), increments 20 to 96

  inc    worst ratio    node       below1e-3 reading
  20     4.9285e-01     354        0        nothing has lost significant support
  26     2.9108e-01     353        0        worst node still has live elements  [near the band edge]
  39     3.8355e-02     440        0        worst node has lost its bulk but its facets are still intact
  96     4.4641e-04     440        1        worst node has lost its bulk but its facets are still intact
```

Two things about it are deliberate.

- **It does not invent precision.** `02-DIAGNOSTICS.md`'s `3.7e-07` is
  `gmin*(Kn*A/K_bulk)` computed **for `s3rad`** — a deck constant, not a
  universal threshold. The reader says so, and flags a value sitting near a
  band edge as a trend rather than a class. Hard-coding that number as
  universal is precisely the chain of tuned thresholds this project exists to
  avoid.
- **It reproduces a recorded fact from a fresh run.** On `fast-plain` it
  reports the extreme as node 415 at **1.0406e-01** — which is the number in
  `04-REFUTED.md` explaining why that deck can never exercise the mask, since
  `CCX_DAMAGE_AUTOSPC` is clamped at `1.e-1`. Nobody had to remember to look
  it up.

## What is still open

- One record kind. The other ten readings of `02-DIAGNOSTICS.md` are still
  prose, and the largest of them — the line-search ladder census, §1, *"the
  most valuable single diagnostic in the tree"* — is the one worth doing next.
- `m.cvg` carries **three significant digits** (`research/02-COMPARISON.md`).
  The convergence record is the artefact a decomposition of the Newton loop
  will need to compare tightly, and a `[MON]` record is where that should be
  fixed rather than by changing the `.cvg` format the world already reads.
- Nothing yet records **why** an attempt was abandoned in machine-readable
  form. That is the Convergence object (item 5), and the `[MON]` stream is
  where its reason enum should land.
