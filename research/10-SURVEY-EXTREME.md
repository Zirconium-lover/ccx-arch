# 10. The near-failure regime: what other codes do

Survey, prompted by an observation that turned out to be right: the `s3rad`
specimen was effectively broken and nothing in this tree could say so.  The
measurement behind it is in `research/09-SEVERANCE.md` - **minimum cut 0.0031
against a nominal section of 5.7, or 0.05% of a cross-section**, while the
topological sweep the code actually runs says "connected" and is correct to
say it.

Note on sourcing, because the evidence discipline applies to a survey too:
the network egress in this environment blocked the primary PDFs and vendor
doc servers (`ftp.lstc.com`, `abaqusdocs.*`, `code-aster.org`,
`dynasupport.com`).  What is cited below is what could actually be read -
search-result summaries and the open-access papers - and where a claim rests
on a summary rather than a fetched primary page, it says so.  Anything a
decision is taken on should be re-checked against the vendor manual.

The run exposes three separate problems.  They get conflated here, and they
are solved differently everywhere else.

---

## A. "When is it over?"  Termination criteria

**Here.** Four switches, and all four are the same question: `CCX_FRACTURE_TERMINATION`
(which node sets), `CCX_FRACTURE_LINK` (node or face conduction),
`CCX_FRACTURE_DEADFACET` (do dead cohesive facets conduct), `CCX_DAMAGE_DEADALL`.
Every one of them is **topological** - is there a chain of surviving elements.
`09-SEVERANCE` shows all six combinations answer "connected" on a specimen
hanging by 0.05% of a section.

**Elsewhere, nobody asks a topology question.**  LS-DYNA's `*TERMINATION`
family stops on a *named scalar crossing a threshold*: `*TERMINATION_NODE`
watches a node's displacement, `*TERMINATION_BODY` a rigid body's motion,
`*TERMINATION_CURVE` an arbitrary curve, `*TERMINATION_DELETED_SOLIDS` a
count of eroded elements, `*TERMINATION_SENSOR` a sensor value.  Separately,
`*CONTROL_TERMINATION`'s `DTMIN` stops the run when the timestep collapses -
which is *exactly* how `s3rad` ended, at `dtime=1.46e-06` against
`tmin=1.00e-06`.  (Keyword names from the LS-DYNA Keyword User's Manual;
the manual PDF itself was not reachable from here, so treat the per-keyword
detail as needing a check.)

Abaqus has no separation detector either: the user is expected to `*MONITOR`
a degree of freedom, or to watch the stabilisation energy `ALLSD` against
total strain energy `ALLIE`, with the manual's own guidance being to keep
`ALLSD` below about 5% of `ALLIE`
([GoEngineer](https://www.goengineer.com/blog/stabilization-strategies-for-nonlinear-static-abaqus-models)).

**What to take.**  The industry answer to "is it over" is *a scalar the deck
author names*, not a graph algorithm - and our own min-cut measurement
explains why that is the right instinct.  Connectivity is a yes/no question;
failure is a quantity.  Two things follow, and they are cheap:

1. **A reaction-drop stop.**  One number in the deck: stop when the reaction
   on the loaded set falls below a stated fraction of its own peak.  On
   `s3rad` at 10% of peak that ends the run at increment 520 instead of 599 -
   **18.4% of the wall clock**, and it removes the twelve-firing rescue
   pile-up and the `tmin` error from the record entirely, because neither
   would happen.  This is `*TERMINATION_CURVE` with the curve being the grip
   reaction.
2. **Report the minimum cut, not the connectivity.**  It is computable at the
   same moment the current sweep runs - only when a deletion batch commits -
   and it turns "is this still a specimen" from a boolean into an area.  It
   also explains the thing that looked like luck: whether a model prints
   `[FRACTURE COMPLETE]` depends on whether the last thread happens to be
   *deleted* or merely damaged to a part in two thousand.  One element
   decides a boolean; nothing decides an area.  `loadpath.c` in the parallel
   line of work owns this question, so this is offered as a specification.

---

## B. "How do you get through the post-peak?"  The walls

Every wall in this tree is late, on nearly-destroyed models.  That is not a
coincidence and it is not special to this code: it is the standard hard
regime, and there are four established answers.

**1. Viscous regularisation of the damage.**  Already here -
`CCX_DAMAGE_VISCOSITY=1.e-4`, a Duvaut-Lions relaxation with
`beta = dtime/(eta+dtime)`.  This is the cheapest and most common industrial
fix and this tree uses it by default.  Nothing to adopt; worth saying because
it means the obvious first move is already made.

**2. Artificial viscous damping with an energy budget.**  Abaqus
`*STATIC, STABILIZE`.  The architecture is the interesting part, not the
damping: the damping factor is **not** tuned by a convergence heuristic, it
is controlled by the ratio of energy dissipated by the damping to the total
strain energy, with `ALLSDTOL` as the budget; adaptive stabilisation raises
the factor when convergence is poor and **lowers it when it is distorting the
solution**, and the user is handed `ALLSD/ALLIE` to check the answer was not
bought with fake energy
([GoEngineer](https://www.goengineer.com/blog/stabilization-strategies-for-nonlinear-static-abaqus-models),
[CAE University](http://caeuniversity.com/static-instabilities/)).

This is the single idea most worth stealing.  Six globalization mechanisms
here are controlled by convergence heuristics and none of them reports what
it cost the physics.  One number - *how much of this answer is artificial* -
with a default budget and a printed actual, would replace most of them and
would make the leave-one-out in `06-TANGENT-VERDICT` unnecessary, because
each mechanism would carry its own price tag.

**3. Dissipation-based path following.**  The established fix for snap-back,
where neither load nor displacement control works because both turn back.
Gutierrez (2004) derives an arc-length constraint on the **energy release
rate**; Verhoosel, Remmers and Gutierrez, *A dissipation-based arc-length
method for robust simulation of brittle and ductile failure*, IJNME 2009,
[doi:10.1002/nme.2447](https://onlinelibrary.wiley.com/doi/abs/10.1002/nme.2447),
makes the dissipation increment the control parameter - monotone through
snap-back by construction, because dissipation cannot decrease.  A more
recent open-access treatment for a different discretisation is
[arXiv:2007.12594](https://arxiv.org/pdf/2007.12594).

**This tree already implements it and does not use it.**  `CCX_PATHFOLLOW`
takes a dissipation `tau` per increment; `CCX_DISSIPATION_CONTROL` and
`CCX_DISSIPATION_TARGET` exist.  On `s3rad` all three are **unset** -
`run_s3rad.sh` explicitly `unset`s the dissipation pair.  So the standard
answer to the exact failure mode being hit is present in the binary and off
on the deck that needs it.  That is a measurement to make, not a feature to
write.

**4. Switch to explicit.**  The industrial default when a quasi-static
implicit run will not converge through failure: run the terminal phase as
explicit dynamics with mass scaling.  Abaqus documents Standard-to-Explicit
import for exactly this; the reasoning given is that explicit "solves certain
types of static problems more readily" and handles "material degradation and
failure, which often cause convergence difficulties in Standard"
([Abaqus Getting Started, ch.13](https://classes.engineering.wustl.edu/2009/spring/mase5513/abaqus/docs/v6.6/books/gsa/ch13.html)),
and the crack literature says the same - a pure implicit approach "will lead
to convergence problems whereas an explicit modelling strategy doesn't face
these limitations"
([EPJ Web of Conferences, DYMAT 2021](https://www.epj-conferences.org/articles/epjconf/pdf/2021/04/epjconf_dymat2021_02033.pdf)).

**Reject for this tree**, and say why rather than leaving it implicit:
CalculiX has an explicit path but not a Standard-to-Explicit state import,
the damage state lives in `xstate` arrays the explicit path does not carry,
and the cost of building that bridge is a project, not a refactor.  Worth
naming because it is what the rest of the world does, and because it reframes
the six mechanisms: they are an attempt to do implicitly what industry does
by changing integrator.

---

## C. "Why does it hang by one thread?"  Mesh objectivity

A minimum cut of 0.05% of a section is the signature of damage localising
into a single band of elements.  With a local softening law that is not a
physical result - it is the well-known pathology: the dissipated energy goes
to zero with the element size, so the answer depends on the mesh.

The established fixes, in the order they are usually tried:

- **Crack band** (Bazant): put the element characteristic length into the
  softening law so the dissipated energy per unit crack area is mesh
  independent.  Cheapest; local; needs only the element size.
- **Integral nonlocal damage** (Bazant & Pijaudier-Cabot): the damage driver
  at a point is a weighted average over a neighbourhood, which introduces a
  real length scale.
- **Gradient-enhanced damage**: the same length scale through an extra PDE
  for a nonlocal variable, which is better conditioned than the integral form
  in a finite element setting.

Survey summary and the classification into integral and gradient families:
[Nonlocal Damage Theory, Bazant & Pijaudier-Cabot](https://www.researchgate.net/publication/245284392_Nonlocal_Damage_Theory);
a recent open-access implementation study is
[arXiv:2103.04770](https://arxiv.org/pdf/2103.04770).

**Defer, but measure first.**  Any of these changes the physics and needs a
length scale the deck does not currently carry, so none is a refactor.  What
is free is the diagnosis: run `s3rad` at two mesh densities and compare the
dissipated energy and the min-cut.  If they track the element size, the walls
are a modelling artefact and no amount of solver work fixes them; if they do
not, localisation is not the story.  That measurement has never been made
here and it is one deck variant.

---

## What this says about the six mechanisms

`09-STEERING` asks which globalization mechanisms are scar tissue.  The survey
sharpens the question rather than answering it.

Elsewhere, the near-failure regime is handled by **three** things: a damping
scheme with an energy budget, a dissipation-controlled path follower, and a
change of integrator.  This tree has the first in a weaker form (viscous
regularisation with no budget and no report), has the second and does not arm
it, and cannot do the third.  In place of them it has six mechanisms tuned by
convergence heuristics.

So the ranked recommendation, cheapest first:

| rank | what | why it is first | cost |
|---|---|---|---|
| 1 | reaction-drop termination | 18.4% of `s3rad` wall clock, and it deletes the failure mode rather than surviving it | one scalar, one comparison |
| 2 | arm `CCX_PATHFOLLOW` on `s3rad` and measure | the established answer to this exact failure is already compiled in and switched off | one run |
| 3 | min-cut instead of connectivity in the termination report | turns "is this a specimen" from a boolean decided by one element into an area | one max-flow at commit time |
| 4 | an energy budget for the globalization, Abaqus `ALLSDTOL` style | would let every mechanism state its price, which is what §5 is really asking for | a real piece of work |
| 5 | mesh-density study for localisation | decides whether the walls are physics or discretisation | two runs |
| 6 | nonlocal or crack-band regularisation | the actual fix if 5 says discretisation | a project |

Sources:
- [Stabilization Strategies for Nonlinear Static Abaqus Models, GoEngineer](https://www.goengineer.com/blog/stabilization-strategies-for-nonlinear-static-abaqus-models)
- [How to solve problems with static instabilities in Abaqus, CAE University](http://caeuniversity.com/static-instabilities/)
- [Quasi-Static Analysis with Abaqus/Explicit, Getting Started ch.13 (v6.6)](https://classes.engineering.wustl.edu/2009/spring/mase5513/abaqus/docs/v6.6/books/gsa/ch13.html)
- [Verhoosel, Remmers & Gutierrez, IJNME 2009, doi:10.1002/nme.2447](https://onlinelibrary.wiley.com/doi/abs/10.1002/nme.2447)
- [Direct dissipation-based arc-length approach, arXiv:2007.12594](https://arxiv.org/pdf/2007.12594)
- [Numerical modelling strategies using implicit and explicit solvers, EPJ Web Conf. DYMAT 2021](https://www.epj-conferences.org/articles/epjconf/pdf/2021/04/epjconf_dymat2021_02033.pdf)
- [Bazant & Pijaudier-Cabot, Nonlocal Damage Theory](https://www.researchgate.net/publication/245284392_Nonlocal_Damage_Theory)
- [An FFT framework for simulating non-local ductile failure, arXiv:2103.04770](https://arxiv.org/pdf/2103.04770)
- [Code_Aster STAT_NON_LINE / PILOTAGE (PRED_ELAS, DEFORMATION, LONG_ARC)](https://code-aster.org/V2/doc/v11/en/man_u/u4/u4.51.03.pdf) - not fetchable from here; the PILOTAGE types and their purpose for softening laws are from the search summary
- [LS-DYNA Keyword User's Manual](https://lsdyna.ansys.com/wp-content/uploads/2025/02/ls-dyna_950_manual_k.pdf) - the `*TERMINATION` family; manual not fetchable from here
