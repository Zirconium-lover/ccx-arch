# 6. The damage-consistent tangent: what it is, what it costs, and the verdict

All numbers on this page: `OMP_NUM_THREADS=MKL_NUM_THREADS=2`, one machine,
`test/fast` decks, binary built 2026-09-11 from `src/build_mkl.sh`.  Each arm
was driven by `tools/outcome.py`, which passes overrides positionally and
verifies them against each run's own `[SWITCHES]` banner, because the same
A/B was twice run with both arms configured identically.

## What was actually wrong

The consistent tangent of a degrading material is

    C = g(D) C_ep  -  sigma_eff (x) dD/d(eps),     g = 1 - D

and this tree assembles the rank-1 correction in two different places, in
two different conventions, under two different switch values.  The question
`09-STEERING` put first - *is the Newton direction a Newton direction* -
turned on which of those pieces is right.

**`mafilldamas.f` (UNSYM) is correct.**  The hypothesis carried into this
work was that its projection loses most of the term: the correction is
computed at full constitutive size and moves the worst matrix coefficient by
7.13 against a gap of 1375.  That hypothesis is **refuted**.  The projection
was extracted into `src/damrank1.f` - one owner for "damjac(12) at an
integration point -> the element block" - and given a self test that checks
it three independent ways:

| check | result |
|---|---|
| the outer product against `e_c3d`'s own quadruple sum, at `F != I` | agrees to `4.9e-16` of the block's largest entry |
| at `F = I`, against a finite difference of the element internal force through the damage scalar | agrees to `1.1e-10` |
| the same with the shear slots of `dD/d(eps)` doubled - the exact shape of an engineering-vs-tensor convention error | **caught**, `0.25` relative |
| a flat tetrahedron | returns `detJ = 0` and writes nothing |

The third row is the point.  A test that cannot be made to fail is not a
test, so the test breaks the projection deliberately in the way that was
suspected and shows it going red.

The extraction is **bit-identical**: all ten gate cases byte-for-byte
unchanged (`--exact`), including `m.frd`.

**`resultsmech.f` (FD_SYM) was wrong, in two ways, and is now fixed.**

1. `damageq` is `dD/d(emec)` and `emec(4:6)` are **tensor** shears, while
   the 21-entry constitutive matrix `e_c3d` consumes is in **engineering**
   shear.  The UNSYM path halved the shear components; the symmetric path
   used them raw.  Every entry of its rank-1 correction touching a shear
   index was a factor 2 too large and every shear-shear entry a factor 4.
2. With viscous regularisation the degradation follows `Dvis`, so the term
   needs `beta * dD/d(eps)` with `beta = dtime/(eta+dtime)`.  The symmetric
   path carried no `beta` at all.

Both conversions now happen once, into `damqeng`, which both paths read.

A third defect, latent: in the compression branch the hydrostatic part of
the stress is **not** degraded, so the rank-1 term must ride on the
deviatoric normals only.  Both paths used the full effective stress.  Fixed
in both.  `CCX_DAMAGE_COMPRESSION` is off by default and off on every deck
here, so this changed no measurement - it is recorded because it would have
been found the hard way the first time somebody armed it.

## What the fix did

Before: `CCX_DAMAGE_TANGENT=FD_SYM` **walled `fast-plain` at increment 41**,
`rc=201`, `theta=0.0296`, **zero deletions** - it destroyed the run.

After: it completes.

## The verdict, on `fast-plain`

| arm | wall | Newton iters | attempts | deletions | fracture |
|---|---|---|---|---|---|
| `CCX_DAMAGE_TANGENT` unset | **23.11 s** | **1286** | 542 | 90 | reference |
| `UNSYM` | 31.58 s (+37%) | 1283 (-0.2%) | 542 | 90 | **byte-identical `m.damage`** |
| `FD_SYM` | 25.62 s (+11%) | 1447 (+13%) | 598 | 91 | one extra element |

and on `fast-wrapped`, the deck that walls:

| arm | rc | theta at the wall | iters | deletions |
|---|---|---|---|---|
| unset | 201 | 0.158754 | 571 | 64 |
| `UNSYM` | 201 | 0.158766 | 606 | 64, identical set |
| `FD_SYM` | 201 | 0.158163 | 552 | 64, identical set |
| `UNSYM` + `TANGENT_FULL=1` | 201 | 0.158766 | 606 | 64, identical set |

Three things follow, and none of them is what the queue expected.

**The wall is not the tangent.**  Every mode walls at the same place with
the same 64 elements gone.  A correct Newton direction does not move it by
0.01% of load factor.  Whatever `02-DIAGNOSTICS` reads in the process zone
is still subject to re-check - an inconsistent tangent does contaminate a
ladder reading - but the wall itself now has a mode-independent measurement
against it.

**The correct rank-1 term does not earn its cost.**  `UNSYM` buys 0.2% of
iterations for 37% of runtime, and produces a byte-identical fracture.  The
37% is the asymmetric factorisation, not the term: PARDISO phase 22 goes
from 9.78 s to 14.35 s and the CSR fill grows 54-fold (`01-PROFILING`).

**`FD_SYM`, now that it is correct, is worse than having no correction at
all** - 13% more iterations, 11% more wall time, and a different fracture.
That is not a surprise in hindsight: the symmetric part of a genuinely
nonsymmetric operator is not a Newton tangent, it is a third operator that
happens to be storable in 21 constants.  Its only defence was that nobody
had run it correctly.  Now somebody has.

**`CCX_DAMAGE_TANGENT_FULL=1`** - which moves the tangent cut-off from
`D=0.999` to the residual-stiffness floor, closing the one hole
`mafilldamas.f` names as genuine (`damcat=5`) - produces **output identical
to leaving it off**.  The band it exists to serve is empty on both fast
decks.  It is not deleted yet only because `s3rad`, with 3790 deletions
against 90, is where a terminal band would show up if anywhere.

## What is still open

- `s3rad` at scale.  Every number here is a two-minute deck.  The cost side
  of the `UNSYM` verdict gets *worse* with `N` (the asymmetric factorisation
  grows faster than the symmetric one); the benefit side is unmeasured.
- Where the remaining operator error comes from, now that the projection is
  exonerated.  `05-TANGENT-POPULATIONS` already measured that the
  discrepancy is present **before any damage**, identical to four
  significant figures with the damage cards stripped, and roughly ten times
  larger after initiation.  The first part of that is `incplas`, not this
  module.

## Appendix: and then the mechanisms built on top of it

`09-STEERING` §5 asks, for each of the six globalization mechanisms, to name
the failure it addresses and the gate case that goes red without it.  Four of
them are armed by `test/s3rad/run_s3rad.sh`, which both fast decks inherit:
the adaptive line-search ladder, Rescue 2, the dogleg trust region, and the
physical re-equilibration scale.  (Path following and crack control are not
armed on these decks.)  Leave-one-out, same binary, two threads:

`fast-plain`, which completes:

| arm | rc | inc | attempts | iters | deletions |
|---|---|---|---|---|---|
| shipped | 0 | 507 | 542 | 1283 | 90 |
| no line search | 0 | 505 | 538 | **1259** | 90, identical set |
| no Rescue 2 (and so no dogleg) | 0 | 507 | 542 | **1283** | 90, identical set |
| no dogleg | 0 | 507 | 542 | **1283** | 90, identical set |
| no physical scale | 0 | 507 | 542 | 1319 | 90, identical set |
| none of the four | 0 | 505 | 538 | 1295 | 90, identical set |

`fast-wrapped`, which walls:

| arm | rc | theta at the wall | attempts | iters | deletions |
|---|---|---|---|---|---|
| shipped | 201 | 0.158766 | 151 | 606 | 64 |
| no line search | 201 | 0.159574 | 150 | 577 | 64, identical set |
| no Rescue 2 (and so no dogleg) | 201 | 0.158754 | 137 | 519 | 63 |
| no dogleg | 201 | 0.158758 | 144 | 557 | 64, identical set |
| no physical scale | 201 | 0.158762 | 150 | 639 | 64, identical set |
| none of the four | 201 | **0.159572** | 144 | 568 | 63 |

Read it plainly.

**Rescue 2 and the dogleg never fire on `fast-plain`.**  Turning either off
reproduces the shipped run to the digit - 542 attempts, 1283 iterations, the
same 90 elements.  That is not "they help a little"; it is that they do not
run.

**The line search is a net cost on both decks.**  Dropping it is 1.9% fewer
iterations on the deck that completes and, on the deck that walls, a wall
0.5% **later** in load factor.

**Turning all four off is better than the shipped configuration on the deck
they were built for.**  `fast-wrapped` walls at `theta=0.159572` with nothing
armed against `0.158766` with everything armed.

What this does **not** license is deleting them.  These mechanisms were added
one per wall on `s3rad` and the DHC decks, and every number above is from a
two-minute deck.  The honest statement is the one the gate can now make:
**on the eleven-case gate and on both fast decks, not one of the four can be
shown to do anything, and three of them can be shown to do nothing.**
`fast-plain-noglob` and `fast-wrapped-noglob` pin that, so it stays a result
and not a memory.  The leave-one-out on `s3rad` is what would turn it into a
deletion.

And note what has changed about the premise.  `09-STEERING` puts the tangent
first because "an unknown fraction of those six mechanisms is scar tissue
around a defect nobody knew was there", the defect being the rank-1 term in
`mafilldamas.f`.  That term is measured correct, and all four tangent modes
wall `fast-wrapped` at the same place with the same elements gone.  The
mechanisms may well be scar tissue - the table above is what that would look
like - but if so it is not scar tissue around **this** wound.
