#define AppVersion "2.0.0"
#ifdef TEST_INSTALLER
  #define AppIdentity "{0F23BE2C-48E1-47CE-A012-20E7728D2C78}"
  #define InstallDirectory "{localappdata}\Programs\StrataCopy"
  #define RequiredPrivileges "lowest"
  #define RegistryClasses "Software\StrataCopyInstallerQA\Classes"
  #define RegistryRun "Software\StrataCopyInstallerQA\Run"
  #define RegistryPreferences "Software\StrataCopyInstallerQA\Preferences"
  #define ProgramShortcut "{app}\Shortcuts\Programs\StrataCopy"
  #define DesktopShortcut "{app}\Shortcuts\Desktop\StrataCopy"
  #define OutputName "StrataCopy-Installer-QA"
  #define OutputFolder "..\build\installer-qa"
#else
  #define AppIdentity "{730C3722-BAF0-454C-A55E-85577EA4064E}"
  #define InstallDirectory "{autopf}\StrataCopy"
  #define RequiredPrivileges "admin"
  #define RegistryClasses "Software\Classes"
  #define RegistryRun "Software\Microsoft\Windows\CurrentVersion\Run"
  #define RegistryPreferences "Software\StrataCopy\StrataCopy"
  #define ProgramShortcut "{autoprograms}\StrataCopy"
  #define DesktopShortcut "{autodesktop}\StrataCopy"
  #define OutputName "StrataCopy-" + AppVersion + "-Setup-x64"
  #define OutputFolder "..\dist"
#endif

[Setup]
AppId={{#AppIdentity}
AppName=StrataCopy
AppVersion={#AppVersion}
AppPublisher=StrataCopy
DefaultDirName={#InstallDirectory}
PrivilegesRequired={#RequiredPrivileges}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
OutputDir={#OutputFolder}
OutputBaseFilename={#OutputName}
SetupIconFile=..\resources\icons\stratacopy.ico
UninstallDisplayIcon={app}\StrataCopy.exe
UninstallDisplayName=StrataCopy
WizardStyle=modern dark
WizardSmallImageFile=..\resources\icons\stratacopy.png
WizardSizePercent=105
DisableWelcomePage=yes
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=yes
DisableFinishedPage=no
Compression=lzma2/normal
SolidCompression=yes
CloseApplications=yes
CloseApplicationsFilter=StrataCopy.exe,StrataCopyExplorer.exe
RestartApplications=no
SetupLogging=yes

[Messages]
WizardSelectTasks=Install StrataCopy
SelectTasksDesc=Fast file transfers. A familiar Windows copy experience.
SelectTasksLabel2=Optional shortcuts:
FinishedHeadingLabel=You're ready to copy.
FinishedLabel=StrataCopy is ready. Change preferences in Settings or right-click the tray icon.

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts"; Flags: unchecked

[Files]
Source: "..\dist\StrataCopy-{#AppVersion}\*"; Excludes: "StrataCopyNative*.dll"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\StrataCopy-{#AppVersion}\StrataCopyNative-{#AppVersion}-p5.dll"; DestDir: "{app}"; Flags: replacesameversion restartreplace uninsrestartdelete

[Icons]
Name: "{#ProgramShortcut}"; Filename: "{app}\StrataCopy.exe"; WorkingDir: "{app}"
Name: "{#DesktopShortcut}"; Filename: "{app}\StrataCopy.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "{#RegistryPreferences}\Interface"; ValueType: string; ValueName: "transferOnly"; ValueData: "true"
Root: HKCU; Subkey: "{#RegistryPreferences}\Interface"; ValueType: string; ValueName: "detailed"; ValueData: "false"
Root: HKCU; Subkey: "{#RegistryPreferences}\Interface"; ValueType: string; ValueName: "pinned"; ValueData: "false"
Root: HKCU; Subkey: "{#RegistryPreferences}\Interface"; ValueType: string; ValueName: "completionNotifications"; ValueData: "true"
Root: HKCU; Subkey: "{#RegistryPreferences}\Interface"; ValueType: string; ValueName: "notificationAnimations"; ValueData: "true"
Root: HKCU; Subkey: "{#RegistryPreferences}\Interface"; ValueType: dword; ValueName: "afterCopy"; ValueData: "1"
Root: HKCU; Subkey: "{#RegistryPreferences}\Installer"; ValueType: dword; ValueName: "performanceProfile"; ValueData: "0"
Root: HKCU; Subkey: "{#RegistryRun}"; ValueType: string; ValueName: "StrataCopyExplorer"; ValueData: """{app}\StrataCopyExplorer.exe"""; Flags: uninsdeletevalue; Check: ShouldEnableExplorerPaste

[Run]
Filename: "{app}\StrataCopy.exe"; Parameters: "--detect-hardware"; Flags: runhidden waituntilterminated; Check: NotTesting
Filename: "{app}\StrataCopyExplorer.exe"; Flags: runhidden nowait; Check: NotTesting
Filename: "{app}\StrataCopy.exe"; Description: "Open StrataCopy"; Flags: nowait postinstall skipifsilent unchecked; Check: NotTesting

[UninstallRun]
Filename: "{app}\StrataCopyExplorer.exe"; Parameters: "--stop"; Flags: runhidden; RunOnceId: "StopExplorerPaste"; Check: NotTesting

[Code]
function NotTesting: Boolean;
begin
#ifdef TEST_INSTALLER
  Result := False;
#else
  Result := True;
#endif
end;

function ShouldEnableExplorerPaste: Boolean;
var
  ExistingCommand: String;
begin
#ifdef TEST_INSTALLER
  Result := False;
#else
  Result := not RegQueryStringValue(HKCU, '{#RegistryRun}', 'StrataCopyExplorer', ExistingCommand);
#endif
end;

procedure CurStepChanged(CurStep: TSetupStep);
var ResultCode: Integer;
begin
  if (CurStep = ssInstall) and NotTesting and FileExists(ExpandConstant('{app}\StrataCopyExplorer.exe')) then
    Exec(ExpandConstant('{app}\StrataCopyExplorer.exe'), '--stop', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  if CurStep = ssInstall then begin
    { Clear registrations created by older installer versions. Integration remains available in app Settings. }
    RegDeleteValue(HKCU, '{#RegistryRun}', 'StrataCopyExplorer');
    RegDeleteKeyIncludingSubkeys(HKCU, '{#RegistryClasses}\*\shell\StrataCopy');
    RegDeleteKeyIncludingSubkeys(HKCU, '{#RegistryClasses}\Directory\shell\StrataCopy');
    RegDeleteKeyIncludingSubkeys(HKCU, '{#RegistryClasses}\Directory\shell\StrataCopyPaste');
    RegDeleteKeyIncludingSubkeys(HKCU, '{#RegistryClasses}\Directory\Background\shell\StrataCopyPaste');
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then begin
    RegDeleteValue(HKCU, '{#RegistryRun}', 'StrataCopyExplorer');
    RegDeleteKeyIncludingSubkeys(HKCU, '{#RegistryClasses}\*\shell\StrataCopy');
    RegDeleteKeyIncludingSubkeys(HKCU, '{#RegistryClasses}\Directory\shell\StrataCopy');
    RegDeleteKeyIncludingSubkeys(HKCU, '{#RegistryClasses}\Directory\shell\StrataCopyPaste');
    RegDeleteKeyIncludingSubkeys(HKCU, '{#RegistryClasses}\Directory\Background\shell\StrataCopyPaste');
  end;
end;
