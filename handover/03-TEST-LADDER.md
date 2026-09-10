# 3. The test ladder, and the rung that is missing

The constraint that shaped everything else: **validation used to cost 2.5
hours**, so nothing could be checked cheaply and every fix was made blind.
That is the mechanism behind "we fix one wall and another appears".

The opposite failure is just as real and less often noticed: a model that
runs in one second reproduces neither real defects nor real walls. It can
prove a change is *harmless*. It cannot prove one *works*.

## The ladder as it stands

| rung | cost | what it can prove | what it cannot |
|---|---|---|---|
| `test/pathfollow/close.inp` | **0.7 s** | a constitutive law, point by point, against its closed form | anything about a structure |
| `test/pathfollow/mixed.inp` | 23 s | the mixed-mode post-peak branch, snap-back, continuation | anything 3-D; its facet never closes |
| `test/fast` **plain** | 46 s | bulk damage, terminal deletion, cutbacks, 3-D topology, 90 deletions | it never masks a node and never walls |
| `test/fast` **wrapped** | **24 s** | **a node with no bulk support, held by facets alone**, and a wall it causes | the `s3rad` convergence class |
| `test/s3rad` | **2.3 h** | everything | cheaply |

`test/regress/run.py` runs the first four as nine cases in about three
minutes, and is proven able to go red.

## What made the 24-second rung work — and what did not

This is the transferable part, because the first four attempts failed.

**Size is not the lever.** Four bigger meshes were tried; none walls, none
masks a node. More elements and more deletion do not produce the mechanism.

**Where the facets are relative to where the crack goes IS the lever.** The
plain deck's crack runs along a free surface two cells away from the cohesive
plane; its worst node has 9 of 12 tetrahedra deleted and **no facet at all**,
so it either keeps bulk support or goes to exactly zero — it never lands in
between, which is where the interesting state is.

**The ordering is the lever.** For a node to end up with no bulk support and
failed facets, **the facets must fail first and the bulk erode afterwards.**
Every arrangement that erodes the phase while its wrap is intact leaves the
stripped node well supported by that wrap — the diagonal ratio floors at
2.5e-2 and never reaches 1e-3.

**Half-width wrapping is what bought the right order.** Wrapping the eroding
phase across the full section makes it the only load path, so it debonds
wholesale and unloads. Wrapping half the width lets the wrap fail
progressively under a stress gradient while the surrounding matrix keeps the
phase loaded.

Failures worth not repeating, each cheap to re-check and expensive to
rediscover: a small wrapped inclusion erodes **0 of its 24 elements** (the
wrap debonds at `Tn0=300` before the phase yields at 600, and an unloaded
inclusion cannot damage); a wrap made *stronger* than the phase erodes it but
then holds the stripped nodes on **intact** facets; moving the brittle band
behind the crack tip changes nothing at all.

## The rung that is missing

**Nothing reproduces the `s3rad` convergence class.** The wrapped deck
manufactures the *state* — a node with no load path — but the wall it hits is
a different mechanism: `CCX_DAMAGE_AUTOSPC_FORCE=1` does not move it, and its
residual misses tolerance by 520x where the `s3rad` wall missed by 5.7%.

The class to reproduce is: **a node with a healthy diagonal whose residual is
irreducible** — `need_du = |R|/k` of order the grip displacement, so no
physically meaningful step can equilibrate it, while the diagonal test sees
nothing wrong. On `s3rad` that is a node at the crack front that has lost 20
of its 24 elements but keeps four.

Building this rung is the highest-value test work available. It would make
the one remaining wall class diagnosable in seconds instead of hours, and it
is the last thing standing between the gate and being a real substitute for
`s3rad`.

## Rules for a new rung

- **Generate the deck, do not commit it**, so it cannot drift from its
  generator. Record the sha256 in the run's provenance.
- **Lift material and section cards verbatim from the target deck.** Restating
  them by hand was tried and was wrong.
- **State what the rung is FOR** in one sentence, in `cases.json`. A case
  whose purpose nobody can state is a case nobody will fix when it goes red.
- **Prove it can fail.** A suite that cannot go red is decoration. Force a
  behaviour change and check the right cases turn red with the right numbers.
- **Pin the trajectory, not just the outcome**: increments, deletion counts,
  and where possible the deletion **set** compared element by element and the
  status file compared byte for byte.
- Fix the thread count. Runs are not reproducible across thread counts.

## Where the time actually goes

`s3rad` at 42807 elements is 2.3 h on 2 threads. It should stay the last
gate, not the first. But note that a large part of its cost is spent **after
the specimen has broken** — increments 753 to 931 are a phantom regime. Fix
that (`05-DEBT.md`, item 1) and the target deck itself gets substantially
cheaper, which is the rare case where correctness and speed are the same
change.
