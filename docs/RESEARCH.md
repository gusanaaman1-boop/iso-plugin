# What a "DJ EQ" actually is — research notes (2026-08-18)

Before writing a line of DSP, this is what the reference products do. ISO is
built from these numbers, and the presets are named after them.

## 1. Bitwig **EQ-DJ**

Bitwig ships a dedicated *EQ-DJ* device next to EQ-2 / EQ-5 / EQ+. The user
guide describes it in one sentence: *"A three-band equalizer with definable
crossover frequencies and kill switches for each band."* Reviews call it a
"kill filter": three bands that can be freely muted, with the two crossover
points as user parameters. That is a **crossover-based isolator**, not a
shelf/peak EQ — which is exactly the topology ISO uses.

Sources: [Bitwig user guide — EQ](https://www.bitwig.com/userguide/latest/eq/),
[Bitwig EQs overview](https://www.bitwig.com/bitwig-eqs/),
[Admiral Bumblebee Bitwig effects review](https://www.admiralbumblebee.com/music/2017/06/27/bitwig-effects-review).

## 2. Serum 2 — EQ effect

Serum's EQ is a compact **two-band parametric** whose bands switch between
Shelf / Peak / Filter with Frequency, Q and Gain. Good for tone shaping inside
a synth patch, but it is not a DJ EQ: no crossovers, no kills, and a two-band
shape. Nothing here was borrowed except the idea that the display must show
the *summed* curve, not the individual bands.

Sources: [MusicRadar — Serum effects guide](https://www.musicradar.com/how-to/a-quick-guide-to-xfer-records-serums-effects),
[Serum 2 on KVR](https://www.kvraudio.com/product/serum-2-by-xfer-records).

## 3. Pioneer DJM mixers (the club standard)

* 3-band channel EQ, switchable curve: **EQ mode −26 dB … +6 dB**, **ISOLATOR
  mode −∞ … +6 dB**. The two modes are the same knobs with a different floor.
* Corner regions roughly LOW ≈ 70–100 Hz shelf, MID ≈ 1 kHz peak, HI ≈ 13 kHz
  shelf on the mixer EQ; the isolator curve is steeper and full-cut.

ISO's **CUT** switch (ISO / EQ) is this exact distinction: same knob, floor at
silence or at −26 dB.

Sources: [Pioneer DJ forum — EQ curve difference](https://forums.pioneerdj.com/hc/en-us/community/posts/203036079--SOLVED-What-is-the-difference-between-the-EQ-curves-),
[Pioneer DJ forum — kill switch](https://forums.pioneerdj.com/hc/en-us/community/posts/206014863-EQ-Kill-Switch-on-Rekordbox-DJ).

## 4. Allen & Heath **Xone:92**

* 4-band: +6 dB / **infinite cut at 12 dB/oct on LF and HF**, −30 dB on the
  two mids; corners at **250 Hz, 350 Hz, 2 kHz, 2.5 kHz**.
* Mid bands: wide cut, narrow boost, so all knobs at max does not pile up gain.

ISO's default corners **250 Hz / 2.5 kHz** and its **12 dB/oct** slope option
come from here (the *Xone:92 style* preset).

Sources: [Allen & Heath Xone:92](https://www.allen-heath.com/hardware/xone-series/xone92/),
[Xone:92 LE user guide (PDF)](https://www.allen-heath.com/content/uploads/2024/08/Xone92-Limited-Edition-User-Guide.pdf),
[Mix — Xone:92 / V6 review](https://www.mixonline.com/recording/allen-heath-xone92xonev6-375941).

## 5. Rotary-mixer isolators (Alpha Recording, E&S, Rane MP2015, Bozak, Vestax…)

* Standalone 3-band units with **big knobs**, wider frequency ranges than a
  mixer EQ (E&S X3004 bass band 10–300 Hz vs the DJM's ~10–100 Hz), and more
  gain per band (often +10 dB) with full kill.
* The industry-standard filter is a **Linkwitz-Riley 4th-order (24 dB/oct)**
  crossover: the three bands sum flat and phase-aligned at the corners, so with
  everything at 0 dB the unit is transparent. Rane's MP2015 describes its
  master isolator as a "24 dB/octave phase-compensated Linkwitz-Riley design
  with continuously variable crossover points".

That is ISO's default mode (24 dB/oct, LR4) and the *Rotary isolator 300 / 3k*
preset.

Sources: [DJ TechTools — mixing with DJ isolators](https://djtechtools.com/2011/12/11/an-introduction-to-mixing-with-dj-isolator-mixers/),
[Linkwitz–Riley filter (Wikipedia)](https://en.wikipedia.org/wiki/Linkwitz%E2%80%93Riley_filter),
[ESP Project 153 — isolator equaliser](https://sound-au.com/project153.htm),
[Rane MP2015 spec listing](https://www.prosoundgear.com/shop/bundles/dj-bundles/rane-dj-mp2015-rotary-dj-mixer-2-krk-rokit-8-g3-rp8g3-8-powered-studio-monitor-speakers-monitor-stands-package/).

## 6. The colour/sweep filter

Every modern DJ mixer also has a **bipolar filter knob**: centre = off, turn
left = low-pass sweeping down, turn right = high-pass sweeping up, with a
resonance control. It sits after the EQ. ISO includes it as FILTER + RES,
because in practice a DJ EQ without one is half a channel strip.

## What ISO takes from all this

| Feature | Taken from | ISO |
|---|---|---|
| 3 bands, definable crossovers, kill per band | Bitwig EQ-DJ | LOW/MID/HIGH, LOW/MID + MID/HIGH knobs (also draggable on the graph), KILL pads |
| LR4 24 dB/oct, flat sum | Rotary isolators, Rane | default SLOPE 24 |
| 12 dB/oct infinite cut, 250 / 2.5k | Xone:92 | SLOPE 12, default corners |
| −26 dB EQ curve vs −∞ ISO curve | Pioneer DJM | CUT switch ISO / EQ |
| +6 … +10 dB boost | DJM / isolators | knob to +12 dB, 0 dB at 70 % of travel |
| Bipolar LP/HP sweep with resonance | DJM colour FX / Xone VCF | FILTER + RES |
| Big knobs, kills you can hit in the dark | every isolator | 120 px band knobs, lamp-style KILL pads |
