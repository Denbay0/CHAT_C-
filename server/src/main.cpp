#include "config.hpp"
#include "server.hpp"
#include <iostream>
#include <thread>
#include <atomic>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <csignal>
#endif

static std::atomic<bool> g_exit{false};

#ifndef _WIN32
static void sig_handler(int){ g_exit = true; }
#endif

int main(int argc, char** argv){
  lanchat::Config cfg;
  lanchat::parse_args(argc, argv, cfg);

#ifndef _WIN32
  std::signal(SIGINT, sig_handler);
  std::signal(SIGTERM, sig_handler);
#endif

  lanchat::Server srv(cfg);
  if (!srv.start()) return 1;

  std::cout << "Press Ctrl+C to stop (or close window on Windows).\n";
  while(!g_exit.load()){
#ifdef _WIN32
    Sleep(200);
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
#endif
  }
  srv.stop();
  return 0;
}
