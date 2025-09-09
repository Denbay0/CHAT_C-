; === installer.iss — установщик для LanChat Server ===
; Сохрани этот файл в корне репозитория и открывай через Inno Setup Compiler

#define MyAppName "LanChat Server"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Denbay0"
#define MyAppExeName "lanchat_server.exe"

[Setup]
; Уникальный ID приложения. Генерируется один раз и не меняется между версиями!
AppId={{63E7C178-8F2E-4A9B-83B5-AAAADBEEFA35}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}

; Куда устанавливать (по умолчанию Program Files)
DefaultDirName={autopf}\LanChat Server
; Группа в меню "Пуск"
DefaultGroupName=LanChat Server

; Выходной каталог и имя setup-файла
OutputDir=dist
OutputBaseFilename=LanChatServerSetup

; Поддержка x64
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
; Создаем папку для данных в ProgramData (останется при удалении)
Name: "{commonappdata}\LanChatServer"; Flags: uninsneveruninstall

[Files]
; Копируем бинарник сервера из сборки Release
Source: "build\Release\lanchat_server.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
; Ярлык в меню "Пуск"
Name: "{group}\LanChat Server"; \
  Filename: "{app}\{#MyAppExeName}"; \
  Parameters: "--data ""{commonappdata}\LanChatServer"""; \
  WorkingDir: "{commonappdata}\LanChatServer"; \
  Comment: "Запуск LanChat Server"

; Ярлык на рабочем столе (по желанию)
Name: "{commondesktop}\LanChat Server"; \
  Filename: "{app}\{#MyAppExeName}"; \
  Parameters: "--data ""{commonappdata}\LanChatServer"""; \
  WorkingDir: "{commonappdata}\LanChatServer"; \
  Tasks: desktopicon

; Ярлык "Открыть папку данных"
Name: "{group}\Открыть папку данных"; \
  Filename: "{cmd}"; \
  Parameters: "/c start """" ""{commonappdata}\LanChatServer"""

[Run]
; После установки предложить запустить сервер
Filename: "{app}\{#MyAppExeName}"; \
  Parameters: "--data ""{commonappdata}\LanChatServer"""; \
  WorkingDir: "{commonappdata}\LanChatServer"; \
  Description: "Запустить LanChat Server"; \
  Flags: nowait postinstall skipifsilent

; Добавить правило брандмауэра Windows для порта 5555
Filename: "{cmd}"; \
  Parameters: "/c netsh advfirewall firewall add rule name=""LanChat Server TCP 5555"" dir=in action=allow protocol=TCP localport=5555"; \
  Flags: runhidden; \
  StatusMsg: "Добавляем правило брандмауэра для TCP 5555..."
