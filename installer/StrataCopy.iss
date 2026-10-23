#define AppVersion "1.0.0"
#ifdef TEST_INSTALLER
  #define AppIdentity "{0F23BE2C-48E1-47CE-A012-20E7728D2C78}"
  #define RegistryClasses "Software\StrataCopyInstallerQA\Classes"
  #define RegistryRun "Software\StrataCopyInstallerQA\Run"
  #define ProgramShortcut "{app}\Shortcuts\Programs\StrataCopy"
  #define DesktopShortcut "{app}\Shortcuts\Desktop\StrataCopy"
  #define OutputName "StrataCopy-Installer-QA"
  #define OutputFolder "..\build\installer-qa"
#else
  #define AppIdentity "{730C3722-BAF0-454C-A55E-85577EA4064E}"
  #define RegistryClasses "Software\Classes"
  #define RegistryRun "Software\Microsoft\Windows\CurrentVersion\Run"
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
DefaultDirName={localappdata}\Programs\StrataCopy
PrivilegesRequired=lowest
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
SelectTasksDesc=Fast file transfers. A smaller, simpler copy window.
SelectTasksLabel2=Choose what works for you:
FinishedHeadingLabel=You're ready to copy.
FinishedLabel=StrataCopy is installed. You can change Explorer integration from History > Explorer menu.

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts"; Flags: unchecked
Name: "explorermenu"; Description: "Add Copy / Paste with StrataCopy to Explorer"; GroupDescription: "Explorer integration"
Name: "keyboardpaste"; Description: "Use StrataCopy as Explorer's file copier (Paste and drag-and-drop)"; GroupDescription: "Explorer integration"; Flags: unchecked

[Files]
Source: "..\dist\StrataCopy-{#AppVersion}\*"; Excludes: "StrataCopyNative*.dll"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\StrataCopy-{#AppVersion}\StrataCopyNative-{#AppVersion}.dll"; DestDir: "{app}"; Flags: ignoreversion restartreplace uninsrestartdelete

[Icons]
Name: "{#ProgramShortcut}"; Filename: "{app}\StrataCopy.exe"; WorkingDir: "{app}"
Name: "{#DesktopShortcut}"; Filename: "{app}\StrataCopy.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "{#RegistryClasses}\*\shell\StrataCopy"; ValueType: string; ValueName: ""; ValueData: "Copy with StrataCopy"; Flags: uninsdeletekey; Tasks: explorermenu
Root: HKCU; Subkey: "{#RegistryClasses}\*\shell\StrataCopy"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\StrataCopy.exe"; Tasks: explorermenu
Root: HKCU; Subkey: "{#RegistryClasses}\*\shell\StrataCopy"; ValueType: string; ValueName: "MultiSelectModel"; ValueData: "Player"; Tasks: explorermenu
Root: HKCU; Subkey: "{#RegistryClasses}\*\shell\StrataCopy\command"; ValueType: string; ValueData: """{app}\StrataCopy.exe"" --add ""%1"""; Tasks: explorermenu
Root: HKCU; Subkey: "{#RegistryClasses}\Directory\shell\StrataCopy"; ValueType: string; ValueData: "Copy with StrataCopy"; Flags: uninsdeletekey; Tasks: explorermenu
Root: HKCU; Subkey: "{#RegistryClasses}\Directory\shell\StrataCopy"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\StrataCopy.exe"; Tasks: explorermenu
Root: HKCU; Subkey: "{#RegistryClasses}\Directory\shell\StrataCopy\command"; ValueType: string; ValueData: """{app}\StrataCopy.exe"" --add ""%1"""; Tasks: explorermenu
Root: HKCU; Subkey: "{#RegistryClasses}\Directory\shell\StrataCopyPaste"; ValueType: string; ValueData: "Paste with StrataCopy"; Flags: uninsdeletekey; Tasks: explorermenu
Root: HKCU; Subkey: "{#RegistryClasses}\Directory\shell\StrataCopyPaste"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\StrataCopy.exe"; Tasks: explorermenu
Root: HKCU; Subkey: "{#RegistryClasses}\Directory\shell\StrataCopyPaste\command"; ValueType: string; ValueData: """{app}\StrataCopyExplorer.exe"" --paste ""%1"""; Tasks: explorermenu
Root: HKCU; Subkey: "{#RegistryClasses}\Directory\Background\shell\StrataCopyPaste"; ValueType: string; ValueData: "Paste with StrataCopy"; Flags: uninsdeletekey; Tasks: explorermenu
Root: HKCU; Subkey: "{#RegistryClasses}\Directory\Background\shell\StrataCopyPaste"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\StrataCopy.exe"; Tasks: explorermenu
Root: HKCU; Subkey: "{#RegistryClasses}\Directory\Background\shell\StrataCopyPaste\command"; ValueType: string; ValueData: """{app}\StrataCopyExplorer.exe"" --paste ""%V"""; Tasks: explorermenu
Root: HKCU; Subkey: "{#RegistryRun}"; ValueType: string; ValueName: "StrataCopyExplorer"; ValueData: """{app}\StrataCopyExplorer.exe"""; Flags: uninsdeletevalue; Tasks: keyboardpaste

[Run]
Filename: "{app}\StrataCopyExplorer.exe"; Flags: nowait runhidden; Tasks: keyboardpaste; Check: NotTesting
Filename: "{app}\StrataCopy.exe"; Description: "Open StrataCopy"; Flags: nowait postinstall skipifsilent unchecked; Check: NotTesting

[UninstallRun]
Filename: "{app}\StrataCopyExplorer.exe"; Parameters: "--stop"; Flags: runhidden; RunOnceId: "StopExplorerPaste"; Check: NotTesting

[Code]
var
  ScopeNote: TNewStaticText;
  FolderNote: TNewStaticText;

function NotTesting: Boolean;
begin
#ifdef TEST_INSTALLER
  Result := False;
#else
  Result := True;
#endif
end;

procedure InitializeWizard;
begin
  ScopeNote := TNewStaticText.Create(WizardForm);
  ScopeNote.Parent := WizardForm.SelectTasksPage;
  ScopeNote.Left := ScaleX(0);
  ScopeNote.Top := WizardForm.TasksList.Top + ScaleY(145) + ScaleY(8);
  ScopeNote.Width := WizardForm.TasksList.Width;
  ScopeNote.Height := ScaleY(62);
  ScopeNote.AutoSize := False;
  ScopeNote.WordWrap := True;
  ScopeNote.Caption := 'Optional native integration handles Explorer file copy/move, including keyboard Paste, menu Paste and drag-and-drop. Virtual items and unsupported operations use Windows. Disabling takes effect immediately.';
  FolderNote := TNewStaticText.Create(WizardForm);
  FolderNote.Parent := WizardForm.SelectTasksPage;
  FolderNote.Left := ScaleX(0);
  FolderNote.Top := ScopeNote.Top + ScopeNote.Height + ScaleY(6);
  FolderNote.Width := WizardForm.TasksList.Width;
  FolderNote.Height := ScaleY(35);
  FolderNote.AutoSize := False;
  FolderNote.WordWrap := True;
  FolderNote.Caption := 'Installs for your account. No administrator access required.';
  WizardForm.TasksList.Height := ScaleY(145);
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpSelectTasks then WizardForm.NextButton.Caption := 'Install';
end;

procedure CurStepChanged(CurStep: TSetupStep);
var ResultCode: Integer;
begin
  if (CurStep = ssInstall) and NotTesting and FileExists(ExpandConstant('{app}\StrataCopyExplorer.exe')) then
    Exec(ExpandConstant('{app}\StrataCopyExplorer.exe'), '--stop', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  if CurStep = ssInstall then begin
    if not WizardIsTaskSelected('keyboardpaste') then
      RegDeleteValue(HKCU, '{#RegistryRun}', 'StrataCopyExplorer');
    if not WizardIsTaskSelected('explorermenu') then begin
      RegDeleteKeyIncludingSubkeys(HKCU, '{#RegistryClasses}\*\shell\StrataCopy');
      RegDeleteKeyIncludingSubkeys(HKCU, '{#RegistryClasses}\Directory\shell\StrataCopy');
      RegDeleteKeyIncludingSubkeys(HKCU, '{#RegistryClasses}\Directory\shell\StrataCopyPaste');
      RegDeleteKeyIncludingSubkeys(HKCU, '{#RegistryClasses}\Directory\Background\shell\StrataCopyPaste');
    end;
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
