# 11. Topology: the transaction that commits an erosion

Next object after Convergence in the migration order of `08-OBJECT-MODEL.md`
section 5. Written before the code, as that section requires.

## 1. Where the responsibility lives today

`nonlingeo.c` holds **139 references** to nine `damage_tent_*` names:

| name | what it is |
|---|---|
| `damage_tent_elem/mat/ip/value` | four parallel arrays, one entry per element eroded in this batch |
| `damage_tent_count` | how many |
| `damage_tent_step/increment` | when it happened |
| `damage_tent_step_time/total_time` | at what time it happened |

Nine variables with no owner is the symptom. The disease is what that
forces, and it is measurable rather than a matter of taste:

**The "discard the transaction" block appears six times** — and in three
different variants, which is worse than six copies of one thing:

| variant | sites | shape |
|---|---|---|
| guarded, count inside | 3 | `if(elem){ free x4; count=0; }` |
| guarded, count outside | 2 | `if(elem){ free x4; } count=0;` |
| unguarded | 1 | `free x4; count=0;` with no NULL test at all |

(The first survey of this file said four sites. It was wrong: grouping the
line numbers by proximity merged two pairs. Counting them by pattern
instead of by eye found six, and found that they are not the same six.)

Collapsing them picks one shape — guarded free, count always zeroed — and
that is a behaviour change unless `count!=0` implies `elem!=NULL`. It does:
`count` is only made non-zero immediately after the arrays are allocated.
The invariant is stated here because it is the thing that makes the
unification legitimate, and bit-identity on the gate and the deck is what
confirms it empirically rather than by argument.

**The "collect what was eroded" block appears three times** — at 13472,
14382 and 14850 in the pre-extraction file, about fifty lines each counting
the sizing pass. Strip whitespace and comments and all three are the *same
string*. Fifty lines of scanning `ipkondamageini` against `ipkon`, finding
the worst integration point per element and applying the DE1.3 trigger
override, written out three times.

(The first survey said twice. Two of the three differ by a three-line
comment, which a diff of the raw text reports as a difference and an eye
skimming for duplicates accepts as "a different block". Normalising before
comparing found the third. Both counts in this section were wrong on the
first pass and in the same direction — undercounting duplication, because
near-copies do not look like copies.)

That is the entire argument for this object. Nobody chose to write the
collection loop twice; it happened because there is nothing to call.

## 2. Survey

**deal.II `Triangulation`** — `include/deal.II/grid/tria.h`, master, fetched
2026-09-13:
<https://raw.githubusercontent.com/dealii/dealii/master/include/deal.II/grid/tria.h>

The mesh-change transaction, and the closest match to this problem in any
settled code. Three named phases where this tree has one inline block:
`set_refine_flag()` marks a cell tentatively; `prepare_coarsening_and_-
refinement()` regularizes the marked set before anything is committed;
`execute_coarsening_and_refinement()` commits the whole batch in one call.
Observers attach around the commit rather than inside it —
`Signals::pre_refinement` and `post_refinement`, tria.h:2379 and 2386.

**Taken**: the phase separation. Marking, regularizing and committing are
three questions, and this tree answers all three in the middle of a 45-line
block that it then repeats. It has a regularize phase already and does not
know it — the DE1.3 trigger override, which replaces the scanned value and
integration point for elements the DE1.3 path flagged, is exactly
`prepare_*`: adjusting the marked set before it is committed.

**Rejected**: `boost::signals2` observer registration. Three observers here
(the deletion history, `topodiag`, the printing) and all three are known at
compile time. A signal registry for three compile-time observers is
ceremony, and this project has already said so once about PETSc's
`SetFromOptions`.

**PETSc `DMAdaptLabel(DM, DMLabel, DM*)`** — `include/petscdm.h:123`, main,
same day; `DMLabel` itself in `include/petscdmlabel.h` (`DMLabelCreate`,
`DMLabelSetValue`, `DMLabelGetStratumIS`).

The same idea written in C, which is the idiom this file needs. The marked
set is a **first-class object with its own lifetime**, created once and
passed to the adapt call — not four arrays whose freeing is copy-pasted
wherever someone remembered.

**Taken**: the marked set as an object. "Discard the transaction" becomes
one call, and it becomes impossible to free three of the four arrays.

**Rejected**: out-of-place adaptation. `DMAdaptLabel` returns a *new* DM
and leaves the original intact, which is the better design and is not
available here: CalculiX mutates `ipkon` in place and the entire increment
loop, the restart path and the `.frd` writer depend on that. Changing it is
not a refactor and is not in this project's scope.

## 3. The object

```c
typedef struct{
  ITG *elem,*mat,*ip;      /* one entry per eroded element */
  double *value;
  ITG count,cap;
  ITG step,increment;      /* when this batch happened */
  double step_time,total_time;
  ITG open;                /* a transaction is in progress */
}topo_txn;
```

The questions it answers, in the order a batch lives through them:

- `topo_txn_discard()` — release the marked set. The four copies collapse
  into this, and this is the whole of step A.
- `topo_txn_open(step,inc,step_time,total_time,cap)` — begin a batch and
  stamp when it is happening.
- `topo_txn_mark(elem,mat,ip,value)` — add one eroded element.
- `topo_txn_collect(...)` — the 45-line scan, once. Calls `mark`.
- `topo_txn_commit(...)` — hand the batch to the history and the observers.

## 4. What is NOT in this object

- **What counts as eroded.** `damstate.c` and the damage model decide that;
  Topology records and commits the decision. The scan reads `ipkon` against
  `ipkondamageini` — it observes a deletion that has already happened.
- **The sparsity decision.** `08-OBJECT-MODEL.md` section 4 pairs Topology
  with `CCX_PARDISO_REUSE_SYMBOLIC`, because a committed batch is exactly
  when the symbolic factorization stops being valid. That is a real
  connection and a later step: it is a behaviour change with its own
  hypothesis, not part of an extraction.
- **The rollback.** There is no rollback today — a discarded transaction
  frees the marked set, it does not restore `ipkon`. Naming the object does
  not create one, and pretending otherwise would be the worst kind of
  refactor.

## 5. Order of work

Step A is `topo_txn_discard()` and the state struct: four call sites
collapse to one call, pure movement, bit-identity is the standard.

Step B is `collect`, which removes the two redundant copies and takes
`topo_element_nip` with it — the element-type-to-integration-point map was
a `static` in nonlingeo.c shared by sixteen call sites and testable by
nobody. It is a pure function of the element label, so it is the one part
of this object that can be tested exhaustively rather than by example.

Step C is `commit` and the observers, which is where the history write and
`topodiag` go.

Each step proves bit-identity on the gate and on the target deck before the
next one starts, the discipline steps A-C of `10-CONVERGENCE.md` used.
