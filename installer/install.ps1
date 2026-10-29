# Per-user Osteon installer (no administrator rights needed).
# Copies Osteon to %LOCALAPPDATA%\Programs\Osteon, adds Start Menu and Desktop shortcuts,
# associates .osteon files and registers an uninstaller in "Installed apps".
$ErrorActionPreference = 'Stop'
$src = Split-Path -Parent $MyInvocation.MyCommand.Path
$dest = Join-Path $env:LOCALAPPDATA 'Programs\Osteon'
New-Item -ItemType Directory -Force -Path $dest | Out-Null
Copy-Item -Path (Join-Path $src 'Osteon.exe') -Destination $dest -Force
foreach ($f in 'README.md', 'uninstall.ps1') {
    if (Test-Path (Join-Path $src $f)) { Copy-Item -Path (Join-Path $src $f) -Destination $dest -Force }
}
$exe = Join-Path $dest 'Osteon.exe'

$shell = New-Object -ComObject WScript.Shell
$startMenu = Join-Path ([Environment]::GetFolderPath('Programs')) 'Osteon.lnk'
$desktop = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Osteon.lnk'
foreach ($lnkPath in $startMenu, $desktop) {
    $lnk = $shell.CreateShortcut($lnkPath)
    $lnk.TargetPath = $exe
    $lnk.WorkingDirectory = $dest
    $lnk.IconLocation = "$exe,0"
    $lnk.Description = 'Osteon 2D skeletal animation editor'
    $lnk.Save()
}

# .osteon file association (current user only).
$classes = 'HKCU:\Software\Classes'
New-Item -Force -Path "$classes\.osteon" | Out-Null
Set-ItemProperty -Path "$classes\.osteon" -Name '(default)' -Value 'Osteon.Project'
New-Item -Force -Path "$classes\Osteon.Project\DefaultIcon" | Out-Null
Set-ItemProperty -Path "$classes\Osteon.Project" -Name '(default)' -Value 'Osteon Project'
Set-ItemProperty -Path "$classes\Osteon.Project\DefaultIcon" -Name '(default)' -Value "`"$exe`",0"
New-Item -Force -Path "$classes\Osteon.Project\shell\open\command" | Out-Null
Set-ItemProperty -Path "$classes\Osteon.Project\shell\open\command" -Name '(default)' -Value "`"$exe`" `"%1`""

# Uninstall entry.
$un = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\Osteon'
New-Item -Force -Path $un | Out-Null
Set-ItemProperty -Path $un -Name 'DisplayName' -Value 'Osteon'
Set-ItemProperty -Path $un -Name 'DisplayVersion' -Value '0.1.0'
Set-ItemProperty -Path $un -Name 'Publisher' -Value 'Osteon'
Set-ItemProperty -Path $un -Name 'DisplayIcon' -Value $exe
Set-ItemProperty -Path $un -Name 'InstallLocation' -Value $dest
Set-ItemProperty -Path $un -Name 'NoModify' -Value 1 -Type DWord
Set-ItemProperty -Path $un -Name 'NoRepair' -Value 1 -Type DWord
Set-ItemProperty -Path $un -Name 'UninstallString' -Value "powershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$dest\uninstall.ps1`""

Write-Host "Osteon installed to $dest"
