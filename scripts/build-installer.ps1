param([switch]$SkipBuild,[switch]$TestInstaller)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
if(!$SkipBuild){& "$PSScriptRoot\build.ps1" -Deploy}
$taskCompiler=Join-Path $taskRoot '.tools\InnoSetup\ISCC.exe'
if(!(Test-Path -LiteralPath $taskCompiler)){$taskCompiler=(Get-Command ISCC.exe -ErrorAction Stop).Source}
if($TestInstaller){& $taskCompiler /DTEST_INSTALLER=1 "$taskRoot\installer\StrataCopy.iss"}else{& $taskCompiler "$taskRoot\installer\StrataCopy.iss"}
if($LASTEXITCODE){throw 'Installer compilation failed'}
