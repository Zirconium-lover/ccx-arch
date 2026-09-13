# What perturbs this deck, and what does not

*2026-09-13.  Target deck `m12_s3rad_gc24_w.inp` (sha256 `2fb0cf4e…`).  The
thread-count pair is on binary `6d752beb`; the switch pairs are on
`389909a7`.  Every pair below differs in exactly one thing.*

The `14-THE-TRAP.md` finding — that the wall is a trap entered on a
one-element perturbation — makes one question urgent: **what counts as a
perturbation here?**  Four pairs answer it, and they split cleanly in two.

## Byte-for-byte identical over a full run

| pair | the one difference | result |
|---|---|---|
| `base2` / `norepack` | `CCX_PARDISO_REPACK` on vs off | **all 5 files identical** |
| `cut6` / `cut4` | `OMP=MKL=6` vs `OMP=MKL=4` | **all 5 files identical** |
| `cutonly` / `cut6` | the binary before and after `DEADALL_FACET` was deleted | **all 5 files identical** |

"Identical" is the whole run: 1034 increments, 3796 deletions, a 134 MB
`m.dat`, compared byte for byte.  The only exception is the `.frd` wall
clock and calendar record, which `ccxdiff` excuses narrowly and has a
mangled-record test to keep narrow.

Two of these are worth stating on their own.

**The CSR permutation cache is an exact no-op at scale.**  The gate proves
it on a three-second deck; this proves it on the deck it was written for.

**This deck is reproducible across thread counts.**  `CLAUDE.md` records
*"Runs are not reproducible across thread counts — measured"*, and the rule
it justifies — pin `OMP_NUM_THREADS` and `MKL_NUM_THREADS` on both arms —
costs nothing and stays.  But under the current recipe it does not
reproduce: PARDISO reports `number of threads = 6` and `= 4` in the two
logs, and the runs agree to the byte.  The likely reason is in the launcher
already: `MKL_CBWR=COMPATIBLE`, which is precisely the setting that makes
MKL reproducible across thread counts.  Recorded so that the next person who
needs a physically neutral perturbation does not reach for this one.

## Perturbations that change the trajectory

| pair | the one difference | first divergence | outcome |
|---|---|---|---|
| `base2` / `spconly` | `CCX_DAMAGE_AUTOSPC_FORCE` | inc **524**, theta 0.2315 (5 iterations vs 8) | same ending: theta 0.5569 / 0.5570, 3796 deletions both |
| `base2` / `dafacet` | `CCX_DAMAGE_DEADALL_FACET` | inc **610**, theta 0.2445 | **different ending**: 0.5569 vs 0.2550 — one falls into the trap |

Neither switch changes an equation.  `AUTOSPC_FORCE` changes which dofs the
convergence *test* looks at; `DEADALL_FACET` changed which of twelve already
dead elements come out.  Both change the path from a definite increment
onward, and one of them decides whether the run meets the trap.

## What this does to the CCX_FRACTURE_CUT result

Four arms carry `CCX_FRACTURE_CUT=1e-4`.  All four end `Job finished` with
`[FRACTURE COMPLETE]`.  Where they end is another matter:

| arm | inc | theta | reaction | % of peak | cut ratio |
|---|---|---|---|---|---|
| `cutonly` | 667 | 0.258750 | 45.54 | 1.4629 % | 4.722e-05 |
| `cut6` | 667 | 0.258750 | 45.54 | 1.4629 % | 4.722e-05 |
| `cut4` | 667 | 0.258750 | 45.54 | 1.4629 % | 4.722e-05 |
| `spccut` | 650 | 0.261500 | 26.43 | **0.8491 %** | 3.198e-05 |

Three arms agree to the byte.  The fourth differs by one switch that changes
no equation, and it stops at **1.7 times less load**.

So the two halves of this result have to be reported separately:

- **the termination EVENT is robust.**  Every arm carrying the criterion
  terminates on it, and none of the arms without it does.  That is the
  result: `rc=201` becomes `Job finished` at a measured load-path width;
- **the termination LOAD is not.**  A perturbation that changes nothing
  physical moves it by a factor of 1.7.  Any sentence of the form "the deck
  separates at N % of peak" is currently unsupported, including the one I
  wrote comparing `spccut` to `cutonly` and have already withdrawn.

The honest statement of what the criterion buys is: *the run now ends on a
measurement of the load path instead of on the solver giving up*, and the
load at which it ends carries an uncertainty of at least a factor of two
until the trap is understood.
