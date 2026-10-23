param([switch]$SkipCompile)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
if(!$SkipCompile){& "$PSScriptRoot\build-installer.ps1" -SkipBuild -TestInstaller}
$taskSetup=Join-Path $taskRoot 'build\installer-qa\StrataCopy-Installer-QA.exe'
$taskInstall=Join-Path $taskRoot 'test-output\installer-qa'
$taskRegistry='HKCU:\Software\StrataCopyInstallerQA'
$taskActualRun='HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$taskBefore=(Get-ItemProperty -LiteralPath $taskActualRun -Name StrataCopyExplorer -ErrorAction SilentlyContinue).StrataCopyExplorer
$taskResult=Start-Process -FilePath $taskSetup -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-',"/DIR=$taskInstall",'/TASKS=desktopicon,explorermenu,keyboardpaste',"/LOG=$taskRoot\build\installer-test-install.log") -WindowStyle Hidden -Wait -PassThru
if($taskResult.ExitCode -ne 0){throw "QA install failed: $($taskResult.ExitCode)"}
foreach($taskFile in @('StrataCopy.exe','StrataCopyExplorer.exe','StrataCopyNative-1.0.0.dll','Qt6Core.dll','unins000.exe','Shortcuts\Programs\StrataCopy.lnk','Shortcuts\Desktop\StrataCopy.lnk')){if(!(Test-Path -LiteralPath (Join-Path $taskInstall $taskFile))){throw "Missing installed file: $taskFile"}}
$taskVerb=Get-Item -LiteralPath "$taskRegistry\Classes\*\shell\StrataCopy\command"
if($taskVerb.GetValue('') -notlike '*--add*'){throw 'Copy verb was not registered'}
$taskPaste=Get-Item -LiteralPath "$taskRegistry\Classes\Directory\Background\shell\StrataCopyPaste\command"
if($taskPaste.GetValue('') -notlike '*--paste*'){throw 'Paste verb was not registered'}
if(!(Get-ItemProperty -LiteralPath "$taskRegistry\Run" -Name StrataCopyExplorer).StrataCopyExplorer){throw 'Opt-in startup entry missing'}
$taskSourceHash=(Get-FileHash -LiteralPath "$taskRoot\dist\StrataCopy-1.0.0\StrataCopy.exe").Hash
if((Get-FileHash -LiteralPath "$taskInstall\StrataCopy.exe").Hash -ne $taskSourceHash){throw 'Installed executable differs from the release'}
$taskResult=Start-Process -FilePath "$taskInstall\StrataCopyExplorer.exe" -ArgumentList '--self-test' -WindowStyle Hidden -Wait -PassThru
if($taskResult.ExitCode -ne 0){throw 'Installed helper self-test failed'}
$taskResult=Start-Process -FilePath $taskSetup -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-',"/DIR=$taskInstall","/LOG=$taskRoot\build\installer-test-upgrade.log") -WindowStyle Hidden -Wait -PassThru
if($taskResult.ExitCode -ne 0){throw 'QA upgrade failed'}
if(!(Test-Path -LiteralPath "$taskRegistry\Classes\Directory\Background\shell\StrataCopyPaste\command")){throw 'Upgrade lost the previously selected Explorer menu task'}
if(!(Get-ItemProperty -LiteralPath "$taskRegistry\Run" -Name StrataCopyExplorer).StrataCopyExplorer){throw 'Upgrade lost the previously selected startup task'}
$taskResult=Start-Process -FilePath "$taskInstall\unins000.exe" -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',"/LOG=$taskRoot\build\installer-test-uninstall.log") -WindowStyle Hidden -Wait -PassThru
if($taskResult.ExitCode -ne 0){throw 'QA uninstall failed'}
foreach($taskKey in @('Classes\*\shell\StrataCopy','Classes\Directory\shell\StrataCopy','Classes\Directory\shell\StrataCopyPaste','Classes\Directory\Background\shell\StrataCopyPaste')){if(Test-Path -LiteralPath "$taskRegistry\$taskKey"){throw "Registry entry remained after uninstall: $taskKey"}}
if((Get-ItemProperty -LiteralPath "$taskRegistry\Run" -Name StrataCopyExplorer -ErrorAction SilentlyContinue).StrataCopyExplorer){throw 'QA startup entry remained after uninstall'}
$taskAfter=(Get-ItemProperty -LiteralPath $taskActualRun -Name StrataCopyExplorer -ErrorAction SilentlyContinue).StrataCopyExplorer
if($taskAfter -ne $taskBefore){throw 'Real Windows startup configuration changed during QA'}
'PASS: isolated install, optional registrations, shortcut creation, deployed helper self-test and uninstall cleanup.'
'PASS: real Explorer/startup registration was unchanged; no copy UI or background interception was started.'
