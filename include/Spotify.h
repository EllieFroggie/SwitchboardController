#pragma once
#include <string>
#include <array>

class Spotify {

    public: 
        static void get_all_sinks(std::array<int, 4>& sinks, bool& lock);
        int get_volume();
        int sink;


    private:
        int sink_volume;

};

std::string exec_cmd(const char* cmd);


