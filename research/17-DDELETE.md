# Lowering the deletion threshold: measured on both decks, and it is worse

*2026-09-13.  Binary `6d752beb` plus the declaration commit; deck as
committed; `OMP=MKL=2` on the fast sweep, `OMP=MKL=6` on the target deck.*

The question was whether `Ddelete = 0.999` is too strict — an element must
reach damage 0.999 before it leaves the assembly.  It was asked with a
reference to Abaqus practice.  Two things came out of measuring it.

## What Abaqus actually does, as far as I could check

For progressive damage in ductile metals Abaqus bounds the overall damage
variable `D` and deletes the element when the bound is reached.  The
documentation's default bound is **1.0 when element deletion is on** (and
always 1.0 for cohesive elements); **0.99 is the default when deletion is
off**, i.e. for an element that stays in the model at its degraded
stiffness.  `*SECTION CONTROLS, MAX DEGRADATION=` lowers it deliberately to
make the deletion condition less strict.

So for the deletion case our 0.999 is, if anything, *less* strict than the
Abaqus default of 1.0 — not more.

**Provenance caveat, stated because the discipline requires it.**  Both
documentation hosts are blocked by this environment's egress proxy, so the
above is from a search engine's summary of those pages, not from the pages
themselves.  Pages summarised:
[v6.9ef 21.2.3](http://abaqusdocs.eait.uq.edu.au/v6.9ef/books/usb/pt05ch21s02abm42.html),
[v6.11 23.2.3](http://abaqusdocs.eait.uq.edu.au/v6.11/books/usb/pt05ch23s02abm42.html),
[v6.6 19.2.3](https://classes.engineering.wustl.edu/2009/spring/mase5513/abaqus/docs/v6.6/books/usb/pt05ch19s02abm39.html).
Treat as second-hand until somebody opens them.

## Fast deck: the outcome is not monotone, and the good band is a phantom

Eleven values on `fast-wrapped`:

| `Ddelete` | outcome |
|---|---|
| 0.9990, 0.9980, 0.9970, 0.9950 | `rc=201` at theta ≈ 0.158 |
| **0.9920, 0.9900, 0.9880, 0.9850** | **`rc=0` at theta 1.0** |
| 0.9800, 0.9700, 0.9600 | `rc=201` at theta ≈ 0.157 |

A band four values wide where the wall disappears, with walls on both sides
of it.  Tempting.  It is not a fix.  Re-run inside the band with
`CCX_FRACTURE_CUT_EXACT` armed:

```
[LOADCUT] inc=56   cut=2.555556e-04 ratio=2.937713e-04 conn=1
[LOADCUT] inc=101  cut=2.500000e-04 ratio=2.873850e-04 conn=1
   ... the run continues to increment 539, theta = 1.0, rc=0
```

The specimen separates at increment 56 and the solver applies load for 480
more increments before reporting success at full load.  Lowering the
threshold does not make the specimen break.  It makes the solver stop
noticing that it already has — the phantom regime of `05-DEBT.md` item 1.

## Target deck: it kills the run just past peak load

`CCX_DAMAGE_DELETE_D=0.99` with `CCX_FRACTURE_CUT=1e-4` armed, against the
same-binary reference `cut6`:

| | `cut6` (`Ddelete` 0.999) | `dd099` (`Ddelete` 0.99) |
|---|---|---|
| ending | `FRACTURE COMPLETE`, inc 667 | `rc=201`, inc 168 |
| theta | 0.258750 | **0.191431** |
| grip reaction | 45.5 = **1.46 %** of peak | 2969 = **95.39 %** of peak |
| elements deleted | 3749 | **278** |

The peak is at theta 0.1575 in both.  So the lowered threshold kills the run
essentially at the top of the curve, before the specimen has softened at
all, with 7 % of the deletions.

**And the mechanism is the one already written at the trigger**, in the
comment above the `usevisc` line in `damage_de13_mark_terminal`:

> Triggering deletion on D therefore removes an element that is still
> carrying `1-Dvis` of its effective stress, and releases that force in one
> increment at constant load.

At `Ddelete = 0.99` every removed element dumps one percent of its effective
stress at once; the batch prints confirm it (`batch_Dvis = 0.9900`,
`0.9901`).  And the wall has a different signature from the trap: the
residual **diverges** — 7.23, 7.15, 16.93, 13.95 over the last four
iterations — where the trap's residual creeps at 0.57.  Two different
failure modes, and this one is a shock, not a stall.

## Verdict

`Ddelete = 0.999` is not the thing to change.  On one deck lowering it buys
a phantom; on the other it costs the run 26 % of the load it otherwise
reaches.  The number is not a knob for "when the specimen breaks" — it is a
knob for **how much force each deletion dumps**, and the default is where it
is for a measured reason.

The adjacent lever was tested too: `CCX_DAMAGE_DELETE_VISC=0`, judging the
trigger on the instantaneous damage instead of the viscous one, does not
clear the wrapped wall either (`rc=201`, theta 0.1575, minimum cut already
at 2.75e-04).

What *does* change the outcome remains `CCX_FRACTURE_CUT`: it ends the run
on a measurement of the load path instead of on the solver giving up, and it
is the switch that turns both of this note's failure modes into a reported
separation.
