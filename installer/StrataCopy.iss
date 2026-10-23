#define AppVersion "1.1.0"
#ifdef TEST_INSTALLER
  #define AppIdentity "{0F23BE2C-48E1-47CE-A012-20E7728D2C78}"
  #define RegistryClasses "Software\StrataCopyInstallerQA\Classes"
  #define RegistryRun "Software\StrataCopyInstallerQA\Run"
  #define RegistryPreferences "Software\StrataCopyInstallerQA\Preferences"
  #define ProgramShortcut "{app}\Shortcuts\Programs\StrataCopy"
  #define DesktopShortcut "{app}\Shortcuts\Desktop\StrataCopy"
  #define OutputName "StrataCopy-Installer-QA"
  #define OutputFolder "..\build\installer-qa"
#else
  #define AppIdentity "{730C3722-BAF0-454C-A55E-85577EA4064E}"
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
FinishedLabel=StrataCopy is ready. Change preferences in Settings or right-click the tray icon.

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts"; Flags: unchecked
Name: "explorermenu"; Description: "Add Copy / Paste with StrataCopy to Explorer"; GroupDescription: "Explorer integration"
Name: "keyboardpaste"; Description: "Use StrataCopy for Explorer copy, move and delete"; GroupDescription: "Explorer integration"; Flags: unchecked

[Files]
Source: "..\dist\StrataCopy-{#AppVersion}\*"; Excludes: "StrataCopyNative*.dll"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\StrataCopy-{#AppVersion}\StrataCopyNative-{#AppVersion}-p5.dll"; DestDir: "{app}"; Flags: replacesameversion restartreplace uninsrestartdelete

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

Root: HKCU; Subkey: "{#RegistryPreferences}\Interface"; ValueType: string; ValueName: "transferOnly"; ValueData: "{code:PreferenceValue|0}"
Root: HKCU; Subkey: "{#RegistryPreferences}\Interface"; ValueType: string; ValueName: "detailed"; ValueData: "{code:PreferenceValue|1}"
Root: HKCU; Subkey: "{#RegistryPreferences}\Interface"; ValueType: string; ValueName: "pinned"; ValueData: "{code:PreferenceValue|2}"
Root: HKCU; Subkey: "{#RegistryPreferences}\Interface"; ValueType: string; ValueName: "completionNotifications"; ValueData: "{code:PreferenceValue|3}"
Root: HKCU; Subkey: "{#RegistryPreferences}\Interface"; ValueType: string; ValueName: "notificationAnimations"; ValueData: "{code:PreferenceValue|4}"
Root: HKCU; Subkey: "{#RegistryPreferences}\Interface"; ValueType: dword; ValueName: "afterCopy"; ValueData: "{code:CompletionValue}"
Root: HKCU; Subkey: "{#RegistryPreferences}\Installer"; ValueType: dword; ValueName: "performanceProfile"; ValueData: "{code:PerformanceValue}"; Check: PerformanceSelected

[Run]
Filename: "{app}\StrataCopy.exe"; Parameters: "--detect-hardware"; Flags: runhidden waituntilterminated; Check: NotTesting
Filename: "{app}\StrataCopyExplorer.exe"; Flags: nowait runhidden; Tasks: keyboardpaste; Check: NotTesting
Filename: "{app}\StrataCopy.exe"; Description: "Open StrataCopy"; Flags: nowait postinstall skipifsilent unchecked; Check: NotTesting

[UninstallRun]
Filename: "{app}\StrataCopyExplorer.exe"; Parameters: "--stop"; Flags: runhidden; RunOnceId: "StopExplorerPaste"; Check: NotTesting

[Code]
var
  ScopeNote: TNewStaticText;
  FolderNote: TNewStaticText;
  PreferencesPage: TInputOptionWizardPage;
  PerformancePage: TInputOptionWizardPage;
  PreviousCompletion: Cardinal;

function PreferenceValue(Param: String): String;
begin
  if PreferencesPage.Values[StrToIntDef(Param, 0)] then Result := 'true' else Result := 'false';
end;

function CompletionValue(Param: String): String;
begin
  if WizardSilent then Result := IntToStr(PreviousCompletion)
  else if PreferencesPage.Values[5] then Result := '1' else Result := '0';
end;

function PerformanceSelected: Boolean;
begin
  Result := PerformancePage.SelectedValueIndex > 0;
end;

function PerformanceValue(Param: String): String;
begin
  Result := IntToStr(PerformancePage.SelectedValueIndex - 1);
end;

procedure LoadPreference(Index: Integer; Name: String; DefaultValue: Boolean);
var Existing: String;
begin
  PreferencesPage.Values[Index] := DefaultValue;
  if RegQueryStringValue(HKCU, '{#RegistryPreferences}\Interface', Name, Existing) then
    PreferencesPage.Values[Index] := (Lowercase(Existing) = 'true') or (Existing = '1');
end;

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
  ScopeNote.Height := ScaleY(54);
  ScopeNote.AutoSize := False;
  ScopeNote.WordWrap := True;
  ScopeNote.Caption := 'Optional Explorer integration handles copy, move and delete from keyboard shortcuts and menus. Virtual items and unsupported operations use Windows. Disable it anytime in Settings.';
  FolderNote := TNewStaticText.Create(WizardForm);
  FolderNote.Parent := WizardForm.SelectTasksPage;
  FolderNote.Left := ScaleX(0);
  FolderNote.Top := ScopeNote.Top + ScopeNote.Height + ScaleY(6);
  FolderNote.Width := WizardForm.TasksList.Width;
  FolderNote.Height := ScaleY(20);
  FolderNote.AutoSize := False;
  FolderNote.WordWrap := True;
  FolderNote.Caption := 'Installs for your account. No administrator access required.';
  { Keep notes above the navigation bar at every supported DPI. }
  FolderNote.Top := WizardForm.SelectTasksPage.ClientHeight - FolderNote.Height - ScaleY(2);
  ScopeNote.Top := FolderNote.Top - ScopeNote.Height - ScaleY(6);
  WizardForm.TasksList.Height := ScopeNote.Top - WizardForm.TasksList.Top - ScaleY(8);
  PreferencesPage := CreateInputOptionPage(wpSelectTasks, 'Make it yours',
    'Choose how StrataCopy looks and behaves', 'You can change these choices later in Settings or the tray menu.', False, False);
  PreferencesPage.Add('Show the window only during transfers (tray mode)');
  PreferencesPage.Add('Use the detailed transfer window by default');
  PreferencesPage.Add('Keep the transfer window on top');
  PreferencesPage.Add('Show completion notifications with elapsed time');
  PreferencesPage.Add('Animate notifications: slide up and slide down');
  PreferencesPage.Add('Close the window after success (otherwise keep open)');
  LoadPreference(0, 'transferOnly', False);
  LoadPreference(1, 'detailed', False);
  LoadPreference(2, 'pinned', False);
  LoadPreference(3, 'completionNotifications', True);
  LoadPreference(4, 'notificationAnimations', True);
  PreviousCompletion := 1;
  RegQueryDWordValue(HKCU, '{#RegistryPreferences}\Interface', 'afterCopy', PreviousCompletion);
  PreferencesPage.Values[5] := PreviousCompletion = 1;
  PerformancePage := CreateInputOptionPage(PreferencesPage.ID, 'Performance profile',
    'Choose a starting point for this PC',
    'Auto checks CPU, RAM and both transfer drives. You can adjust performance later in Settings.', True, False);
  PerformancePage.Add('Keep my current performance settings (upgrades)');
  PerformancePage.Add('Auto (recommended): hardware-aware thread limits');
  PerformancePage.Add('Safe / Silent: 4 threads');
  PerformancePage.Add('NVMe Extreme: 32 threads, no retries');
  if RegKeyExists(HKCU, '{#RegistryPreferences}\Interface') then PerformancePage.SelectedValueIndex := 0
  else PerformancePage.SelectedValueIndex := 1;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = PerformancePage.ID then WizardForm.NextButton.Caption := 'Install'
  else if (CurPageID = wpSelectTasks) or (CurPageID = PreferencesPage.ID) then WizardForm.NextButton.Caption := 'Next';
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
