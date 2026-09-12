# The s3rad wall is a trap, not a barrier

*2026-09-12.  Every arm below is the committed deck
(sha256 `2fb0cf4e…`), binary **`389909a7`**, `OMP_NUM_THREADS=MKL_NUM_THREADS=6`,
`MKL_CBWR=COMPATIBLE`.  `prof2` is on a different binary and is shown only to
mark where this started.*

## The arms

| arm | overrides | ending | theta | reaction | % of peak | deleted |
|---|---|---|---|---|---|---|
| `prof2` *(old binary `d58ce3ab`)* | — | rc=201 inc 599 | 0.255019 | 66.45 | 2.1346 % | 3744 |
| `base2` | **none** | rc=201 inc 1034 | **0.556913** | 1.3532 | **0.0435 %** | 3796 |
| `spconly` | `AUTOSPC_FORCE` | rc=201 inc 1003 | 0.557017 | 1.3502 | 0.0434 % | 3796 |
| `spcforce` | `AUTOSPC_FORCE + DEADALL_FACET` | rc=201 inc 997 | 0.556835 | 1.3501 | 0.0434 % | 3808 |
| `dafacet` | `DEADALL_FACET` | rc=201 inc 664 | **0.254981** | 66.82 | **2.1467 %** | 3745 |
| `cutonly` | `FRACTURE_CUT=1e-4` | **COMPLETE** inc 667 | 0.258750 | 45.54 | 1.4629 % | 3749 |
| `spccut` | `AUTOSPC_FORCE + CUT=1e-4` | **COMPLETE** inc 650 | 0.261500 | 26.43 | 0.8491 % | 3757 |

## What this says

**1. `CCX_DAMAGE_AUTOSPC_FORCE` does nothing here.**  `base2` and `spconly`
differ only by that switch and land on the same theta to 0.02 %, the same
reaction to four figures, and the **same 3796 deletions**.  The switch is
armed, it excludes 117–273 dofs, its own honesty check passes on 525
accepted increments — and the run goes exactly where it would have gone
anyway.  A mechanism can be correct, instrumented, and irrelevant.

**2. The wall is path-dependent, not a barrier at a load level.**  Two arms
fall into it (`prof2`, `dafacet`) and stop at theta 0.2550 with 2.13–2.15 %
of peak still carried.  Four arms walk past the same load level without
noticing.  What separates them is not a switch that "fixes" the physics:
`dafacet` differs from `base2` by **one extra element deleted** (3745
against 3744 at the same stage) and that is enough to fall in.

So the wall at 2.13 % of peak is a **trap the trajectory can fall into**,
entered or missed on a perturbation far smaller than any mechanism here.
`prof2` fell in on an older binary; `base2` misses it on a newer one with no
switch changed.  Which of the twelve commits between them moved it is not
yet known — an arm with `CCX_PARDISO_REPACK=0` is running, because that
commit flipped a **default** and is the only one of the twelve that changes
what the solver computes rather than what it prints.

**3. `CCX_DAMAGE_DEADALL_FACET` is worse than useless on this deck.**  It
was already a retirement candidate for buying nothing.  It now has an arm
where it is the **only** difference from a run that goes twice as far.  It
does not cause the trap — `spcforce` carries it and misses the trap — but it
is the perturbation that put one run into it.  Off by default, and it should
come out.

**4. `CCX_FRACTURE_CUT` is the only switch here that changes an outcome.**
Both arms carrying it end `Job finished` with `[FRACTURE COMPLETE]` instead
of `rc=201`, at a measured load-path width (3.2e-05 and 4.7e-05 of the width
at the first deletion batch) rather than at whichever increment the solver
gave up on.  That is the one result in this note that survives.

## What I got wrong, and the rule that would have caught it

I compared six arms against `prof2` without reading the executable hash in
`provenance.txt`, and published two conclusions on that comparison
(`5abcf8c`, `2edd69f`).  `prof2` was twelve commits older.  The control that
refutes both — *the stock recipe on the same binary* — is the first arm that
should have been run and was the seventh.

The rule is already written down in `CLAUDE.md`: **compare arms at a
physically meaningful event**, and **feature off must be bit-identical, and
you must check it**.  I checked "feature off" on the gate, where it is a
three-minute run, and did not check it on the deck the conclusion was about.

Two further consequences:

- The `spccut` vs `cutonly` difference (0.85 % against 1.46 % of peak at
  termination) is a **single-arm difference of the same size as the trap
  perturbation**, and must not be read as an effect of the force mask until
  it is repeated.
- Any future s3rad claim needs its own stock-recipe arm on the same binary,
  in the same batch.  A baseline from yesterday is not a baseline.
