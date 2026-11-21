@echo off
setlocal enabledelayedexpansion

echo ======================================================================
echo                ZERO OS 9.0.0 — SAFE LAUNCHER (QEMU)
echo ======================================================================
echo SAFETY INVARIANT: Host physical drives and Windows partitions are ISOLATED.
echo QEMU runs in pure user-space with snapshot=on (no physical disk writes).
echo ======================================================================

set "ROOT=%~dp0"
set "BUILD_DIR=%ROOT%build"
set "IMG=%BUILD_DIR%\zero-os.img"
if not exist "%IMG%" set "IMG=%ROOT%zero-os.img"

:: 1. Locate QEMU executable
set "QEMU_BIN="
if exist "%ROOT%tools\qemu\qemu-system-x86_64.exe" set "QEMU_BIN=%ROOT%tools\qemu\qemu-system-x86_64.exe"
if not defined QEMU_BIN if exist "C:\Program Files\qemu\qemu-system-x86_64.exe" set "QEMU_BIN=C:\Program Files\qemu\qemu-system-x86_64.exe"
if not defined QEMU_BIN (
    where qemu-system-x86_64.exe >nul 2>&1
    if !errorlevel! equ 0 for /f "delims=" %%i in ('where qemu-system-x86_64.exe') do set "QEMU_BIN=%%i"
)

if not defined QEMU_BIN (
    echo [ERROR] QEMU binary (qemu-system-x86_64.exe) not found!
    echo.
    echo To run ZERO OS safely on Windows without administrator rights:
    echo 1. Download portable QEMU for Windows (e.g. from https://qemu.weilnetz.de/w64/)
    echo 2. Place files into: %ROOT%tools\qemu\
    echo.
    echo Or install via winget (requires user confirmation):
    echo    winget install --id SoftwareFreedomConservancy.QEMU -e
    echo.
    pause
    exit /b 1
)
echo [OK] Located QEMU: "%QEMU_BIN%"

:: 2. Check bootable disk image
if not exist "%IMG%" (
    echo [ERROR] Bootable disk image not found: "%IMG%"
    echo.
    echo The kernel disk image (zero-os.img) has not been compiled yet on this host.
    echo To compile the kernel, use the documented Linux cross-compiler toolchain:
    echo    make image
    echo and place zero-os.img in "%BUILD_DIR%\" or "%ROOT%".
    echo.
    pause
    exit /b 2
)
echo [OK] Located Boot Image: "%IMG%"

:: 3. Locate OVMF UEFI firmware
set "OVMF_CODE="
set "OVMF_VARS="
if exist "%ROOT%tools\ovmf\OVMF_CODE_4M.fd" (
    set "OVMF_CODE=%ROOT%tools\ovmf\OVMF_CODE_4M.fd"
    set "OVMF_VARS=%ROOT%tools\ovmf\OVMF_VARS_4M.fd"
)
if not defined OVMF_CODE if exist "%ROOT%build\OVMF_CODE_4M.fd" (
    set "OVMF_CODE=%ROOT%build\OVMF_CODE_4M.fd"
    set "OVMF_VARS=%ROOT%build\OVMF_VARS_4M.fd"
)
if not defined OVMF_CODE if exist "C:\Program Files\qemu\share\edk2-x86_64-code.fd" (
    set "OVMF_CODE=C:\Program Files\qemu\share\edk2-x86_64-code.fd"
    set "OVMF_VARS=C:\Program Files\qemu\share\edk2-i386-vars.fd"
)

if not defined OVMF_CODE (
    echo [ERROR] OVMF UEFI Firmware file not found!
    echo.
    echo Place OVMF_CODE_4M.fd and OVMF_VARS_4M.fd into:
    echo    %ROOT%tools\ovmf\
    echo.
    pause
    exit /b 3
)
echo [OK] Located OVMF Firmware: "%OVMF_CODE%"

:: 4. Prepare writable copy of UEFI variable store
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
set "RUN_VARS=%BUILD_DIR%\vars-run.fd"
if exist "%OVMF_VARS%" (
    copy /Y "%OVMF_VARS%" "%RUN_VARS%" >nul
) else (
    copy /Y "%OVMF_CODE%" "%RUN_VARS%" >nul
)
echo [OK] Prepared writable UEFI variable store: "%RUN_VARS%"

:: 5. Launch QEMU with strict safety configuration
echo.
echo Starting ZERO OS in isolated QEMU machine...
echo Settings: 256 MB RAM, 1 CPU Core, TCG emulation, Snapshot virtual disk mode.
echo Window will open shortly. Close the window to shut down ZERO OS.
echo.

"%QEMU_BIN%" ^
  -machine q35 -cpu qemu64 -m 256M -smp 1 -accel tcg ^
  -drive if=pflash,format=raw,readonly=on,file="%OVMF_CODE%" ^
  -drive if=pflash,format=raw,file="%RUN_VARS%" ^
  -drive file="%IMG%",format=raw,if=virtio,snapshot=on ^
  -device isa-debug-exit,iobase=0xf4,iosize=0x04 ^
  -serial file:"%ROOT%zero-serial.log" -no-reboot

echo.
echo ZERO OS session ended. Serial log saved to: "%ROOT%zero-serial.log"
pause
