#pragma once
#include <string>
#include <array>

class Spotify {

    public: 
        std::array<int, 3> get_all_sinks();
        int get_volume();
        int sink;


    private:
        int sink_volume;

};

std::string exec_cmd(const char* cmd);


