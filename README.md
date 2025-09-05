запуск сервера

1) В командной палитре (Ctrl+Shift+P) выбери CMake: Build.
2) После сборки бинарник появится в CHAT_C-/build/Debug/lanchat_server.exe.
3) cd build/Debug
   .\lanchat_server.exe --bind 0.0.0.0 --port 5555 --data ..\..\data --secret "dev_secret"
