# Working brief for coding agents

## Mission

Develop and test a robust continuation method for cohesive fracture in this
CalculiX 2.23 fork.  The immediate target is to pass the known `s3rad` wall
near accepted increment 589 with genuine physical front advance.

Work autonomously.  You may refactor across source files, replace an
unsuitable continuation coordinate, or revert an unsuccessful direction.
Do not preserve an implementation merely because it already exists.

## Starting point

The repository already contains:

1. A bordered two-solve path-following implementation in `src/pathfollow.c`
   and its integration in `src/nonlingeo.c`.
2. A clean Mode-I cohesive benchmark in `test/pathfollow/cohesive.inp`.
3. A measured COD result with 798 accepted post-peak increments and an exact
   bilinear reaction branch.
4. The self-contained real target deck
   `test/s3rad/m12_s3rad_gc24_w.inp`.

Treat the Mode-I result as a regression test, not as proof that normal COD is
the right coordinate for `s3rad`.

## Facts that should not be rediscovered by speculative patching

- `vold != vini` before the Newton loop is expected: stock `prediction()`
  extrapolates the state and the predicted state is copied into `vold`.
- The historical `s3rad` run used PARDISO, an unsymmetric damage tangent,
  viscosity `1e-4`, Rescue2 and the dogleg rescue.  It stopped with `rc=201`
  at the wall around increment 589.
- The target deck has no external `*INCLUDE`; it is complete as committed.
- Prior measurements of the active `s3rad` front showed substantial shear.
  Normal opening alone is therefore not an adequate default control
  coordinate for the target.
- Generated run products do not belong in Git.

## Experimental discipline

Before a material code change, state a falsifiable hypothesis and the one
measurement that can reject it.  Do not respond to every failed run by adding
another threshold or flag.

Preserve these gates:

1. Feature-off behavior remains unchanged.
2. The existing Mode-I cohesive benchmark still follows the analytical
   descending branch.
3. Both the equilibrium residual and the continuation constraint converge.
4. Trial-state probes restore all mutable state or demonstrate reproducible
   regeneration.
5. Solver changes are explicit.  Do not compare a SPOOLES exploratory run to
   the PARDISO baseline as though only the continuation method changed.

## Recommended next sequence

1. Reproduce the committed Mode-I benchmark and record exact build and run
   parameters.
2. Audit only critical lifecycle points: predictor base, rollback/commit,
   load-factor sensitivity, bordered solves and convergence acceptance.
3. Generalize the control coordinate for mixed mode.  A reasonable starting
   point is a frozen linearization of

       deff^2 = max(dn,0)^2 + beta*(ds1^2 + ds2^2)

   so the constraint remains affine during one corrector.  This is a
   recommendation, not a required architecture.
4. Pass a small mixed-mode UC6 benchmark for at least 20 consecutive
   post-peak increments.
5. Only then run a bounded `s3rad` pilot.  Do not start `bandrad`, `s4rad`,
   `s5rad` or a parameter ladder.

## `s3rad` pilot success criteria

Success requires all of the following:

- the previous wall is passed;
- equilibrium and constraint residuals both converge;
- several consecutive continuation increments are accepted;
- the process zone, failed integration-point count, or deleted-interface
  count advances beyond the pre-wall state;
- the result is not obtained by merely weakening convergence criteria.

Many tiny accepted increments without front advance are a negative result.

Log at minimum: accepted increment, load factor, control coordinate, `deff`,
equilibrium norm, constraint residual, selected facet/IP, failed UC6 points
and deleted elements.

## Scope and delivery

Create a new branch.  Do not rewrite `main`.  Commit at evidence-backed
milestones.  Keep binaries and all `.dat`, `.frd`, `.sta`, `.log`, `.damage`,
`.12d`, plots and caches out of Git.

If GitHub write access is unavailable, deliver a verified `git bundle` with
base commit, head commit and import commands.  In the final report separate
reproduced facts, new measurements, assumptions, limitations and the single
best next experiment if the target has not been passed.
