#include "SerialPort.h"
#include "AudioManager.h"
#include "VolumeWorker.h"
#include "Switchboard.h"
#include <iostream>
#include <thread>

// valgrind --leak-check=full --show-leak-kinds=all -s ./build/test_SwitchboardController

int main() {

  std::signal(SIGINT, Utils::signal_handler);
  std::signal(SIGKILL, Utils::signal_handler);

  std::thread worker((VolumeWorker::async_worker));
  AudioManager::get_all_sinks(VolumeWorker::sink, VolumeWorker::sink_lock);

  std::cout << "main(): Worker Thread Started.\n";

  while (Utils::keep_running) {
    auto status = Switchboard::run_serial_subsystem()

    if (status == Switchboard::SubsystemState::NormalExit) {
      std::cout << "main(): Subsystem requested normal shutdown." << std::endl;
      break;
    }
    
    if (status == Switchboard::SubsystemState::RestartNeeded) {
      std::cerr << "main(): Restarting serial subsystem..." << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  }

  std::cout << "main(): Joining worker thread..." << std::endl;
     
  if (worker.joinable()) {
    worker.join();
  }

  std::cout << "main(): Shutdown complete. Goodbye." << std::endl;

  return 0;
}
