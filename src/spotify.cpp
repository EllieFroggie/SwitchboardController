#include "Spotify.h"
#include  <iostream>
#include <fstream>
#include <algorithm>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

// super not safe but will have to do for now
std::string exec(const char* cmd) {
    std::array<char, 256> buffer{};
    std::string result;

    FILE* pipe = popen(cmd, "r");
    if (!pipe) throw std::runtime_error("popen() failed!");

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    int ret = pclose(pipe);
    if (ret != 0) {
        std::cerr << "Command exited with code " << ret << "\n";
    }
    return result;
}

//https://shinyu.org/en/cpp/strings/removing-quotes-from-a-string/
std::string remove_quotes(std::string input) {
    input.erase(std::remove(input.begin(), input.end(), '\"'), input.end());
    input.erase(std::remove(input.begin(), input.end(), '\''), input.end());
    return input;
}

int Spotify::get_sink() {
    json j;
    int index = -1;

    try {
        j = json::parse(std::string(exec("pactl -f json list sink-inputs")));
    } catch (const std::exception& e) {
        std::cerr << "JSON parse error: " << e.what() << endl;
        return -1;
    }

    for(int i = 0; i < j.size(); i++) {

        if(remove_quotes(j[i].at("properties").at("application.process.binary")) == "spotify" || remove_quotes(j[i].at("properties").at("application.process.binary")) == "spotify_player" ) {
            index = j[i].at("index");
        } else index = -1;
    }

    return index;
}

int* Spotify::get_all_sinks() {
    json j;
    static int sinks[3] = {-1, -1, -1}; // Priority from 0: Discord, Spotify, Waterfox/youtube 
    string mediaName;

    try {
        j = json::parse(std::string(exec("pactl -f json list sink-inputs")));
    } catch (const std::exception& e) {
        std::cerr << "JSON parse error: " << e.what() << endl;
        return sinks;
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

    return sinks;
}



