@echo off
setlocal
cd /d "%~dp0"
title Holterdipolter Comp - Build

set "LOG=%~dp0build-log.txt"
set "STATUS=%~dp0build-status.txt"
echo LAEUFT > "%STATUS%"
echo Build gestartet %DATE% %TIME% > "%LOG%"

rem --- CMake finden: erst PATH, dann das von Visual Studio mitgelieferte ---
set "CMAKE=cmake"
where cmake >nul 2>nul
if errorlevel 1 (
    set "CMAKE="
    for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath 2^>nul`) do (
        if exist "%%i\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" set "CMAKE=%%i\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    )
)
if not defined CMAKE (
    echo FEHLER: CMake wurde nicht gefunden. >> "%LOG%"
    echo FEHLER: CMake wurde nicht gefunden.
    goto :fail
)
echo CMake: %CMAKE% >> "%LOG%"

echo [1/3] Konfigurieren ...
"%CMAKE%" -S . -B build -A x64 >> "%LOG%" 2>&1
if errorlevel 1 goto :fail

echo [2/3] Bauen (Release) - das dauert einige Minuten ...
"%CMAKE%" --build build --config Release --parallel >> "%LOG%" 2>&1
if errorlevel 1 goto :fail

echo [3/3] Ergebnis bereitlegen ...
if not exist "build\HolterdipolterComp_artefacts\Release\VST3\Holterdipolter Comp.vst3" (
    echo FEHLER: Build-Ergebnis nicht gefunden. >> "%LOG%"
    goto :fail
)
if exist "Fertig\Holterdipolter Comp.vst3" rmdir /s /q "Fertig\Holterdipolter Comp.vst3"
xcopy /e /i /y /q "build\HolterdipolterComp_artefacts\Release\VST3\Holterdipolter Comp.vst3" "Fertig\Holterdipolter Comp.vst3" >> "%LOG%" 2>&1
if errorlevel 1 goto :fail

echo OK > "%STATUS%"
echo Build fertig %DATE% %TIME% >> "%LOG%"
echo.
echo FERTIG. Die VST3 liegt in:
echo   %~dp0Fertig\Holterdipolter Comp.vst3
echo Diesen Ordner nach C:\Program Files\Common Files\VST3\ kopieren.
echo.
pause
exit /b 0

:fail
echo FEHLER > "%STATUS%"
echo Build fehlgeschlagen %DATE% %TIME% >> "%LOG%"
echo.
echo FEHLER - Details stehen in build-log.txt
echo.
pause
exit /b 1
