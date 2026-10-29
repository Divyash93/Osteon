# Removes the per-user Osteon install created by install.ps1. User projects and settings
# (%APPDATA%\Osteon) are left in place.
$dest = Join-Path $env:LOCALAPPDATA 'Programs\Osteon'
Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path ([Environment]::GetFolderPath('Programs')) 'Osteon.lnk')
Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path ([Environment]::GetFolderPath('Desktop')) 'Osteon.lnk')
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue 'HKCU:\Software\Classes\Osteon.Project'
if ((Get-ItemProperty -ErrorAction SilentlyContinue 'HKCU:\Software\Classes\.osteon').'(default)' -eq 'Osteon.Project') {
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue 'HKCU:\Software\Classes\.osteon'
}
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\Osteon'
# The script lives inside the install folder, so delete the folder after this process exits.
Start-Process -WindowStyle Hidden cmd.exe -ArgumentList "/c timeout /t 2 >nul & rmdir /s /q `"$dest`""
Write-Host 'Osteon uninstalled.'
