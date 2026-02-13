#include "SerialPort.h"
#include "AudioManager.h"
#include "VolumeWorker.h"
#include "Switchboard.h"
#include <iostream>
#include <thread>

// clear && g++ -std=c++20 -Iinclude src/main.cpp src/SerialPort.cpp src/AudioManager.cpp src/VolumeWorker.cpp src/Switchboard.cpp src/Utils.cpp -o build/test_SwitchboardController
// valgrind --leak-check=full --show-leak-kinds=all -s ./build/test_SwitchboardController
// cp ./build/SwitchboardController $HOME/.local/bin/SwitchboardController

int main() {

  std::thread worker((VolumeWorker::async_worker));
  
  AudioManager::get_all_sinks(VolumeWorker::sink, VolumeWorker::sink_lock);
  std::cout << "main(): Worker Thread Started.\n";

  while (true) {
        auto status = Switchboard::run_serial_subsystem();

        if (status == Switchboard::SubsystemState::NormalExit) {
            std::cout << "main(): Subsystem requested normal shutdown." << std::endl;
            break;
        }
        
        if (status == Switchboard::SubsystemState::RestartNeeded) {
            std::cerr << "main(): Restarting serial subsystem..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    return 0;
}
