# ISO — manual

**A three-band DJ isolator EQ.** The EQ section of a club mixer — big band knobs,
kills, definable crossovers, and the sweep filter — as a plug-in, with the
crossover maths of a rotary isolator underneath.

VST3 · Audio Unit · Standalone. macOS universal (Apple Silicon + Intel), Windows x64.

---

## The idea in one paragraph

The signal is split into LOW, MID and HIGH by a Linkwitz-Riley crossover. Each
band has its own gain — all the way down to silence — and the three are summed
back. With every knob at 0 dB the sum is *exactly* flat (measured 0.000 dB) and
the plug-in adds no latency, so ISO can sit on a channel doing nothing until you
reach for it. Turn a knob down and that region of the spectrum goes away
cleanly, with no bump and no ringing at the seams; hit KILL and it is gone.

---

## Controls

### The three band knobs — LOW · MID · HIGH
Each band's gain, **−30 dB … +12 dB**, 0 dB at about 70 % of the travel so most
of the knob is *cut*, where DJs live. Double-click returns to 0 dB. Typing into
the read-out works: `+3`, `-12`, `KILL`.

What the bottom of the knob means depends on the **CUT** switch (below).

### KILL ×3
A latching pad under each band. Lit = that band is silent. It is a 6 ms fade to
exactly zero — instant to the ear, click-free on the meter. Kills are ordinary
parameters, so they automate and MIDI-map like anything else.

### LOW / MID and MID / HIGH — the crossovers
Where the bands meet. LOW/MID **60 Hz – 1 kHz** (default 250 Hz), MID/HIGH
**800 Hz – 10 kHz** (default 2.5 kHz). They can also be dragged straight on the
graph — grab the small handle at the top of either dashed line. The two can never
cross: LOW/MID is held at least 1.5× below MID/HIGH.

The defaults are the Xone:92's corners. Presets carry the others (DJM-ish 180 Hz
/ 3 kHz, rotary isolator 300 / 3k, bass-focus 100 / 1.5k, wide-mid 120 / 6k).

### SLOPE — 12 / 24
* **24 dB/oct** — Linkwitz-Riley 4th order, the standard in rotary-mixer
  isolators (Rane, Alpha Recording, E&S). Steeper, more surgical kills.
* **12 dB/oct** — Linkwitz-Riley 2nd order, the Xone:92's law. Gentler skirts,
  bands blend more; the mid band is polarity-inverted internally, exactly as on
  the hardware, and the sum is still flat.

Both slopes sum to a flat all-pass at unity. Switch freely.

### CUT — ISO / EQ
What the bottom of a band knob does. This is the Pioneer DJM's "EQ curve /
ISOLATOR curve" switch:
* **ISO** — the last part of the travel fades to **silence**. Knob-at-bottom is a
  kill. Read-out says `KILL`.
* **EQ** — the knob floors at **−26 dB**, the DJM EQ curve. Read-out says
  `-26.0 dB`. The band never fully disappears — useful when you want the
  "EQ'd out" sound rather than the "isolated out" sound.

### FILTER + RES
The colour/sweep filter every modern DJ mixer has, after the EQ:
* Centre = **OFF** (a ±4 % dead zone, so it does not engage by accident).
* Turn **left** = **low-pass**, sweeping from 20 kHz open down to 60 Hz.
* Turn **right** = **high-pass**, sweeping from 20 Hz open up to 12 kHz.
* **RES** 0–100 % sets the resonance (Q 0.7 → 6). It fades in over the first
  fifth of the travel out of centre, so a high RES cannot leave a peak ringing at
  20 kHz when the knob is only just off OFF.

12 dB/oct, state-variable, safe at any resonance. Double-click FILTER = OFF.

### OUTPUT
±12 dB trim, after everything. The two-bar meter beside it shows the output
peak (−60 … +6 dB).

### BYPASS
Bit-exact pass-through.

### Presets
Host programs (Cubase's own preset menu lists them). A preset sets the
**character** — crossovers, slope, cut law, resonance — and leaves your
**performance** — gains, kills, filter position — exactly where they are, so
switching character mid-set does not un-kill anything.

| # | Preset | corners | slope | cut |
|---|---|---|---|---|
| 0 | Init — Isolator 24 dB | 250 / 2.5k | 24 | ISO |
| 1 | Xone:92 style 12 dB | 250 / 2.5k | 12 | ISO |
| 2 | DJM EQ curve (−26 dB) | 180 / 3k | 24 | EQ |
| 3 | DJM ISO curve | 180 / 3k | 24 | ISO |
| 4 | Rotary isolator 300 / 3k | 300 / 3k | 24 | ISO |
| 5 | Bass focus 100 / 1.5k | 100 / 1.5k | 24 | ISO |
| 6 | Wide mids 120 / 6k | 120 / 6k | 24 | ISO |

---

## The graph

The response of the exact filters that are running — not a sketch. Each band's
own contribution is filled in its colour (ember = LOW, gold = MID, ice = HIGH);
a killed band goes dim. The white line is the total, including the sweep filter
and OUTPUT — what your ears get. Drag the crossover handles at the top.

## Window size
Drag the corner. 60 % – 200 %, aspect locked; the size is saved with the
session.

---

## Tips
* **Bass swap**: LOW KILL on the outgoing deck, hold, un-kill on the incoming.
  With SLOPE 24 the low band is gone below the crossover with almost nothing
  left; with SLOPE 12 a little more of the low-mids stays.
* **Isolate the vocal**: KILL LOW and HIGH, drag the crossovers in around the
  voice.
* **Rise**: FILTER right (HP), RES around 60 %, sweep up; drop with LOW back in.
* Load "DJM EQ curve" when you want cuts that leave a ghost of the band rather
  than a hole.

## What is measured
Every claim above is a printed number in the two test suites that ship with the
source: `IsoTests` (51 DSP checks: flat sum, corner points, slopes, kills, gain
accuracy, filter law, click-free kill, allocation-free processing at 44.1–192 kHz)
and `IsoHostTests` (74 host-contract checks: parameters, bus layouts, state,
presets, editor). See `docs/SPEC.md`.
