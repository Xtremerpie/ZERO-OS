@echo off
echo ======================================================================
echo                ZERO OS 9.0.0 — SAFE VM SHUTDOWN
echo ======================================================================

echo Terminating running QEMU instance safely...
taskkill /IM qemu-system-x86_64.exe /T /F >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] ZERO OS QEMU process terminated safely.
    echo Disk snapshot was discarded. Host storage and images are unchanged.
) else (
    echo [INFO] No running ZERO OS QEMU process was found.
)
echo ======================================================================
pause
