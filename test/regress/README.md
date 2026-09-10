# The regressions that run in minutes

```sh
CCX_EXE=/path/to/ccx_2.23_pardiso test/regress/run.py -j 2
```

Nine cases, **about three minutes** on two cores, exit status = number of
failed cases.  `-k NAME` runs a subset, `-o DIR` puts the runs somewhere you
can keep.

This exists because of the architecture audit's central finding: validation
cost 2.5 hours, so nothing could be checked cheaply, and every fix was
therefore a point fix made blind.  That is the mechanism behind "we fix one
wall and another appears".

## What is covered, and why each case is there

| case | what it pins |
|---|---|
| `fast-plain` | bulk damage, terminal deletion, cutbacks, 3-D topology: 507 increments, 90 elements deleted, reaches `theta=1` |
| `fast-plain-smooth` | that the regulariser is a **byte-for-byte no-op** on a deck with real bulk damage and 90 deletions where no *damaged* facet ever closes.  `m.sta` and the deletion set are compared byte for byte.  The blend differs from the sharp law only when `g<1` **and** the facet is in compression, and this case is what holds that claim to it |
| `fast-wrapped` | a node with **no bulk support left**, held by cohesive facets alone (ratio `4.4641e-04`), and the crack-face kink wall it sits next to |
| `fast-wrapped-smooth` | that the kink regulariser removes the wall **and** deletes the same 64 elements - the deletion sets are compared element by element |
| `fast-wrapped-nospc` | that the load-path mask does **not** change that wall.  Two walls, two mechanisms; this case keeps them apart |
| `mixed-analytic` | the closed-form mixed-mode post-peak branch at 36% shear, through `check_mixed.py`: kinematics, reaction, displacement, snap-back and 2400 accepted post-peak increments |
| `mixed-analytic-smooth` | that arming the regulariser on a facet that never closes is **bit-identical**, `mixed.sta` compared byte for byte |
| `close-sharp` | the UC6 **normal law itself**, point by point, on a facet damaged to `gmin` and then pushed into contact.  This is where the five-orders tangent jump is *measured* - far-compression slope `999999` against `Kn=1e6`, far-tension slope `1` against `g*Kn=1`, **ratio 1e+06** - rather than argued from reading the source |
| `close-blend` | the same facet under the regulariser: the far-field slopes are unchanged, so nothing physical moved, and the blend band is actually entered (6 increments inside it) |

A preflight also fails the run if the generated switch registry is stale.
Every case requires the `DAMAGE TR`, `DAMSTATE` and `LSLADDER` self
tests to report `PASSED`, the `[SWITCHES]` line to be present, and fails on any `[X] ... N failure(s)` with `N>0`
anywhere in its log.

## What it does not cover

`s3rad` itself.  These cases are the cheap gate before it, not a replacement:
passing them says a change is *harmless*, and only `s3rad` says whether it
*helps*.  Run `test/s3rad/run_s3rad.sh` for that, and expect 2.5 hours.

Runs are **not reproducible across thread counts** (measured), so the numbers
in `cases.json` are 1-thread numbers and the runner pins
`OMP_NUM_THREADS=MKL_NUM_THREADS=1` unless you set them yourself.

## Adding a case

Add it to `cases.json` with a `what` that states the failure it is there to
catch.  A case whose purpose nobody can state is a case nobody will fix when
it goes red.  Cross-case relations available: `same_deletion_set_as` and
`identical_sta_as`.

## Proof that it can fail

A suite that cannot go red is decoration.  Forcing `CCX_UC6_CONTACT_SMOOTH`
on for the whole suite:

```
fast-wrapped        FAIL rc: want 201, got 0
                    FAIL last_inc: want 99, got 515
                    FAIL theta: want '0.158766E+00', got '0.100000E+01'
fast-wrapped-nospc  (the same three)
fast-wrapped-smooth ok
```

two of three red, with the exact quantity that moved, and exit status 1.
