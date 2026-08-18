# ISO — DJ isolator EQ

Three-band Linkwitz-Riley isolator with kills, definable crossovers, DJM-style
ISO / EQ cut law, and a bipolar sweep filter. VST3 / AU / Standalone (JUCE 9).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
./build/IsoTests_artefacts/Release/IsoTests            # 51 DSP measurements
./build/IsoHostTests_artefacts/Release/IsoHostTests    # 74 host-contract checks
./build/IsoShot_artefacts/Release/IsoShot ui-shots     # deterministic UI frames
packaging/make-packages.sh --mac-only                  # dist/ISO-<ver>-macOS.zip
```

Windows: `packaging/INSTALL-ISO.bat` on a machine with Visual Studio 2022, or the
GitHub Actions workflow in `.github/workflows/windows.yml` (see `docs/CI-WINDOWS.md`).

* `docs/RESEARCH.md` — what Bitwig EQ-DJ, Serum, Pioneer DJM, Xone:92 and rotary isolators do, and what ISO took from each.
* `docs/SPEC.md` — signal path, controls, laws, verification.
* `docs/PARAMETER-TABLE.md` — generated from the plug-in.
* `docs/MANUAL.md` — the user manual.
* `docs/RELEASE-STATUS.md` — what is verified, what is prepared, what is yours.
