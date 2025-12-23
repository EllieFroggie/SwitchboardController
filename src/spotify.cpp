#include "Spotify.h"
#include <iostream>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <stdexcept>

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
        std::cerr << "Command has exited with code " << ret << "\n";
        throw std::runtime_error("Command Failed");
    }
    return result;
}

//https://shinyu.org/en/cpp/strings/removing-quotes-from-a-string/
std::string remove_quotes(std::string input) {
    input.erase(std::remove(input.begin(), input.end(), '\"'), input.end());
    input.erase(std::remove(input.begin(), input.end(), '\''), input.end());
    return input;
}


void Spotify::get_all_sinks(std::array<int, 3>& sinks) {
    json j;
    std::string mediaName;

    try {
        j = json::parse(std::string(exec_cmd("pactl -f json list sink-inputs")));
    } catch (const std::exception& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return;
    }


    for(int i = 0; i < j.size(); i++) {
        mediaName = j[i].at("properties").at("media.name");  
        
        if (mediaName.find("YouTube") != std::string::npos) {
            sinks[2] = j[i].at("index");
        }
        
        if (mediaName.find("Spotify") != std::string::npos) {
            sinks[1] = j[i].at("index");
        }

        mediaName = j[i].at("properties").at("application.process.binary");

        if (mediaName.find("vesktop") != std::string::npos) {
            sinks[0] = j[i].at("index");
        }

    }
    return;
}



