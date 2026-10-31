# Builds Osteon (Release), runs the tests, and produces:
#   dist\Osteon\                 portable folder (Osteon.exe + README)
#   dist\Osteon-0.1.0-win64.zip  portable zip
#   dist\OsteonSetup.exe         per-user installer (built with Windows' own IExpress)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
& "$root\build.bat" test
if ($LASTEXITCODE -ne 0) { throw 'Build or tests failed' }

$dist = Join-Path $root 'dist'
$app = Join-Path $dist 'Osteon'
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $dist
New-Item -ItemType Directory -Force -Path $app | Out-Null
Copy-Item "$root\build\Osteon.exe" $app
Copy-Item "$root\README.md" $app
Copy-Item "$root\installer\install.ps1", "$root\installer\uninstall.ps1" $app
Compress-Archive -Path "$app\*" -DestinationPath (Join-Path $dist 'Osteon-0.1.0-win64.zip') -Force

# IExpress self-extracting installer that runs install.ps1.
$stage = Join-Path $dist 'stage'
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item "$app\*" $stage
Set-Content -Encoding ASCII -Path "$stage\setup.cmd" -Value '@powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1"'
$files = Get-ChildItem $stage -File
$sed = @"
[Version]
Class=IEXPRESS
SEDVersion=3
[Options]
PackagePurpose=InstallApp
ShowInstallProgramWindow=0
HideExtractAnimation=1
UseLongFileName=1
InsideCompressed=0
CAB_FixedSize=0
CAB_ResvCodeSigning=0
RebootMode=N
InstallPrompt=
DisplayLicense=
FinishMessage=Osteon has been installed. Start it from the Start menu or the desktop shortcut.
TargetName=$dist\OsteonSetup.exe
FriendlyName=Osteon Setup
AppLaunched=cmd /c setup.cmd
PostInstallCmd=<None>
AdminQuietInstCmd=
UserQuietInstCmd=
SourceFiles=SourceFiles
[Strings]
"@
$sed += "`r`n"
$i = 0
foreach ($f in $files) { $sed += "FILE$i=`"$($f.Name)`"`r`n"; $i++ }
$sed += "[SourceFiles]`r`nSourceFiles0=$stage\`r`n[SourceFiles0]`r`n"
for ($j = 0; $j -lt $i; $j++) { $sed += "%FILE$j%=`r`n" }
$sed = $sed -replace "`r?`n", "`r`n"
Set-Content -Encoding ASCII -Path "$dist\osteon.sed" -Value $sed
# IExpress returns before it finishes unless waited on explicitly.
Start-Process -FilePath "$env:SystemRoot\System32\iexpress.exe" -ArgumentList '/N', '/Q', "$dist\osteon.sed" -Wait
Remove-Item -Recurse -Force $stage, "$dist\osteon.sed"
Remove-Item -Force -ErrorAction SilentlyContinue "$dist\~*.DDF"
if (-not (Test-Path "$dist\OsteonSetup.exe")) { throw 'IExpress did not produce OsteonSetup.exe' }
# The installer carries the Osteon icon too.
try { & "$root\installer\set_exe_icon.ps1" -Exe "$dist\OsteonSetup.exe" -Icon "$root\src\editor\app.ico" } catch { Write-Warning "Could not set the installer icon: $_" }
Get-ChildItem $dist
