@echo off
setlocal enabledelayedexpansion
title ISO 1.0.1 - Installer

REM  Copies the prebuilt ISO.vst3 next to this file into the VST3 folder.
REM  Same shape as TRIX's installer, which is the one that works on this
REM  user's machine. Every failure prints where and why; nothing is hidden.

echo.
echo  ============================================
echo    ISO by Gussa Naaman  -  v1.0.1
echo    DJ Isolator EQ
echo  ============================================
echo.

REM --- administrator ----------------------------------------------------------
net session >nul 2>&1
if errorlevel 1 (
    echo  [X] This installer needs administrator rights.
    echo.
    echo      Close this window, RIGHT-CLICK INSTALL-ISO.bat
    echo      and choose "Run as administrator".
    echo.
    pause
    exit /b 1
)
echo  [OK] Running as administrator.

REM --- the payload must be sitting next to this file ---------------------------
set "SRC=%~dp0ISO.vst3"

if not exist "%SRC%\" (
    echo  [X] ISO.vst3 was not found next to this installer.
    echo.
    echo      Looked in: %~dp0
    echo.
    echo      You are probably running this from inside the ZIP.
    echo      EXTRACT the whole ZIP to a real folder first
    echo      ^(right-click the ZIP, "Extract All"^), then run
    echo      INSTALL-ISO.bat from the extracted folder.
    echo.
    pause
    exit /b 1
)
if not exist "%SRC%\Contents\x86_64-win\ISO.vst3" (
    echo  [X] ISO.vst3 is here but its binary is missing:
    echo      %SRC%\Contents\x86_64-win\ISO.vst3
    echo      The ZIP was not extracted completely. Extract it again.
    echo.
    pause
    exit /b 1
)
echo  [OK] Found ISO.vst3 to install.

set "DEST=C:\Program Files\Common Files\VST3"
if not exist "%DEST%\" mkdir "%DEST%" 2>nul

REM --- warn about a running DAW ------------------------------------------------
set "DAW="
for %%P in ("Cubase.exe" "Nuendo.exe" "Ableton Live.exe" "FL64.exe" "FL.exe" "Studio One.exe" "reaper.exe" "Bitwig Studio.exe") do (
    tasklist /fi "imagename eq %%~P" 2>nul | find /i "%%~P" >nul && set "DAW=%%~P"
)
if defined DAW (
    echo.
    echo  [!] %DAW% appears to be running.
    echo      Windows will not let the plugin be replaced while a host has it
    echo      loaded. Please close it now, then press any key to continue.
    echo.
    pause
)

REM --- remove any previous install --------------------------------------------
echo.
echo  [1/3] Removing any previous version...

if exist "%DEST%\ISO.vst3\" (
    echo        found a folder bundle - deleting
    rmdir /s /q "%DEST%\ISO.vst3"
) else if exist "%DEST%\ISO.vst3" (
    echo        found a single file - deleting
    del /f /q "%DEST%\ISO.vst3"
) else (
    echo        nothing to remove
)

if exist "%DEST%\ISO.vst3" (
    echo.
    echo  [X] The old ISO could not be removed.
    echo      Something still has it open - usually a DAW, or the plugin
    echo      scanner that runs in the background after a DAW closes.
    echo.
    echo      Close every audio application, wait a few seconds and run
    echo      this installer again.
    echo.
    pause
    exit /b 1
)
echo        done.

REM --- install ------------------------------------------------------------------
echo  [2/3] Installing ISO 1.0.1 ...
xcopy /e /i /y "%SRC%" "%DEST%\ISO.vst3\" >nul
if errorlevel 1 (
    echo.
    echo  [X] Copy failed.
    echo      Source: %SRC%
    echo      Target: %DEST%\ISO.vst3
    echo.
    pause
    exit /b 1
)

REM --- verify -------------------------------------------------------------------
echo  [3/3] Verifying...
if not exist "%DEST%\ISO.vst3\Contents\x86_64-win\ISO.vst3" (
    echo.
    echo  [X] The plugin binary is not where it should be after copying.
    echo      Expected: %DEST%\ISO.vst3\Contents\x86_64-win\ISO.vst3
    echo.
    pause
    exit /b 1
)
echo        verified.

echo.
echo  ============================================
echo    Done. ISO 1.0.1 is installed.
echo.
echo    Installed to:
echo    %DEST%\ISO.vst3
echo.
echo    1. Start your DAW and rescan plugins
echo       ^(Cubase: Studio, VST Plug-in Manager, Update^)
echo    2. Find "ISO" under Naaman, category EQ
echo    3. Put it on an AUDIO channel and hit KILL
echo  ============================================
echo.
pause
