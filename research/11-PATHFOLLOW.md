# 11. Diagnosing the dissipation path follower

`CCX_PATHFOLLOW` implements the Gutierrez (2004) / Verhoosel et al. (2009)
dissipation constraint.  `research/10-SURVEY-EXTREME.md` named it as the
established answer to the post-peak regime and noted that it is compiled in
and switched off on every deck here.  Before recommending it be armed, it
needed a verdict.  It now has one, and the verdict is that **the algebra is
sound and the integration is broken.**

The tool is `tools/pathfollow.py`.  It runs a deck with the follower armed,
parses the per-increment `[PATHFOLLOW]` lines and checks five properties that
fail separately.  Its own self test constructs a synthetic log violating each
property in turn and shows the diagnostic going red on that property and no
other; it runs in the gate preflight alongside `ccxdiff --selftest`.

## What was measured

`fast-plain`, `OMP_NUM_THREADS=MKL_NUM_THREADS=2`, control arm is the same
deck with the follower off: **507 increments, theta 1.0, 90 deletions, 1286
Newton iterations**.

| tau | rc | increments | iterations | deletions | final lambda | ENFORCED | FREE |
|---|---|---|---|---|---|---|---|
| 1e-4 | **201** | 7 | - | 0 | 0.0115 | 0 of 1 (worst **3084%**) | - |
| 1e-3 | 0 | 996 | 6894 | **0** | **0.0132** | 784 of 989 (worst 219%) | **0 of 988** |
| 3e-3 | 0 | 996 | 7357 | **0** | **0.0152** | 755 of 989 (worst 100%) | **0 of 988** |
| 1e-2 | 201 | - | - | 0 | 0.0266 | 802 of 927 (worst 68%) | **0 of 926** |
| 3e-2 | 201 | - | - | 0 | 0.0135 | 0 of 1 (worst **1547%**) | - |

## Four separate findings

**1. The constraint algebra works.**  Where the follower runs at all it
enforces `dG = tau` to within 5% on about 80% of engaged increments.  That is
not perfect - a fifth of increments miss, the worst by a factor of three -
but it is a constraint being solved, not ignored.  The bordered system, the
fixed reference vector and the exact derivatives in `src/pathfollow.c` are
doing what the paper says.

**2. `tau` is an absolute energy with no stated scale, and the failure mode
for a bad guess is death.**  On this deck the natural dissipation of the
first softening increment is `3.18e-03`.  At `tau=1e-4` - thirty-two times
smaller - the follower engages, overshoots by 3084%, cuts `tau` five times
(1e-4 → 3.1e-6), and the run dies with "too many cutbacks".  At `tau=3e-2` it
overshoots by 1547% and dies the same way.  Nothing in the declaration, the
banner or the documentation tells the deck author what decade to choose, and
the working window here is one decade wide.  Note the direction of the
cutback: when the achieved dissipation **exceeds** the target, the response
is to **lower the target**, which widens the gap it is trying to close.

**3. lambda never decreases.  Not once, in 988 engaged increments, at any
tau.**  A dissipation follower exists to walk a branch on which the load
parameter falls; that is the entire argument for preferring it to a
geometric arc length, and it is quoted in the file's own header.  On both
fast decks it has never done it.  So the mechanism has never been exercised
where it matters, and "it works" remains unproven for the case it was
written for.

**4. And the one that matters: it reports success and produces no fracture.**
At `tau=1e-3` the run ends `rc=0` with `theta=1.0`, which reads as a clean
completion - better than the control, which is the same deck ending at
`theta=1.0` after 507 increments.  It is not better.  It deleted **zero
elements** against the control's 90, and the final load factor is
**lambda = 0.0132**.  The specimen was loaded to 1.3% of the history that
breaks it.

The mechanism of that false success is stated in the follower's own arming
banner: *"lambda is decoupled from the step time and MAY DECREASE; theta
stays monotone so dtime>0"*.  Decoupling them is correct and necessary.  What
is missing is that **the run's completion test still reads `theta`**.  So the
follower walks step time to 1.0 while the load factor stalls at 0.013, and
CalculiX declares the step finished.  Nothing anywhere checks that lambda
arrived.

This is the `09-STEERING` trap in its purest form - *compare arms at a
physically meaningful event, not at whichever increment the solver gave up
on*.  Here the arm that "succeeded" is the one where nothing happened.

## What it means for the wall

It means `CCX_PATHFOLLOW` cannot be recommended as it stands, and
`10-SURVEY-EXTREME.md` rank 2 is withdrawn: arming it on `s3rad` would
produce a run that ends `rc=0` having fractured nothing.

It would not have helped anyway.  `research/09-SEVERANCE.md` shows the
`s3rad` wall is a **mechanism** - two bodies joined at one triangular element
face - and a path follower chooses a better control parameter for tracing an
equilibrium branch.  There is no branch there to trace.

## What would make it usable

Three things, in order, none of them large:

1. **A completion test on lambda.**  If lambda is the load parameter, the
   step is finished when lambda reaches 1, not when theta does.  Without this
   every path-following run is a false pass.
2. **`tau` expressed relative to something the deck knows** - the elastic
   energy at first damage, say - rather than as a bare number, so that a deck
   author can choose it. Or auto-set from the first dissipative increment,
   which this run measures anyway and prints.
3. **The cutback sign.**  Overshooting the target dissipation should shrink
   the increment, not the target.

Until 1 exists, the switch should refuse to arm rather than produce a run
that looks finished and is not.
