# ISO 1.0.0 — release status (2026-08-18)

## Done on this Mac, measured

| | |
|---|---|
| DSP suite (`IsoTests`) | 51 / 51, Release and ASan+UBSan |
| Host-contract suite (`IsoHostTests`) | 74 / 74, Release and ASan+UBSan |
| `auval -v aufx Iso1 Naam` | PASS |
| Universal binaries (VST3 / AU / Standalone) | x86_64 + arm64, verified with `lipo` |
| Bundle icon | `packaging/icon.png`, rendered by `IsoShot --icon` from the same Path code the header uses |
| macOS delivery | `dist/ISO-1.0.0-macOS.zip` — pkg (3 selectable components) + MANUAL + PARAMETER-TABLE + uninstaller; payload re-opened and checked |
| Git | local repository, commit tagged `v1.0.0`; no remote |

## Windows — built and verified by CI (run 32154924811, 2026-08-18)

| | |
|---|---|
| Compiler | Visual Studio 2022 / MSVC, x64, JUCE pinned `857aab9c` |
| DSP suite under MSVC | 51 / 51 |
| Host-contract suite under MSVC | 74 / 74 |
| Installer | `ISO-1.0.0-windows.exe` (Inno Setup, 4.7 MB) — installed AND uninstalled for real on the runner, payload checked on disk |
| Delivery | `dist/ISO-1.0.0-Windows-Setup.zip` — installer .exe, raw `ISO.vst3`, TRIX-shaped `INSTALL-ISO.bat` + `UNINSTALL-ISO.bat`, READ ME, MANUAL |

Three CI runs failed first, all on the same host-suite check: the allocation
counter was global to the process and charged JUCE's Windows message/timer
threads' background allocations to the audio path (random block sizes gave it
away). The counter is now thread-local — the audio thread is what the claim is
about — and the plug-in itself never changed.

## The two files to send

```
dist/ISO-1.0.0-macOS.zip           12 MB   pkg (VST3 / AU / app, selectable) + READ ME + MANUAL + uninstaller
dist/ISO-1.0.0-Windows-Setup.zip   6.8 MB  INSTALL-ISO.bat + ISO.vst3 + installer .exe + UNINSTALL + READ ME + MANUAL
```

Repository: https://github.com/gusanaaman1-boop/iso-plugin (public), tag `v1.0.0`.

## Not signed

Neither platform's binaries are code-signed or notarised — see `docs/SIGNING.md`
for exactly what that costs the user and what fixes it (paid certificates, so
not done, per the standing no-payments rule).
