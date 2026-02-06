#include "Spotify.h"
#include <chrono>
#include <iostream>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>



using json = nlohmann::json;

// super not safe but will have to do for now
std::string exec_cmd(const char* cmd) {
    std::array<char, 256> buffer{};
    std::string result;

    FILE* pipe = popen(cmd, "r");
    if (!pipe) throw std::runtime_error("popen() failed!");

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    int ret = pclose(pipe);
    if (ret != 0) {
        std::cerr << "Error in exec_cmd: {"<< cmd << "} exited with code " << ret << "\n";
        throw std::runtime_error("Command Failed");
        return 0;
    }
    return result;
}

//https://shinyu.org/en/cpp/strings/removing-quotes-from-a-string/
std::string remove_quotes(std::string input) {
    input.erase(std::remove(input.begin(), input.end(), '\"'), input.end());
    input.erase(std::remove(input.begin(), input.end(), '\''), input.end());
    return input;
}

void Spotify::get_all_sinks(std::array<int, 4> &sinks, bool &lock) {
  lock = true;
  json j;

  try {
    j = json::parse(exec_cmd("pactl -f json list sink-inputs 2>/dev/null"));
  } catch (const std::exception &e) {
    std::cerr << "JSON parse error in get_all_sinks(): " << e.what() << '\n';
    lock = false;
    return;
  }

  int found = 0;

  for (const auto &entry : j) {
    if (!entry.contains("properties")) continue;

    const auto &props = entry["properties"];
    const int index = entry.value("index", -1);

    if (index < 0) continue;

    const std::string &bin = props.value("application.process.binary", "");
    const std::string &name = props.value("media.name", "");

    if (bin.find("vesktop") != std::string::npos) {
      sinks[0] = index;
      found++;
    } else if (name.find("Spotify") != std::string::npos) {
      sinks[1] = index;
      found++;
    } else if (bin.find("waterfox") != std::string::npos) {
      sinks[2] = index;
      found++;
    } else if (bin.find("vlc") != std::string::npos) {
      sinks[3] = index;
      found++;
    }

    if (found == 4) break;
  }
  lock = false;
}