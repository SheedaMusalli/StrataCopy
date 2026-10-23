param([switch]$Deploy,[switch]$SkipTests,[switch]$UiTestHooks)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskQt=Join-Path $taskRoot '.tools\Qt\6.8.3\mingw_64'
$taskMingw=Join-Path $taskRoot '.tools\Qt\Tools\mingw1310_64\bin'
$taskCmake=Join-Path $taskRoot '.tools\python\cmake\data\bin\cmake.exe'
$taskNinja=Join-Path $taskRoot '.tools\python\bin\ninja.exe'
if(!(Test-Path -LiteralPath $taskCmake)){$taskCmake=(Get-Command cmake -ErrorAction Stop).Source}
if(!(Test-Path -LiteralPath $taskNinja)){$taskNinja=(Get-Command ninja -ErrorAction Stop).Source}
if($env:QT_ROOT){$taskQt=$env:QT_ROOT}
$env:PATH="$taskMingw;$taskQt\bin;"+$env:PATH
$taskHooks=if($UiTestHooks){'ON'}else{'OFF'}
& $taskCmake -S $taskRoot -B "$taskRoot\build" -G Ninja "-DCMAKE_MAKE_PROGRAM=$taskNinja" "-DCMAKE_PREFIX_PATH=$taskQt" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON "-DSTRATACOPY_ENABLE_UI_TEST_HOOKS=$taskHooks"
if($LASTEXITCODE){throw 'CMake configure failed'}
& $taskCmake --build "$taskRoot\build" --parallel 4
if($LASTEXITCODE){throw 'Compilation failed'}
if(!$SkipTests){
foreach($taskSuite in @(@('stratacopy_tests','test-results'),@('stratacopy_native_tests','native-test-results'))){
& "$taskRoot\build\$($taskSuite[0]).exe" -o ("{0}\build\{1}.txt,txt" -f $taskRoot,$taskSuite[1])
if($LASTEXITCODE){Get-Content "$taskRoot\build\$($taskSuite[1]).txt";throw 'Tests failed'}
Get-Content "$taskRoot\build\$($taskSuite[1]).txt"
if(Select-String -LiteralPath "$taskRoot\build\$($taskSuite[1]).txt" -Pattern '^SKIP\s+:'){
throw 'A required test was skipped. Stop the Explorer helper and rerun all checks.'
}
}}
if($Deploy){
$taskVersion=[regex]::Match((Get-Content -LiteralPath "$taskRoot\CMakeLists.txt" -Raw),'project\(StrataCopy VERSION ([0-9.]+)').Groups[1].Value
& $taskCmake --install "$taskRoot\build" --prefix "$taskRoot\dist\StrataCopy-$taskVersion";if($LASTEXITCODE){throw 'Deployment failed'}
}
