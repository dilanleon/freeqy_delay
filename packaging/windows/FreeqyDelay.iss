[Setup]
AppId={{7BB581C2-36E3-4412-B1F4-4F8BD658E801}
AppName=FREEQY DELAY
AppVersion={#PluginVersion}
AppPublisher=Station Sound
DefaultDirName={commonpf64}\Station Sound\FREEQY DELAY
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64os
ArchitecturesInstallIn64BitMode=x64os
MinVersion=10.0.22000
UsePreviousSetupType=no
OutputDir={#PackageOut}
OutputBaseFilename=FREEQY-DELAY-Windows-x64-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Types]
Name: "full"; Description: "All formats"
Name: "custom"; Description: "Choose formats"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3"; Types: full
Name: "clap"; Description: "CLAP"; Types: full

[Files]
Source: "{#PayloadDir}\FREEQY DELAY.vst3\*"; DestDir: "{commoncf64}\VST3\FREEQY DELAY.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PayloadDir}\FREEQY DELAY.clap"; DestDir: "{commoncf64}\CLAP"; Components: clap; Flags: ignoreversion

[Code]
// Require a format before continuing, including in unattended installations.
function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = wpSelectComponents) or (CurPageID = wpReady) then
  begin
    Result := WizardIsComponentSelected('vst3') or WizardIsComponentSelected('clap');
    if not Result and not WizardSilent then
      MsgBox('Select at least one plug-in format.', mbError, MB_OK);
  end;
end;
