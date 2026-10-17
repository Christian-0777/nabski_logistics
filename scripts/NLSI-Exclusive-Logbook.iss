; NLSI Exclusive Logbook installer
; Product version: 1.8.6 Beta

#define MyAppName "NLSI Exclusive Logbook"
#define MyAppVersion "1.8.6 Beta"
#define MyAppPublisher "NABSKI Logistics Solutions Inc."
#define MyAppExeName "nlsi_dashboard.exe"
#define PluginFileName "nlsi_scs_plugin.dll"

[Setup]
AppId={{B58E42DA-BC15-4FD0-9DCC-44E4E14D374D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={code:GetDefaultInstallDir}
DefaultGroupName={#MyAppName}
OutputDir=..\releases\1.8.6-beta
OutputBaseFilename=NLSI-Exclusive-Logbook-v1.8.6-beta-Setup
SetupIconFile=..\resources\nlsi.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
VersionInfoVersion=1.8.6.0
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppName} Setup - {#MyAppVersion}
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion=1.8.6.0
ArchitecturesAllowed=x64os
ArchitecturesInstallIn64BitMode=x64os
PrivilegesRequired=admin
CloseApplications=yes
CloseApplicationsFilter={#MyAppExeName}
RestartApplications=no
SetupLogging=yes
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "{#ReleasePayloadDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ReleasePayloadDir}\.nlsi-managed-files"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#ReleasePayloadDir}\plugin\win_x64\{#PluginFileName}"; DestDir: "{code:GetETS2X64PluginDirectory}"; Flags: ignoreversion restartreplace; Check: ShouldInstallETS2X64; AfterInstall: MarkETS2X64PluginInstalled
Source: "{#ReleasePayloadDir}\plugin\win_x86\{#PluginFileName}"; DestDir: "{code:GetETS2X86PluginDirectory}"; Flags: ignoreversion restartreplace; Check: ShouldInstallETS2X86; AfterInstall: MarkETS2X86PluginInstalled
Source: "{#ReleasePayloadDir}\plugin\win_x64\{#PluginFileName}"; DestDir: "{code:GetATSX64PluginDirectory}"; Flags: ignoreversion restartreplace; Check: ShouldInstallATSX64; AfterInstall: MarkATSX64PluginInstalled
Source: "{#ReleasePayloadDir}\plugin\win_x86\{#PluginFileName}"; DestDir: "{code:GetATSX86PluginDirectory}"; Flags: ignoreversion restartreplace; Check: ShouldInstallATSX86; AfterInstall: MarkATSX86PluginInstalled

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\resources\nlsi.ico"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\resources\nlsi.ico"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent

[Code]
const
  AppUninstallKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{B58E42DA-BC15-4FD0-9DCC-44E4E14D374D}_is1';
  ProductName = 'NLSI Exclusive Logbook';
  ProductVersion = '1.8.6 Beta';
  ETS2FolderName = 'Euro Truck Simulator 2';
  ATSFolderName = 'American Truck Simulator';
  PluginDllName = 'nlsi_scs_plugin.dll';
  PluginMarkerSuffix = '.nlsi-owner';
  PluginBackupSuffix = '.nlsi-backup.dll';
  TikTokProfileUrl = 'https://www.tiktok.com/@nabskiplays';

var
  ETS2RootPath: String;
  ATSRootPath: String;
  SkippedPluginTargets: String;
  PreviousManagedFiles: TStringList;
  TikTokFollowCheck: TNewCheckBox;

procedure InitializeWizard;
begin
  TikTokFollowCheck := TNewCheckBox.Create(WizardForm);
  TikTokFollowCheck.Parent := WizardForm.FinishedPage;
  TikTokFollowCheck.Left := WizardForm.FinishedLabel.Left;
  TikTokFollowCheck.Top := WizardForm.FinishedLabel.Top +
    WizardForm.FinishedLabel.Height + ScaleY(12);
  TikTokFollowCheck.Width := WizardForm.FinishedPage.ClientWidth -
    TikTokFollowCheck.Left - ScaleX(16);
  TikTokFollowCheck.Caption := 'Follow Nabski Plays on TikTok';
  TikTokFollowCheck.Checked := False;
end;

function IsSafeManagedRelativePath(const RelativePath: String): Boolean;
begin
  Result := (RelativePath <> '') and
    (RelativePath[1] <> '\') and
    (Pos(':', RelativePath) = 0) and
    (Pos('/', RelativePath) = 0) and
    (Pos('..', RelativePath) = 0);
end;

procedure RemoveObsoleteManagedFiles;
var
  CurrentManagedFiles: TStringList;
  Index: Integer;
  RelativePath: String;
  TargetFile: String;
begin
  if (PreviousManagedFiles = nil) or (PreviousManagedFiles.Count = 0) then
    Exit;

  CurrentManagedFiles := TStringList.Create;
  try
    CurrentManagedFiles.CaseSensitive := False;
    try
      CurrentManagedFiles.LoadFromFile(
        ExpandConstant('{app}\.nlsi-managed-files'));
    except
      RaiseException('Could not read the new NLSI managed-file list.');
    end;

    for Index := 0 to PreviousManagedFiles.Count - 1 do
    begin
      RelativePath := Trim(PreviousManagedFiles[Index]);
      if not IsSafeManagedRelativePath(RelativePath) or
         (CurrentManagedFiles.IndexOf(RelativePath) >= 0) then
        Continue;
      TargetFile := ExpandConstant('{app}\') + RelativePath;
      if FileExists(TargetFile) and not DeleteFile(TargetFile) then
        RaiseException('Could not remove an obsolete NLSI-managed file: ' + TargetFile);
    end;
  finally
    CurrentManagedFiles.Free;
  end;
end;

procedure AddUniquePath(Paths: TStringList; const Path: String);
var
  NormalizedPath: String;
begin
  NormalizedPath := RemoveBackslashUnlessRoot(Path);
  if (NormalizedPath <> '') and (Paths.IndexOf(NormalizedPath) < 0) then
    Paths.Add(NormalizedPath);
end;

procedure AddSteamLibraryPaths(Paths: TStringList; const SteamRoot: String);
var
  VdfFile: String;
  VdfContents: AnsiString;
  SearchFrom: Integer;
  KeyPosition: Integer;
  FirstQuote: Integer;
  SecondQuote: Integer;
  LibraryPath: String;
begin
  if SteamRoot = '' then
    Exit;

  AddUniquePath(Paths, SteamRoot);
  VdfFile := AddBackslash(SteamRoot) + 'steamapps\libraryfolders.vdf';
  if not LoadStringFromFile(VdfFile, VdfContents) then
    Exit;

  SearchFrom := 1;
  while SearchFrom <= Length(VdfContents) do
  begin
    KeyPosition := Pos('"path"', Copy(VdfContents, SearchFrom, Length(VdfContents)));
    if KeyPosition = 0 then
      Break;
    KeyPosition := KeyPosition + SearchFrom - 1;
    FirstQuote := Pos('"', Copy(VdfContents, KeyPosition + 6, Length(VdfContents)));
    if FirstQuote = 0 then
      Break;
    FirstQuote := KeyPosition + 6 + FirstQuote - 1;
    SecondQuote := Pos('"', Copy(VdfContents, FirstQuote + 1, Length(VdfContents)));
    if SecondQuote = 0 then
      Break;
    SecondQuote := FirstQuote + SecondQuote;
    LibraryPath := Copy(VdfContents, FirstQuote + 1, SecondQuote - FirstQuote - 1);
    StringChangeEx(LibraryPath, '\\', '\', True);
    StringChangeEx(LibraryPath, '/', '\', True);
    AddUniquePath(Paths, LibraryPath);
    SearchFrom := SecondQuote + 1;
  end;
end;

function SteamLibraries: TStringList;
var
  Paths: TStringList;
  SteamRoot: String;
begin
  Paths := TStringList.Create;
  AddSteamLibraryPaths(Paths, 'C:\Program Files (x86)\Steam');
  if RegQueryStringValue(HKCU, 'Software\Valve\Steam', 'SteamPath', SteamRoot) or
     RegQueryStringValue(HKCU, 'Software\Valve\Steam', 'InstallPath', SteamRoot) or
     RegQueryStringValue(HKLM32, 'Software\Valve\Steam', 'InstallPath', SteamRoot) or
     RegQueryStringValue(HKLM64, 'Software\Valve\Steam', 'InstallPath', SteamRoot) then
    AddSteamLibraryPaths(Paths, SteamRoot);
  Result := Paths;
end;

function FindGameRoot(const GameFolder: String; const ExecutableName: String): String;
var
  Libraries: TStringList;
  Index: Integer;
  Candidate: String;
begin
  Result := '';
  Libraries := SteamLibraries;
  try
    for Index := 0 to Libraries.Count - 1 do
    begin
      Candidate := AddBackslash(Libraries[Index]) + 'steamapps\common\' + GameFolder;
      if FileExists(Candidate + '\bin\win_x64\' + ExecutableName) or
         FileExists(Candidate + '\bin\win_x86\' + ExecutableName) then
      begin
        Result := Candidate;
        Break;
      end;
    end;
  finally
    Libraries.Free;
  end;
end;

function IsRecognizedNLSIMarker(const Marker: String): Boolean;
begin
  Result :=
    (Marker = 'NLSI Exclusive Logbook 1.8.6 Beta') or
    (Marker = 'NLSI Exclusive Logbook 1.8.5 Beta') or
    (Marker = 'NLSI Exclusive Logbook 1.8.4 Beta') or
    (Marker = 'NLSI Exclusive Logbook 1.8.3 Beta') or
    (Marker = 'NLSI Exclusive Logbook 1.8.2 Beta') or
    (Marker = 'NLSI Exclusive Logbook 1.8.1 Beta') or
    (Marker = 'NLSI Exclusive Logbook 1.8.0 Beta') or
    (Marker = 'NLSI Exclusive Logbook 1.7.9 Beta') or
    (Marker = 'NLSI Exclusive Logbook 1.7.8 Beta') or
    (Marker = 'NLSI Exclusive Logbook 1.7.7 Beta') or
    (Marker = 'NLSI Exclusive Logbook 1.7.6 Beta') or
    (Marker = 'NLSI Exclusive Logbook 1.7.5 Beta') or
    (Marker = 'NLSI Exclusive Logbook 1.7.4 Beta') or
    (Marker = 'NLSI Exclusive Logbook 1.7.3 Beta') or
    (Marker = 'NLSI Exclusive Logbook 1.7.2 Beta') or
    (Marker = 'NLSI Exclusive Logbook 1.7.1 Beta') or
    (Marker = 'NLSI Exclusive Logbook 1.7.0 Beta') or
    (Marker = 'NLSI Exclusive Logbook 1.6.9 Beta') or
    (Marker = 'NLSI Exclusive Logbook 1.6.8 Beta') or
    (Marker = 'NLSI Exclusive Logbook 1.6.7 Beta') or
    (Marker = 'NLSI Exclusive Logbook 1.6.6 Beta') or
    (Marker = 'NLSI Exclusive Logbook 1.6.5 Beta') or
    (Pos('NLSI Exclusive Logbook 1.8.6 Beta|', Marker) = 1) or
    (Pos('NLSI Exclusive Logbook 1.8.5 Beta|', Marker) = 1) or
    (Pos('NLSI Exclusive Logbook 1.8.4 Beta|', Marker) = 1) or
    (Pos('NLSI Exclusive Logbook 1.8.3 Beta|', Marker) = 1) or
    (Pos('NLSI Exclusive Logbook 1.8.2 Beta|', Marker) = 1) or
    (Pos('NLSI Exclusive Logbook 1.8.1 Beta|', Marker) = 1) or
    (Pos('NLSI Exclusive Logbook 1.8.0 Beta|', Marker) = 1) or
    (Pos('NLSI Exclusive Logbook 1.7.9 Beta|', Marker) = 1) or
    (Pos('NLSI Exclusive Logbook 1.7.8 Beta|', Marker) = 1) or
    (Pos('NLSI Exclusive Logbook 1.7.7 Beta|', Marker) = 1) or
    (Pos('NLSI Exclusive Logbook 1.7.6 Beta|', Marker) = 1) or
    (Pos('NLSI Exclusive Logbook 1.7.5 Beta|', Marker) = 1) or
    (Pos('NLSI Exclusive Logbook 1.7.4 Beta|', Marker) = 1) or
    (Pos('NLSI Exclusive Logbook 1.7.3 Beta|', Marker) = 1) or
    (Pos('NLSI Exclusive Logbook 1.7.2 Beta|', Marker) = 1) or
    (Pos('NLSI Exclusive Logbook 1.7.1 Beta|', Marker) = 1) or
    (Pos('NLSI Exclusive Logbook 1.7.0 Beta|', Marker) = 1) or
    (Pos('NLSI Exclusive Logbook 1.6.9 Beta|', Marker) = 1) or
    (Pos('NLSI Exclusive Logbook 1.6.8 Beta|', Marker) = 1) or
    (Pos('NLSI Exclusive Logbook 1.6.7 Beta|', Marker) = 1) or
    (Pos('NLSI Exclusive Logbook 1.6.6 Beta|', Marker) = 1) or
    (Pos('NLSI Exclusive Logbook 1.6.5 Beta|', Marker) = 1);
end;

function PluginCanBeReplaced(const TargetFile: String): Boolean;
var
  MarkerContents: AnsiString;
  Marker: String;
  Separator: Integer;
  ExpectedHash: String;
begin
  Result := not FileExists(TargetFile);
  if Result or not FileExists(TargetFile + PluginMarkerSuffix) then
    Exit;

  if not LoadStringFromFile(TargetFile + PluginMarkerSuffix, MarkerContents) then
    Exit;
  Marker := String(MarkerContents);
  Result := IsRecognizedNLSIMarker(Marker);
  Separator := Pos('|', Marker);
  if Result and (Separator > 0) then
  begin
    ExpectedHash := Copy(Marker, Separator + 1, Length(Marker));
    Result := (ExpectedHash <> '') and
      (CompareText(GetSHA256OfFile(TargetFile), ExpectedHash) = 0);
  end;
end;

function ETS2X64Directory: String;
begin
  Result := AddBackslash(ETS2RootPath) + 'bin\win_x64';
end;

function ETS2X86Directory: String;
begin
  Result := AddBackslash(ETS2RootPath) + 'bin\win_x86';
end;

function ATSX64Directory: String;
begin
  Result := AddBackslash(ATSRootPath) + 'bin\win_x64';
end;

function ATSX86Directory: String;
begin
  Result := AddBackslash(ATSRootPath) + 'bin\win_x86';
end;

function ETS2X64PluginPath: String;
begin
  Result := AddBackslash(ETS2X64Directory) + 'plugins\' + PluginDllName;
end;

function ETS2X86PluginPath: String;
begin
  Result := AddBackslash(ETS2X86Directory) + 'plugins\' + PluginDllName;
end;

function GetETS2X64PluginDirectory(Param: String): String;
begin
  Result := AddBackslash(ETS2X64Directory) + 'plugins';
end;

function GetETS2X86PluginDirectory(Param: String): String;
begin
  Result := AddBackslash(ETS2X86Directory) + 'plugins';
end;

function ATSX64PluginPath: String;
begin
  Result := AddBackslash(ATSX64Directory) + 'plugins\' + PluginDllName;
end;

function ATSX86PluginPath: String;
begin
  Result := AddBackslash(ATSX86Directory) + 'plugins\' + PluginDllName;
end;

function GetATSX64PluginDirectory(Param: String): String;
begin
  Result := AddBackslash(ATSX64Directory) + 'plugins';
end;

function GetATSX86PluginDirectory(Param: String): String;
begin
  Result := AddBackslash(ATSX86Directory) + 'plugins';
end;

function GetDefaultInstallDir(Param: String): String;
var
  PreviousInstallDir: String;
begin
  Result := ExpandConstant('{autopf}\Nabski Logistics Solutions Inc');
  if RegQueryStringValue(HKLM64, AppUninstallKey, 'InstallLocation', PreviousInstallDir) or
     RegQueryStringValue(HKLM32, AppUninstallKey, 'InstallLocation', PreviousInstallDir) or
     RegQueryStringValue(HKCU, AppUninstallKey, 'InstallLocation', PreviousInstallDir) then
  begin
    if DirExists(PreviousInstallDir) then
      Result := PreviousInstallDir;
  end;
end;

function GetETS2X64Payload(Param: String): String;
begin
  Result := ExpandConstant('{app}\plugin\win_x64\' + PluginDllName);
end;

function GetETS2X86Payload(Param: String): String;
begin
  Result := ExpandConstant('{app}\plugin\win_x86\' + PluginDllName);
end;

function GetATSX64Payload(Param: String): String;
begin
  Result := ExpandConstant('{app}\plugin\win_x64\' + PluginDllName);
end;

function GetATSX86Payload(Param: String): String;
begin
  Result := ExpandConstant('{app}\plugin\win_x86\' + PluginDllName);
end;

function HasMatchingGameExecutable(const ArchitectureDirectory: String;
  const ExecutableName: String): Boolean;
begin
  Result := DirExists(ArchitectureDirectory) and
    FileExists(AddBackslash(ArchitectureDirectory) + ExecutableName);
end;

function IsPluginTargetInstallable(const GameRoot: String;
  const ArchitectureDirectory: String; const ExecutableName: String;
  const TargetFile: String): Boolean;
begin
  Result := (GameRoot <> '') and
    HasMatchingGameExecutable(ArchitectureDirectory, ExecutableName) and
    PluginCanBeReplaced(TargetFile);
end;

function ShouldInstallETS2X64: Boolean;
begin
  Result := IsPluginTargetInstallable(ETS2RootPath, ETS2X64Directory,
    'eurotrucks2.exe', ETS2X64PluginPath);
end;

function ShouldInstallETS2X86: Boolean;
begin
  Result := IsPluginTargetInstallable(ETS2RootPath, ETS2X86Directory,
    'eurotrucks2.exe', ETS2X86PluginPath);
end;

function ShouldInstallATSX64: Boolean;
begin
  Result := IsPluginTargetInstallable(ATSRootPath, ATSX64Directory,
    'amtrucks.exe', ATSX64PluginPath);
end;

function ShouldInstallATSX86: Boolean;
begin
  Result := IsPluginTargetInstallable(ATSRootPath, ATSX86Directory,
    'amtrucks.exe', ATSX86PluginPath);
end;

function GameProcessIsRunning(const ImageName: String; var ErrorMessage: String): Boolean;
var
  OutputFile: String;
  CommandLine: String;
  ExitCode: Integer;
  OutputText: AnsiString;
begin
  Result := False;
  ErrorMessage := '';
  OutputFile := ExpandConstant('{tmp}\nlsi-process-' + ImageName + '.txt');
  DeleteFile(OutputFile);
  CommandLine := '/C tasklist.exe /FI "IMAGENAME eq ' + ImageName +
    '" /NH /FO CSV > "' + OutputFile + '"';
  if not Exec(ExpandConstant('{sys}\cmd.exe'), CommandLine, '', SW_HIDE,
      ewWaitUntilTerminated, ExitCode) or (ExitCode <> 0) then
  begin
    ErrorMessage := 'Could not determine whether ' + ImageName + ' is running.';
    Exit;
  end;
  if not LoadStringFromFile(OutputFile, OutputText) then
  begin
    ErrorMessage := 'Could not read the process check result for ' + ImageName + '.';
    Exit;
  end;
  DeleteFile(OutputFile);
  Result := Pos(Lowercase(ImageName), Lowercase(String(OutputText))) > 0;
end;

function CheckGamesAreClosed(var ErrorMessage: String): Boolean;
var
  ProcessError: String;
begin
  Result := False;
  ErrorMessage := '';
  if GameProcessIsRunning('eurotrucks2.exe', ProcessError) then
  begin
    ErrorMessage := 'Euro Truck Simulator 2 is running. Close the game and run the installer again.';
    Exit;
  end;
  if ProcessError <> '' then
  begin
    ErrorMessage := ProcessError;
    Exit;
  end;
  if GameProcessIsRunning('amtrucks.exe', ProcessError) then
  begin
    ErrorMessage := 'American Truck Simulator is running. Close the game and run the installer again.';
    Exit;
  end;
  if ProcessError <> '' then
  begin
    ErrorMessage := ProcessError;
    Exit;
  end;
  Result := True;
end;

function PreparePluginTarget(const TargetFile: String; const ArchitectureDirectory: String): Boolean;
var
  BackupFile: String;
begin
  Result := False;
  if not DirExists(ArchitectureDirectory) then
    Exit;
  if not ForceDirectories(ExtractFileDir(TargetFile)) then
    Exit;
  if not PluginCanBeReplaced(TargetFile) then
    Exit;
  if FileExists(TargetFile) then
  begin
    BackupFile := TargetFile + PluginBackupSuffix;
    if not FileExists(BackupFile) and not CopyFile(TargetFile, BackupFile, False) then
      Exit;
  end;
  Result := True;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  GameError: String;
begin
  Result := '';
  if not CheckGamesAreClosed(GameError) then
  begin
    Result := GameError + ' The installer will not close the game automatically.';
    Exit;
  end;
  if PreviousManagedFiles <> nil then
    PreviousManagedFiles.Free;
  PreviousManagedFiles := TStringList.Create;
  PreviousManagedFiles.CaseSensitive := False;
  if FileExists(ExpandConstant('{app}\.nlsi-managed-files')) then
  begin
    try
      PreviousManagedFiles.LoadFromFile(ExpandConstant('{app}\.nlsi-managed-files'));
    except
      PreviousManagedFiles.Free;
      PreviousManagedFiles := nil;
      Result := 'Could not read the existing NLSI managed-file list; application files were not changed.';
      Exit;
    end;
  end;

  if ShouldInstallETS2X64 and not PreparePluginTarget(ETS2X64PluginPath, ETS2X64Directory) then
    Result := 'Could not safely prepare the ETS2 x64 plugin. Close ETS2 and retry.';
  if (Result = '') and ShouldInstallETS2X86 and
     not PreparePluginTarget(ETS2X86PluginPath, ETS2X86Directory) then
    Result := 'Could not safely prepare the ETS2 x86 plugin. Close ETS2 and retry.';
  if (Result = '') and ShouldInstallATSX64 and
     not PreparePluginTarget(ATSX64PluginPath, ATSX64Directory) then
    Result := 'Could not safely prepare the ATS x64 plugin. Close ATS and retry.';
  if (Result = '') and ShouldInstallATSX86 and
     not PreparePluginTarget(ATSX86PluginPath, ATSX86Directory) then
    Result := 'Could not safely prepare the ATS x86 plugin. Close ATS and retry.';
end;

procedure MarkPluginInstalled(const SourceFile: String; const TargetFile: String);
var
  MarkerContents: String;
begin
  if not FileExists(TargetFile) then
    RaiseException('The telemetry plugin replacement could not be staged: ' + TargetFile);
  MarkerContents := ProductName + ' ' + ProductVersion + '|' + GetSHA256OfFile(SourceFile);
  if not SaveStringToFile(TargetFile + PluginMarkerSuffix, MarkerContents, False) then
    RaiseException('Could not write the NLSI ownership marker for: ' + TargetFile);
end;

procedure MarkETS2X64PluginInstalled;
begin
  MarkPluginInstalled(GetETS2X64Payload(''), ETS2X64PluginPath);
end;

procedure MarkETS2X86PluginInstalled;
begin
  MarkPluginInstalled(GetETS2X86Payload(''), ETS2X86PluginPath);
end;

procedure MarkATSX64PluginInstalled;
begin
  MarkPluginInstalled(GetATSX64Payload(''), ATSX64PluginPath);
end;

procedure MarkATSX86PluginInstalled;
begin
  MarkPluginInstalled(GetATSX86Payload(''), ATSX86PluginPath);
end;

procedure RestorePlugin(const TargetFile: String);
var
  MarkerContents: AnsiString;
  Marker: String;
  Separator: Integer;
  ExpectedHash: String;
  BackupFile: String;
begin
  if not FileExists(TargetFile + PluginMarkerSuffix) then
    Exit;
  if not LoadStringFromFile(TargetFile + PluginMarkerSuffix, MarkerContents) then
    Exit;
  Marker := String(MarkerContents);
  if not IsRecognizedNLSIMarker(Marker) or (Pos('|', Marker) = 0) then
    Exit;
  Separator := Pos('|', Marker);
  ExpectedHash := Copy(Marker, Separator + 1, Length(Marker));
  BackupFile := TargetFile + PluginBackupSuffix;
  if not FileExists(TargetFile) or
     (CompareText(GetSHA256OfFile(TargetFile), ExpectedHash) = 0) then
  begin
    if FileExists(BackupFile) then
    begin
      DeleteFile(TargetFile);
      if CopyFile(BackupFile, TargetFile, False) then
      begin
        DeleteFile(BackupFile);
        DeleteFile(TargetFile + PluginMarkerSuffix);
      end;
    end
    else
    begin
      DeleteFile(TargetFile);
      DeleteFile(TargetFile + PluginMarkerSuffix);
    end;
  end;
end;

procedure RestorePluginsUnderRoot(const GameRoot: String);
begin
  if GameRoot = '' then
    Exit;
  RestorePlugin(GameRoot + '\bin\win_x64\plugins\' + PluginDllName);
  RestorePlugin(GameRoot + '\bin\win_x86\plugins\' + PluginDllName);
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  GameError: String;
begin
  if CurStep = ssInstall then
  begin
    if not CheckGamesAreClosed(GameError) then
      RaiseException(GameError + ' The installer will not close the game automatically.');
  end;
  if CurStep = ssPostInstall then
  begin
    if not CheckGamesAreClosed(GameError) then
      RaiseException(GameError + ' No game plugin DLL was replaced.');
    SkippedPluginTargets := '';
    if DirExists(ETS2X64Directory) and FileExists(ETS2X64PluginPath) and
       not PluginCanBeReplaced(ETS2X64PluginPath) then
      SkippedPluginTargets := SkippedPluginTargets + ETS2X64PluginPath +
        ' (existing DLL is not an unchanged NLSI-managed file; it was not replaced)';
    if DirExists(ETS2X86Directory) and FileExists(ETS2X86PluginPath) and
       not PluginCanBeReplaced(ETS2X86PluginPath) then
    begin
      if SkippedPluginTargets <> '' then
        SkippedPluginTargets := SkippedPluginTargets + Chr(13) + Chr(10);
      SkippedPluginTargets := SkippedPluginTargets + ETS2X86PluginPath +
        ' (existing DLL is not an unchanged NLSI-managed file; it was not replaced)';
    end;
    if DirExists(ATSX64Directory) and FileExists(ATSX64PluginPath) and
       not PluginCanBeReplaced(ATSX64PluginPath) then
    begin
      if SkippedPluginTargets <> '' then
        SkippedPluginTargets := SkippedPluginTargets + Chr(13) + Chr(10);
      SkippedPluginTargets := SkippedPluginTargets + ATSX64PluginPath +
        ' (existing DLL is not an unchanged NLSI-managed file; it was not replaced)';
    end;
    if DirExists(ATSX86Directory) and FileExists(ATSX86PluginPath) and
       not PluginCanBeReplaced(ATSX86PluginPath) then
    begin
      if SkippedPluginTargets <> '' then
        SkippedPluginTargets := SkippedPluginTargets + Chr(13) + Chr(10);
      SkippedPluginTargets := SkippedPluginTargets + ATSX86PluginPath +
        ' (existing DLL is not an unchanged NLSI-managed file; it was not replaced)';
    end;
    RemoveObsoleteManagedFiles;
    if ETS2RootPath = '' then
    begin
      if SkippedPluginTargets <> '' then
        SkippedPluginTargets := SkippedPluginTargets + Chr(13) + Chr(10);
      SkippedPluginTargets := SkippedPluginTargets +
        'ETS2 game directory not found; ETS2 plugin installation was skipped.';
    end;

    if ATSRootPath = '' then
    begin
      if SkippedPluginTargets <> '' then
        SkippedPluginTargets := SkippedPluginTargets + Chr(13) + Chr(10);
      SkippedPluginTargets := SkippedPluginTargets +
        'ATS game directory not found; ATS plugin installation was skipped.';
    end;
    if SkippedPluginTargets <> '' then
      MsgBox('These plugin targets were skipped because a game or architecture directory was unavailable, or to protect existing files:' +
        Chr(13) + Chr(10) + Chr(13) + Chr(10) + SkippedPluginTargets, mbInformation, MB_OK);
  end;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  BrowserErrorCode: Integer;
begin
  Result := True;
  if (CurPageID = wpFinished) and TikTokFollowCheck.Checked then
    ShellExec('open', TikTokProfileUrl, '', '', SW_SHOWNORMAL, ewNoWait, BrowserErrorCode);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
  begin
    RestorePluginsUnderRoot(FindGameRoot(ETS2FolderName, 'eurotrucks2.exe'));
    RestorePluginsUnderRoot(FindGameRoot(ATSFolderName, 'amtrucks.exe'));
  end;
end;

function InitializeSetup: Boolean;
var
  GameError: String;
begin
  if not CheckGamesAreClosed(GameError) then
  begin
    MsgBox(GameError + #13#10#13#10 +
      'The installer will not terminate either game. Close the game first.',
      mbError, MB_OK);
    Result := False;
    Exit;
  end;
  ETS2RootPath := FindGameRoot(ETS2FolderName, 'eurotrucks2.exe');
  ATSRootPath := FindGameRoot(ATSFolderName, 'amtrucks.exe');
  Result := True;
end;

procedure DeinitializeSetup;
begin
  if PreviousManagedFiles <> nil then
    PreviousManagedFiles.Free;
end;
