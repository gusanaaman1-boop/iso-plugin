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

## Prepared, NOT verified (needs a Windows compiler)

* `packaging/INSTALL-ISO.bat` — one-file build-and-install for a Windows desktop.
* `packaging/ISO.iss` + `.github/workflows/windows.yml` — CI: MSVC build, both
  suites, Inno installer, install + uninstall on the runner, artefacts.
* `packaging/make-packages.sh` (without `--mac-only`) — the two-file delivery,
  once `packaging/fetch-windows-ci.sh` has pulled a green Windows run.

## The one step that is yours

CI needs the repository on GitHub, **public** (hosted Actions are billing-blocked
on this account's private repos since 2026-08-06; EDGE went public and builds
green). From `Make Music/Iso`:

```
gh repo create iso-plugin --public --source=. --push
git push origin v1.0.0
```

The `v1.0.0` tag push starts the Windows workflow; when it is green,
`packaging/fetch-windows-ci.sh` then `packaging/make-packages.sh` produce the
two delivery zips.

## Not signed

Neither platform's binaries are code-signed or notarised — see `docs/SIGNING.md`
for exactly what that costs the user and what fixes it (paid certificates, so
not done, per the standing no-payments rule).
