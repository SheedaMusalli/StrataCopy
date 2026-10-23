param([switch]$SkipBuild,[string]$Version='1.1.0')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
if(!$SkipBuild){& "$PSScriptRoot\build.ps1" -Deploy}
$taskPortable=Join-Path $taskRoot "dist\StrataCopy-$Version"
if(!(Test-Path -LiteralPath "$taskPortable\StrataCopy.exe")){throw 'Deploy the app first'}
Compress-Archive -LiteralPath $taskPortable -DestinationPath "$taskRoot\dist\StrataCopy-$Version-Windows-x64.zip" -CompressionLevel Optimal -Force
$taskSources=@('CMakeLists.txt','.gitignore','.clang-format','README.md','RELEASE_NOTES.md','MEMORY_REVIEW.md','THIRD_PARTY_NOTICES.md','ARCHITECTURE.txt','src','resources','tests','scripts','installer','third_party') | ForEach-Object {Join-Path $taskRoot $_}
if(Test-Path -LiteralPath "$taskRoot\VALIDATION.md"){$taskSources+=Join-Path $taskRoot 'VALIDATION.md'}
Compress-Archive -LiteralPath $taskSources -DestinationPath "$taskRoot\dist\StrataCopy-$Version-source.zip" -CompressionLevel Optimal -Force
$taskHashFiles=@("$taskRoot\dist\StrataCopy-$Version-Windows-x64.zip","$taskRoot\dist\StrataCopy-$Version-source.zip")
if(Test-Path -LiteralPath "$taskRoot\dist\StrataCopy-$Version-Setup-x64.exe"){$taskHashFiles+="$taskRoot\dist\StrataCopy-$Version-Setup-x64.exe"}
$taskHashes=Get-FileHash -Algorithm SHA256 -LiteralPath $taskHashFiles
$taskHashes | ForEach-Object {$_.Hash.ToLower()+'  '+(Split-Path -Leaf $_.Path)} | Set-Content -LiteralPath "$taskRoot\dist\SHA256SUMS.txt" -Encoding utf8
$taskHashes | Format-Table Hash,Path
