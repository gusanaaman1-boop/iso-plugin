#!/usr/bin/env bash
#
# Builds ISO's entire delivery: exactly TWO files, one per platform.
#
#   dist/ISO-<ver>-macOS.zip           installer + manual + uninstaller
#   dist/ISO-<ver>-Windows-Setup.zip   installer + raw VST3 + read me
#
# Two files and no decisions. Anything a person has to choose between before
# they can install a plug-in is a chance to choose wrong.
#
# The Windows half is NOT built here - there is no Windows compiler on this
# Mac. GitHub Actions builds it with Visual Studio 2022 and runs all 125
# measurement checks with MSVC before packing anything. Fetch it first:
#
#     packaging/fetch-windows-ci.sh
#
# Everything below is verified after it is produced, by opening it again and
# looking inside. A package that builds is not a package that contains
# anything.

set -euo pipefail

cd "$(dirname "$0")/.."
ROOT="$PWD"
DIST="$ROOT/dist"
WINCI="$ROOT/build-win-ci"

# One source of truth for the version - the same line CMake and the Windows
# installer read. A delivery whose halves disagree about their own version is
# a support call nobody can diagnose from the outside.
VERSION=$(grep -E '^project\(Iso VERSION' CMakeLists.txt | awk '{print $3}')
[ -n "$VERSION" ] || { echo "could not read the version from CMakeLists.txt"; exit 1; }

echo "ISO $VERSION - full delivery"

# The Windows bundle is produced from committed code, so uncommitted changes
# would be in the macOS half and absent from the Windows half.
if ! git diff-index --quiet HEAD -- 2>/dev/null; then
    echo
    echo "REFUSING: the working tree has uncommitted changes."
    git status --short
    exit 1
fi

MAC_ONLY=0
[ "${1:-}" = "--mac-only" ] && MAC_ONLY=1

# --- the Windows half must already be here -----------------------------------
WIN_EXE=$(find "$WINCI" -name "ISO-*-windows.exe" 2>/dev/null | head -1 || true)
WIN_VST3=$(find "$WINCI" -type d -name "*windows-VST3" 2>/dev/null | head -1 || true)

if [ "$MAC_ONLY" = "0" ] && { [ -z "$WIN_EXE" ] || [ -z "$WIN_VST3" ]; }; then
    echo
    echo "REFUSING: no Windows binaries in build-win-ci/."
    echo "Run packaging/fetch-windows-ci.sh first - it downloads them from the"
    echo "last green CI run and checks that run built this exact commit."
    exit 1
fi

rm -rf "$DIST"
mkdir -p "$DIST"

# =============================================================================
#  macOS
# =============================================================================
echo
echo "== macOS: universal build =="
cmake -B build-universal -DCMAKE_BUILD_TYPE=Release -DISO_COPY_AFTER_BUILD=OFF >/dev/null
cmake --build build-universal --target Iso_All -j8 >/dev/null

ART="$ROOT/build-universal/Iso_artefacts/Release"
for f in "VST3/ISO.vst3" "AU/ISO.component" "Standalone/ISO.app"; do
    [ -e "$ART/$f" ] || { echo "MISSING: $ART/$f"; exit 1; }
done

# Every slice must be there. A "universal" build that is arm64 only looks
# identical until someone opens it under Rosetta.
for f in "VST3/ISO.vst3" "AU/ISO.component" "Standalone/ISO.app"; do
    archs=$(lipo -archs "$ART/$f/Contents/MacOS/ISO")
    echo "  $(basename "$f"): $archs"
    case "$archs" in
        *x86_64*arm64*|*arm64*x86_64*) ;;
        *) echo "NOT UNIVERSAL: $f ($archs)"; exit 1;;
    esac
done

# The parameter table is generated FROM the binary being shipped. A table
# copied from the repository could describe a different build.
cmake --build build-universal --target IsoShot -j8 >/dev/null
"$ROOT/build-universal/IsoShot_artefacts/Release/IsoShot" --params docs/PARAMETER-TABLE.md >/dev/null

echo
echo "== macOS: installer =="
"$ROOT/packaging/make-installer-macos.sh" "$VERSION" >/dev/null
[ -f "$DIST/ISO-$VERSION.pkg" ] || { echo "the installer was not produced"; exit 1; }

MSTAGE="$DIST/stage-mac"
mkdir -p "$MSTAGE"
mv "$DIST/ISO-$VERSION.pkg" "$MSTAGE/ISO-$VERSION-macOS.pkg"
cp "$ROOT/docs/MANUAL.md"          "$MSTAGE/"
cp "$ROOT/docs/PARAMETER-TABLE.md" "$MSTAGE/"

# A double-clickable uninstaller. The shell script works, but nobody opens
# Terminal to remove a plug-in, and .command opens on double-click.
cat > "$MSTAGE/Uninstall ISO.command" <<'CMD'
#!/usr/bin/env bash
#  Removes ISO from this Mac. Double-click me.
cd "$(dirname "$0")"
echo
echo "This removes ISO - the VST3, the Audio Unit and the standalone app."
echo "Your presets and window size are not touched."
echo
read -r -p "Remove ISO? [y/N] " a
case "$a" in
    [yY]*) ;;
    *) echo "Nothing was removed."; exit 0 ;;
esac
echo
echo "Your Mac password may be asked for - the plug-ins live in /Library."
for t in "/Library/Audio/Plug-Ins/VST3/ISO.vst3" \
         "/Library/Audio/Plug-Ins/Components/ISO.component" \
         "/Applications/ISO.app" \
         "$HOME/Library/Audio/Plug-Ins/VST3/ISO.vst3" \
         "$HOME/Library/Audio/Plug-Ins/Components/ISO.component" \
         "$HOME/Library/Logs/ISO"; do
    if [ -e "$t" ]; then
        if [ -w "$(dirname "$t")" ]; then rm -rf "$t"; else sudo rm -rf "$t"; fi
        echo "  removed  $t"
    fi
done
echo
echo "Done. Rescan in your DAW."
read -r -p "Press return to close. "
CMD
chmod +x "$MSTAGE/Uninstall ISO.command"

cat > "$MSTAGE/READ ME.txt" <<TXT
ISO $VERSION - a DJ isolator EQ
by Gussa Naaman


INSTALL

  Double-click   ISO-$VERSION-macOS.pkg

  It asks which formats you want. Untick anything you do not use:

    VST3          Cubase, Live, Reaper, Bitwig, Studio One
    Audio Unit    Logic Pro, GarageBand
    Standalone    ISO on its own, no host needed

  Universal - Apple Silicon and Intel - so it loads natively and under
  Rosetta.

  In Cubase afterwards: Studio menu, VST Plug-in Manager, then Update.
  ISO appears under Naaman, in the EQ category.


THE FIRST TIME YOU OPEN THE STANDALONE

  This build is not notarised, so macOS refuses it once: right-click
  ISO.app, choose Open, then Open again in the dialog. After that it
  behaves normally.

  Plug-ins loaded by a DAW are NOT affected - only the standalone.


WHAT IT IS

  A three-band DJ isolator: LOW / MID / HIGH with a KILL under each, two
  crossover knobs (also draggable on the graph), and a Pioneer-style
  bipolar FILTER (left = low-pass, right = high-pass) with RES.

  SLOPE     24 dB/oct Linkwitz-Riley (rotary isolator) or 12 dB/oct (Xone:92)
  CUT       ISO = the knob's bottom is silence; EQ = the DJM's -26 dB floor
  OUTPUT    +/-12 dB trim

  With everything at 0 dB it is flat to 0.000 dB and adds zero latency.

  MANUAL.md is the manual. PARAMETER-TABLE.md lists every control and every
  preset, generated from the plug-in itself.


REMOVING IT

  Double-click "Uninstall ISO.command".
TXT

( cd "$MSTAGE" && zip -qr "$DIST/ISO-$VERSION-macOS.zip" . -x ".*" )
rm -rf "$MSTAGE"

# =============================================================================
#  Windows
# =============================================================================
if [ "$MAC_ONLY" = "0" ]; then
echo
echo "== Windows: from CI =="
WSTAGE="$DIST/stage-win"
mkdir -p "$WSTAGE"

cp "$WIN_EXE" "$WSTAGE/ISO-$VERSION-windows.exe"
cp -R "$WIN_VST3" "$WSTAGE/ISO.vst3"

# Windows' own Explorer writes these into a VST3 folder for the icon. They are
# harmless but they are not ours, and a delivery should contain only what it
# means to contain.
rm -f "$WSTAGE/ISO.vst3/desktop.ini" "$WSTAGE/ISO.vst3/Plugin.ico"

cat > "$WSTAGE/READ ME.txt" <<TXT
ISO $VERSION - a DJ isolator EQ
by Gussa Naaman


INSTALL - pick one, the first is easier

  1. THE INSTALLER

     Close your DAW, then double-click

         ISO-$VERSION-windows.exe

     Tick VST3 and/or the standalone. It puts them where Windows expects:

         C:\\Program Files\\Common Files\\VST3\\ISO.vst3
         C:\\Program Files\\Naaman\\ISO\\ISO.exe

     It also appears in Windows' Apps list, so it uninstalls like any
     other program.

     Not code-signed: SmartScreen may show a blue "Windows protected your
     PC" panel. Click "More info", then "Run anyway".

  2. BY HAND

     Copy the ISO.vst3 FOLDER next to this file into

         C:\\Program Files\\Common Files\\VST3\\

     Copy the whole folder, not the file inside it. To remove it later,
     delete that folder.


THEN

  Start Cubase, Studio menu, VST Plug-in Manager, Update.
  ISO appears under Naaman, in the EQ category.

  Close your DAW BEFORE installing. Windows will not replace a plug-in a
  running host has open, and that is the usual reason an update seems to
  do nothing at all.


HOW THIS WAS BUILT

  On a Windows machine with Visual Studio 2022, by GitHub Actions. All 125
  measurement checks - 51 signal checks and 74 host-contract checks - were
  run with Microsoft's compiler and had to pass before this installer was
  allowed to exist.


WHAT IT IS

  A three-band DJ isolator: LOW / MID / HIGH with a KILL under each, two
  crossover knobs (also draggable on the graph), and a Pioneer-style
  bipolar FILTER (left = low-pass, right = high-pass) with RES.

  SLOPE     24 dB/oct Linkwitz-Riley (rotary isolator) or 12 dB/oct (Xone:92)
  CUT       ISO = the knob's bottom is silence; EQ = the DJM's -26 dB floor
  OUTPUT    +/-12 dB trim

  With everything at 0 dB it is flat to 0.000 dB and adds zero latency.

  MANUAL.md is the manual.
TXT

cp "$ROOT/docs/MANUAL.md" "$WSTAGE/"

# CRLF, because these are read by Notepad on a machine that expects it.
python3 - "$WSTAGE/READ ME.txt" <<'PY'
import sys
p = sys.argv[1]
b = open(p, 'rb').read()
open(p, 'wb').write(b.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n"))
PY

( cd "$WSTAGE" && zip -qr "$DIST/ISO-$VERSION-Windows-Setup.zip" . -x ".*" )
rm -rf "$WSTAGE"
fi

# =============================================================================
#  Verify, by opening them again
# =============================================================================
echo
echo "== verifying =="
CHECK=$(mktemp -d)

unzip -q "$DIST/ISO-$VERSION-macOS.zip" -d "$CHECK/mac"
for f in "ISO-$VERSION-macOS.pkg" "READ ME.txt" "MANUAL.md" "PARAMETER-TABLE.md" "Uninstall ISO.command"; do
    [ -e "$CHECK/mac/$f" ] || { echo "  macOS zip MISSING $f"; exit 1; }
    echo "  mac  ok  $f"
done
[ -x "$CHECK/mac/Uninstall ISO.command" ] || { echo "  the uninstaller is not executable"; exit 1; }
# Check each COMPONENT, not the product archive. pkgutil --payload-files on a
# distribution package reports one component's count and looks like a pass even
# when a format is missing entirely - and a musician who ticks "Audio Unit" and
# gets nothing has no way to tell that from a Logic problem.
pkgutil --expand "$CHECK/mac/ISO-$VERSION-macOS.pkg" "$CHECK/pkg" >/dev/null
for c in vst3 au app; do
    [ -e "$CHECK/pkg/$c.pkg/Bom" ] || { echo "  the pkg has no $c component"; exit 1; }
    n=$(lsbom -s "$CHECK/pkg/$c.pkg/Bom" | wc -l | tr -d ' ')
    [ "$n" -gt 10 ] || { echo "  the $c component is empty ($n entries)"; exit 1; }
    echo "  mac  ok  $c component, $n entries"
done
# And that each lands where that format is actually loaded from.
lsbom -s "$CHECK/pkg/vst3.pkg/Bom" | grep -q "/Library/Audio/Plug-Ins/VST3/ISO.vst3/Contents/MacOS/ISO$" \
    || { echo "  the VST3 does not install to the VST3 folder"; exit 1; }
lsbom -s "$CHECK/pkg/au.pkg/Bom"   | grep -q "/Library/Audio/Plug-Ins/Components/ISO.component/Contents/MacOS/ISO$" \
    || { echo "  the Audio Unit does not install to the Components folder"; exit 1; }
lsbom -s "$CHECK/pkg/app.pkg/Bom"  | grep -q "/Applications/ISO.app/Contents/MacOS/ISO$" \
    || { echo "  the standalone does not install to /Applications"; exit 1; }
echo "  mac  ok  all three install where their format is loaded from"

if [ "$MAC_ONLY" = "0" ]; then
unzip -q "$DIST/ISO-$VERSION-Windows-Setup.zip" -d "$CHECK/win"
for f in "ISO-$VERSION-windows.exe" "READ ME.txt" "MANUAL.md" "ISO.vst3/Contents/x86_64-win/ISO.vst3"; do
    [ -e "$CHECK/win/$f" ] || { echo "  Windows zip MISSING $f"; exit 1; }
    echo "  win  ok  $f"
done
# An installer that packed nothing is still a valid .exe, about a megabyte of
# wizard and no payload.
sz=$(stat -f%z "$CHECK/win/ISO-$VERSION-windows.exe")
[ "$sz" -gt 2000000 ] || { echo "  the installer is only $sz bytes - it is empty"; exit 1; }
file "$CHECK/win/ISO.vst3/Contents/x86_64-win/ISO.vst3" | grep -q "x86-64" \
    || { echo "  the VST3 payload is not 64-bit"; exit 1; }
echo "  win  ok  installer $((sz / 1048576)) MB, VST3 payload is x86-64"
fi

rm -rf "$CHECK"

# Exactly two files. Anything else in dist/ is something a person could send
# by mistake.
count=$(find "$DIST" -maxdepth 1 -type f | wc -l | tr -d ' ')
want=2; [ "$MAC_ONLY" = "1" ] && want=1
[ "$count" = "$want" ] || { echo; echo "dist/ has $count files, expected $want:"; ls -1 "$DIST"; exit 1; }

echo
[ "$MAC_ONLY" = "1" ] && echo "== done - macOS half only (--mac-only), verified ==" || echo "== done - two files, both verified =="
ls -lh "$DIST" | tail -n +2
