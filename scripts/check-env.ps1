# ZERO OS - Windows 11 environment audit (UNTESTED ON WINDOWS: written without access to a
# Windows machine; treat its output as a convenience, and report any problem).
#
# Reports what is installed; changes nothing. Prints only tool names/versions and coarse
# hardware class so the output is safe to paste into an issue.
#   powershell -ExecutionPolicy Bypass -File scripts\check-env.ps1

$ErrorActionPreference = "SilentlyContinue"

Write-Host "ZERO OS environment check (Windows)"
$os  = Get-CimInstance Win32_OperatingSystem
$cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
Write-Host ("  Windows : {0} (build {1})" -f $os.Caption, $os.BuildNumber)
Write-Host ("  CPU     : {0}, {1} cores" -f $cpu.Name.Trim(), $cpu.NumberOfCores)
Write-Host ("  RAM     : {0:N1} GB total, {1:N1} GB free" -f ($os.TotalVisibleMemorySize/1MB), ($os.FreePhysicalMemory/1MB))
$drive = Get-PSDrive -Name ($PWD.Drive.Name)
Write-Host ("  Disk    : {0:N1} GB free on {1}:" -f ($drive.Free/1GB), $drive.Name)

$missing = 0
function Check($name, $hint) {
    $c = Get-Command $name -ErrorAction SilentlyContinue
    if ($c) { Write-Host ("  [ ok ] {0,-22} {1}" -f $name, $c.Source) }
    else    { Write-Host ("  [MISS] {0,-22} {1}" -f $name, $hint); $script:missing++ }
}

Write-Host "Native Windows tools:"
Check "git"                 "winget install Git.Git"
Check "clang"               "winget install LLVM.LLVM"
Check "ld.lld"              "ships with LLVM"
Check "nasm"                "winget install NASM.NASM"
Check "qemu-system-x86_64"  "winget install SoftwareFreedomConservancy.QEMU"
Check "gdb"                 "MSYS2: pacman -S mingw-w64-ucrt-x86_64-gdb"
Check "python"              "winget install Python.Python.3.12"

Write-Host "WSL2 (the verified build path - see BUILDING.md):"
$wsl = Get-Command wsl.exe -ErrorAction SilentlyContinue
if ($wsl) { wsl.exe --status 2>&1 | Select-Object -First 3 | ForEach-Object { Write-Host "  $_" } }
else      { Write-Host "  [MISS] wsl.exe  (enable the 'Windows Subsystem for Linux' feature)"; $missing++ }

if ($os.TotalVisibleMemorySize/1MB -lt 6) {
    Write-Host "  note: <=4 GB RAM. If you use WSL2, cap it in %UserProfile%\.wslconfig ([wsl2] memory=2GB) so Windows stays responsive."
}
if ($missing -gt 0) { Write-Host "RESULT: $missing item(s) missing."; exit 1 }
Write-Host "RESULT: all listed tools found."
