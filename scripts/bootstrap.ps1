param([string]$Python='python')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
& $Python -m pip install --target "$taskRoot\.tools\python" aqtinstall cmake ninja
if($LASTEXITCODE){throw 'Build utility installation failed'}
$env:PYTHONPATH="$taskRoot\.tools\python"
& $Python -m aqt install-qt windows desktop 6.8.3 win64_mingw -O "$taskRoot\.tools\Qt" -m qtshadertools
if($LASTEXITCODE){throw 'Qt download failed'}
& $Python -m aqt install-tool windows desktop tools_mingw1310 qt.tools.win64_mingw1310 -O "$taskRoot\.tools\Qt"
if($LASTEXITCODE){throw 'MinGW download failed'}
