#define MyAppName "LanChat Server"
#define MyAppVersion "1.2.0"
#define MyAppPublisher "Denbay0"
#define MyAppExeName "lanchat_server.exe"

[Setup]
AppId={{63E7C178-8F2E-4A9B-83B5-AAAADBEEFA35}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\LanChat Server
DefaultGroupName=LanChat Server
OutputDir=dist
OutputBaseFilename=LanChatServerSetup
ArchitecturesInstallIn64BitMode=x64
Compression=lzma
SolidCompression=yes
PrivilegesRequired=admin
AllowNoIcons=yes

[Languages]
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Dirs]
Name: "{app}\data"; Flags: uninsneveruninstall

[Files]
Source: "build\Release\lanchat_server.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\LanChat Server"; \
  Filename: "{app}\{#MyAppExeName}"; \
  Parameters: "--data ""{app}\data"""; \
  WorkingDir: "{app}\data"; \
  Comment: "Запуск LanChat Server"

Name: "{commondesktop}\LanChat Server"; \
  Filename: "{app}\{#MyAppExeName}"; \
  Parameters: "--data ""{app}\data"""; \
  WorkingDir: "{app}\data"; \
  Tasks: desktopicon

Name: "{group}\Открыть папку данных"; \
  Filename: "{cmd}"; \
  Parameters: "/c start """" ""{app}\data"""

[Run]
Filename: "{app}\{#MyAppExeName}"; \
  Parameters: "--data ""{app}\data"""; \
  WorkingDir: "{app}\data"; \
  Description: "Запустить LanChat Server"; \
  Flags: nowait postinstall skipifsilent

Filename: "{cmd}"; \
  Parameters: "/c netsh advfirewall firewall add rule name=""LanChat Server TCP 5555"" dir=in action=allow protocol=TCP localport=5555"; \
  Flags: runhidden; \
  StatusMsg: "Добавляем правило брандмауэра для TCP 5555..."
