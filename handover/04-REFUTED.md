# 4. Measured and rejected

Every entry is a run somebody does not have to spend again. Add to it in the
same form: what was tried, and the measurement that killed it.

## About the walls

| tried | measurement that rejected it |
|---|---|
| a Newton–Krylov corrector for the second wall | stops **earlier**, `theta=0.191828`, worse than no fix. It led in `theta` at every increment from 91 to 141 and still lost the run — **being ahead early is not evidence** |
| event truncation (stop the step at the first active-set crossing) | rejected from the eps ladder before being built: the first crossing buys 0.64% and the best rung 1.03%, against the ladder's own 0.4% |
| projecting the step onto the non-collapsed nodes | 6.7x on `|R|2`, nothing on `|R|inf` — and `|R|inf` is what is judged |
| a smaller increment | the wall's residual is **not proportional to the step**: `dtheta` collapsed 48x while the inherited residual grew from 15.7% to 105.7% of tolerance |
| global snap-back as the cause | `qa` declines smoothly to 58.3% of peak; no snap-back |
| contact chatter as the cause of the *second* wall | `ncomp=0` through increments 553–555 — no UC6 point in compression at all |
| a "node with at most one live bulk element" gate | node 3053 at the third wall has **four** live bulk elements and no facets. The signature was the same and the mechanism was not |
| a stiffness-ratio threshold tuned until a node is included | 76 nodes below 1e-3, 166 below 1e-2, 381 below 1e-1. Tuning among them is the chain of thresholds this project exists to avoid |
| the crack-face regulariser, **on `s3rad`** | moves the wall 28 increments, 0.10% of load factor — and the whole difference is inside the post-severance regime. It works on the fast deck and does nothing here |

## About where the time goes

| tried | measurement that rejected it |
|---|---|
| **holding the sparsity pattern fixed under erosion so the symbolic factorisation happens once** (`08-OBJECT-MODEL.md` §4, first candidate) | it already effectively does, and it buys nothing. `CCX_PARDISO_REUSE_SYMBOLIC=1` retains the analysis for **603 of 606** factorisations on `fast-wrapped`, and the factorisation cost is **11.649 s against 11.380/11.480/11.528 s** over three stock control runs - at the top of the noise, not below it. The arms are bit-identical, so this is a cost measurement and not a trajectory comparison. `research/01-PROFILING.md` |
| **the line-search ladder as a runtime problem** | it consumes 367 of 973 residual evaluations on `fast-wrapped` - 38% of them - but residual evaluation is **12.0%** of the run, so the whole globalization apparatus is about **4.6%**. Deleting mechanisms remains an argument about comprehensibility; it is not an argument about speed |

## About the operator, and about how it was measured

| tried | measurement that rejected it |
|---|---|
| **a plateau of the CENTRAL difference over `h` as evidence that a tangent is wrong** | it is not evidence. At a kink the central difference converges to the MEAN of the one-sided derivatives - a stable converged value, different from both branches, with a plateau indistinguishable from a wrong tangent's. Take the one-sided differences separately: `research/03-OPERATOR.md` |
| **attributing the process-zone tangent error to `CCX_DAMAGE_TANGENT=UNSYM`** | the same state on the stock symmetric path gives **151 wrong coefficients against 157**, and `9.622e-02` against `9.573e-02`. It is not the mode; it is the bulk progressive-damage tangent in both modes |
| **the cohesive tangent as a suspect** | exact: 30906 of 30906 coefficients agree, 2.3e-07 to 8.6e-06 relative, and on the closure benchmark 3.2e-14 with a textbook `h^2` slope |
| **"the cohesive tangent is wrong by 1.0 of the column scale"** (an earlier run of this same probe) | a measurement bug, not a finding: the coefficient reader assumed `au` always carries an upper triangle at offset `nzs[2]`, and on the stock SYMMETRIC path it read past the end of the array. Caught by a self test written against a hand-built matrix, which then caught the first fix as wrong too |

## About the model's structure

| tried | measurement that rejected it |
|---|---|
| a free-fragment / connected-component unit (the `*STATIC, STABILIZE` class) | at **every** facet-stiffness threshold, **0 elements adrift** in `s3rad`. The class does not occur here. Build it against a measured failure, not against the name |
| viscous stabilization, by extension | same measurement. It would have been a seventh globalization mechanism beside six existing ones, for a failure this model does not have |

## About the test decks

| tried | measurement that rejected it |
|---|---|
| a full-section cohesive plane | fails first, the bulk unloads and never damages, `theta=1` with **zero deletions** |
| bigger meshes, to make the fast deck wall | four variants, none walls, none masks a node |
| moving the brittle band behind the crack tip | worst diagonal ratio `1.0405e-01`, 90 deletions — **identical** to no change |
| a small (2x1x2) wrapped inclusion | **0 of its 24 elements erode**: the wrap debonds at `Tn0=300` before the phase yields at 600 |
| a wrap **stronger** than the phase it wraps | the phase erodes, 11 nodes lose their bulk — and are then held by **intact** facets, flooring the ratio at 2.5e-2 |
| the same, stiffer wrap (`Kn=2e6`) | worse: 1.4e-01 |
| the same, weaker wrap (`Tn0=800`) | worse: 1.9e-01, only 36 deletions |
| a wrapped block spanning the **full** width | `theta=1`, worst 2.97e-01: it is the only load path, so it debonds wholesale and unloads |

## Corrections to earlier conclusions, kept because the reasoning recurs

- **"The residual peaks at node 1244."** No — the *correction* peaks there.
  Both statements are true at different states (increment start vs iteration
  8) and conflating them wasted a run.
- **"The census shows N transitions relative to the base state."** No — it is
  referenced to the FULL step. Misreading the direction inverted a conclusion.
- **"DEADFACET changed the trajectory."** No — its code is read-only.
  Discriminating properly showed **thread count** was the cause.
- **"The fast deck validated the refactor."** No — it never masked a single
  node, so the decision branch was never exercised. The threshold is clamped
  at `1.e-1` in the source and that deck's worst node reaches `1.0406e-01`,
  missing it by 4%.
- **"The regulariser was rejected by `s3rad`."** Also no, and this is the
  subtler one: the stopping point was compared, but the stopping point is in
  the phantom regime. Compared where the specimen still exists — severance
  `theta` — the two arms agree to **0.19%**, so the run *confirmed the change
  is harmless at full scale*. **Compare arms at a physically meaningful
  event, not at whichever increment the solver happened to give up on.**
