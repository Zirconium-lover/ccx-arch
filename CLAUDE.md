# Working brief for coding agents

Read `NEXT_TASK.md` before changing code or starting a long calculation.

## Current baseline

This branch already contains two evidence-backed results that must be kept as
regressions:

1. Mixed-mode UC6 crack control follows analytical post-peak branches at both
   moderate and shear-dominated mode mix.
2. The original `s3rad` stop near `theta=0.21218` was caused by the damage
   line-search ladder, not by an orphan node or a repeated deletion batch.
   `src/lsladder.c` fixes that defect and passes the same deck, unchanged at
   `CCX_DAMAGE_DEADALL=1.e-2`, to `theta=0.255574`.

Do not tune either mechanism merely because the next wall is difficult.

## Working discipline

- Work autonomously and follow measurements rather than the previous agents'
  preferred explanation.
- Before a material code change, state a falsifiable hypothesis and the one
  measurement that can reject it.
- Diagnostic instrumentation is welcome; remove noisy or unsafe probes from
  the final patch.
- Do not change physical parameters, the deck, convergence tolerances, solver,
  viscosity, tangent mode, `DEADALL`, or AUTOSPC in a causal A/B experiment.
- Use PARDISO for every quoted `s3rad` comparison. SPOOLES is suitable for
  small unit tests, not as a substitute for the recorded target.
- Keep binaries and generated solver output out of Git.
- Commit only evidence-backed milestones. If a hypothesis is rejected, revert
  its functional patch rather than stacking another workaround.

## Required regressions

Preserve feature-off behavior, all four current self-tests, the analytical
Mode-I and mixed-mode benchmarks, and the old-wall line-search A/B. The first
validation of a new fix must use stock/rescue with crack-control engagement
disabled.

## Delivery

Report separately: reproduced facts, new measurements, the actual cause,
functional changes, regressions, and whether `s3rad` completed or physically
separated. Passing one wall is not the same as completing the specimen.

If GitHub write access is unavailable, deliver a verified git bundle with its
base and head commits.
