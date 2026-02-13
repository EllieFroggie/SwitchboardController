#pragma once
#include "Utils.h"
#include <string>
#include <array>

class AudioManager {

    public: 
        static void get_all_sinks(std::array<int, 4> &sinks, bool& lock);
        int get_volume();
        int sink;


    private:
        int sink_volume;

};



