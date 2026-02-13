#include "AudioManager.h"
#include <chrono>
#include <iostream>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>




using json = nlohmann::json;

void AudioManager::get_all_sinks(std::array<int, 4> &sinks, bool &lock) {
  lock = true;
  json j;

  try {
     j = json::parse(Utils::exec_cmd("pactl -f json list sink-inputs 2>/dev/null"));
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