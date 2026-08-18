# ISO — DJ isolator EQ

Three-band Linkwitz-Riley isolator with kills, definable crossovers, DJM-style
ISO / EQ cut law, and a bipolar sweep filter. VST3 / AU / Standalone (JUCE 9).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
./build/IsoTests_artefacts/Release/IsoTests          # 50 measurements
./build/IsoShot_artefacts/Release/IsoShot ui-shots   # deterministic UI frames
```

* `docs/RESEARCH.md` — what Bitwig EQ-DJ, Serum, Pioneer DJM, Xone:92 and rotary isolators do, and what ISO took from each.
* `docs/SPEC.md` — signal path, controls, laws, verification.
* `docs/PARAMETER-TABLE.md` — generated from the plug-in.
