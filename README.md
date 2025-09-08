настройка окружения
1) Компилятор C++ (MSVC) + Windows SDK
   Установи Visual Studio Build Tools 2022 с нагрузкой Desktop development with C++.
2) CMake 
   winget install Kitware.CMake
3) VS Code + расширения
   Расширения: C/C++ (Microsoft) и CMake Tools.


запуск сервера

1) В командной палитре (Ctrl+Shift+P) выбери CMake: Build.
2) После сборки бинарник появится в CHAT_C-/build/Debug/lanchat_server.exe.
3) cd build/Debug
   .\lanchat_server.exe --bind 0.0.0.0 --port 5555 --data ..\..\data --secret "dev_secret"
   
4) если сборка не нужна то просто 
   cd build/Debug
   .\lanchat_server.exe


запуск клиента 

1) cd client
   python -m venv venv
2) .\venv\Scripts\Activate.ps1
3) python client.py --host 127.0.0.1 --port 5555 --user Alice
4) deactivate(выход из окружения)


