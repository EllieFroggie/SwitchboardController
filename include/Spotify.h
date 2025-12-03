#pragma once
#include <string>

class Spotify {

    public: 
        int get_sink();
        int get_volume();
        int sink;


    private:
        int sink_volume;

};

std::string exec(const char* cmd);


