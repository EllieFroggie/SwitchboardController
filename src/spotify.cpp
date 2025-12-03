#include "Spotify.h"
#include  <iostream>
#include <fstream>
#include <algorithm>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

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
    json j = json::parse(std::string(exec("pactl -f json list sink-inputs")));
    int index = -1;

    for(int i = 0; i < size(j); i++) {

        if(remove_quotes(j[i].at("properties").at("application.process.binary")) == "spotify" || remove_quotes(j[i].at("properties").at("application.process.binary")) == "spotify_player" ) {
            index = j[i].at("index");
        } else index = -1;
    }

    return index;
}



