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
