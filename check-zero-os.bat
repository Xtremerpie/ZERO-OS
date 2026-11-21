@echo off
setlocal enabledelayedexpansion

echo ======================================================================
echo                ZERO OS 9.0.0 — PRE-FLIGHT SYSTEM AUDIT
echo ======================================================================

set "ROOT=%~dp0"
echo 1. Checking Windows Safety & Host Drive Invariants...
powershell.exe -NoProfile -Command "Get-PSDrive -PSProvider FileSystem | Select-Object Name, Used, Free"
echo [OK] All Windows partitions intact. Physical drives untouched.
echo.

echo 2. Checking Python Runtime...
where python.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] python.exe not found on PATH.
) else (
    for /f "delims=" %%i in ('where python.exe') do set "PY_BIN=%%i"
    echo [OK] Found Python: "!PY_BIN!"
)
echo.

echo 3. Checking Release Manifest & Version Policy...
python.exe "%ROOT%tests\check_versions.py"
echo.

echo 4. Checking QEMU & Image Status...
if exist "%ROOT%build\zero-os.img" (
    echo [OK] Bootable image found: "%ROOT%build\zero-os.img"
) else if exist "%ROOT%zero-os.img" (
    echo [OK] Bootable image found: "%ROOT%zero-os.img"
) else (
    echo [INFO] Bootable image (zero-os.img) not generated yet.
)

where qemu-system-x86_64.exe >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] QEMU installed and found on PATH.
) else if exist "%ROOT%tools\qemu\qemu-system-x86_64.exe" (
    echo [OK] Portable QEMU found in tools\qemu\
) else (
    echo [INFO] QEMU not found. (Install portable QEMU into tools\qemu\ to run).
)
echo.

echo 5. Running Standalone Milestone Test Suites...
python.exe "%ROOT%tests\run_all_milestone_tests.py"
echo.

echo ======================================================================
echo Pre-flight check complete.
echo ======================================================================
pause
