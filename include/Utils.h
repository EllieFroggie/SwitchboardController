#pragma once
#include <string>
#include <iostream>
#include <chrono>
#include <atomic>
#include <csignal>


namespace Utils {

  std::string exec_cmd(const std::string& cmd, bool debug = false);
  bool is_num(const std::string& str);
  
  bool anti_thrash_check(int& counter, auto& last) {
  counter++;
    if (counter > 750) {
      std::cout << "anti_thrash_check(): Thrash event detected!" << std::endl;
      return true;
    }
    auto now = std::chrono::steady_clock::now();
    if (now - last > std::chrono::seconds(1)) {
        //std::cout << "Loop frequency: " << counter << " Hz" << std::endl;
        counter = 0;
        last = now;
        return false;
    }
    return false;
  }

  extern std::atomic<bool> keep_running;

  void signal_handler(int signal);

}
