# Dev helper: builds and runs tests, printing only errors and test results.
$o = & "$PSScriptRoot\build.bat" test 2>&1 | Out-String
($o -split "`r?`n" | Where-Object { $_ -match 'error|FAIL|OK \]|tests,' } | Select-Object -First 60) -join "`n"
