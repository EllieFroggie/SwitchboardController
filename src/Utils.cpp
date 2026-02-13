#include "Utils.h"
#include <atomic>

namespace Utils {
  std::atomic<bool> keep_running(true);

  bool is_num(const std::string &str) {
    return !str.empty() &&
           std::all_of(str.begin(), str.end(),
                       [](unsigned char c) { return std::isdigit(c); });
  }

  std::string exec_cmd(const std::string &cmd, bool debug) {

    std::string result;
    std::string redirected_cmd = cmd + " 2>&1";

    FILE *pipe = popen(redirected_cmd.c_str(), "r");
    if (!pipe) {
      std::cerr << "popen() failed for: " << cmd << std::endl;
      return "";
    }

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
      result += buffer;
    }

    int status = pclose(pipe);
    int exit_code = WEXITSTATUS(status);

    if (exit_code != 0 && debug == true) {
      std::cerr << "\nCommand Failed: " << cmd << "\nExit Code: " << exit_code
                << "\nOutput: " << (result.empty() ? "None" : result)
                << std::endl;
    }

    return result;
  }


  void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
      std::cout << "\nsignal_handler(): Shutdown signal received. Cleaning up..."
                << std::endl;
      keep_running = false;

    }
  }


}