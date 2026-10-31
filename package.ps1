# Builds Osteon (Release), runs the tests, and produces:
#   dist\Osteon\                 portable folder (Osteon.exe + README)
#   dist\Osteon-0.1.0-win64.zip  portable zip
#   dist\OsteonSetup.exe         per-user setup wizard (installer\setup.cpp, branded)
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

# The branded setup wizard (installer\setup.cpp): the program, README and banner are embedded in it.
$version = '0.1.0'
if ((Get-Content "$root\src\editor\app.rc" -Raw) -match 'VALUE "ProductVersion", "([^"]+)"') { $version = $Matches[1] }
cmd /c "`"$root\installer\build_setup.bat`" $version `"$dist`""
if ($LASTEXITCODE -ne 0 -or -not (Test-Path "$dist\OsteonSetup.exe")) { throw 'Building the setup program failed' }
Get-ChildItem $dist
