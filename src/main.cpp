#include "SerialPort.h"
#include "Spotify.h"
#include <atomic>
#include <cctype>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>

// clear && g++ -std=c++17 -Iinclude src/main.cpp src/SerialPort.cpp src/spotify.cpp -o build/test_SwitchboardController
// watch -n 0.5 systemctl --user status InoSwitchboardController.service

const int KNOB_SPEAKER_ID = 0;
const int KNOB_SPOTIFY_ID = 1;

const int BUTTON_1_ID =  6;
const int BUTTON_2_ID = 5;

const int SINGLE_CLICK = 0;
const int DOUBLE_CLICK = 1;
const int LONG_CLICK = 2;

std::array<int, 4> sink = {-1, -1, -1, -1}; // 0 Vesktop, 1 Spotify, 2 Youtube, 3 VLC

int &DISCORD_SINK = sink[0];
int &SPOTIFY_SINK = sink[1];
int &YOUTUBE_SINK = sink[2];
int &VLC_SINK = sink[3];

enum class VolumeType {
  Sink,
  Speaker
};

struct VolumeState {
  std::atomic<int> target;
  std::atomic<int> current;
};

struct VolumeChannel {
  VolumeType type;
  VolumeState volume;

  int* sinkID = nullptr;
  std::string speaker;
};


VolumeChannel discord {
  VolumeType::Sink,
  {120, 120},
  &DISCORD_SINK
};

VolumeChannel spotify {
  VolumeType::Sink,
  {40, 40},
  &SPOTIFY_SINK
};

VolumeChannel vlc {
  VolumeType::Sink,
  {45, 45},
  &VLC_SINK
};

VolumeChannel speaker {
  VolumeType::Speaker,
  {100, 100},
  nullptr,
  "alsa_output.pci-0000_00_1f.3.analog-stereo"
};

std::mutex workerMutex;
std::condition_variable workerCv;
std::atomic<bool> workPending = false;


void change_sink_volume(int readPercent, std::atomic<int> &knobPercent, int &sinkId, std::array<int, 4> &sinks) {
  if (sinkId != -1) {
    try {
      exec_cmd(std::string("pactl set-sink-input-volume " +
                           std::to_string(sinkId) + " " +
                           std::to_string(readPercent) + "%")
                   .c_str());
      knobPercent.store(readPercent);
      return;
    } catch (const std::runtime_error &e) {
      std::cout << "Failed to set sink volume with error " << e.what() << std::endl;
      sinkId = -1;
    }
  }
}

void change_speaker_volume(int readPercent, std::atomic<int> &knobPercent, std::string device) {
  try {
    exec_cmd(std::string("pactl set-sink-volume " + device + " " + std::to_string(readPercent) + "%").c_str());
    knobPercent.store(readPercent);
  } catch(const std::runtime_error& e) {
    std::cout << "Failed to set speaker volume with error: " << e.what() << std::endl;
  }
}

void apply_volume(VolumeChannel& ch) {
  int target = ch.volume.target.load();
  int current = ch.volume.current.load();

  if (target == current) return;

  if (ch.type == VolumeType::Sink && ch.sinkID && *ch.sinkID != -1) {
    change_sink_volume(target, ch.volume.current, *ch.sinkID, sink);
  } else if (ch.type == VolumeType::Speaker) {
    change_speaker_volume(target, ch.volume.current, ch.speaker);
  }
}

void async_worker() {
  std::unique_lock<std::mutex> lock(workerMutex);

  while (true) {
    
    workerCv.wait(lock, [] { return workPending.load(); });
    workPending = false;

    lock.unlock();

    apply_volume(discord);
    apply_volume(spotify);
    apply_volume(vlc);
    apply_volume(speaker);

    lock.lock();
  }
}

inline void notify_worker() {
  {
    std::lock_guard<std::mutex> lock(workerMutex);
    workPending = true;
  }
  workerCv.notify_one();
}

bool isNum(const std::string& str) {
  return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit);
}



int main(int argc, char *argv[]) {

  SerialPort serial("/dev/ttyUSB1", 9600);
  SerialPort serial2("/dev/ttyUSB0", 9600);
  std::string data;

  size_t p;
  std::string knobString;
  std::string valueString;
  std::string switchString;
  std::string switchValueString;

  int knobID;
  int knobValue;
  int switchID;
  int switchValue;

  bool switch_one = false;
  bool switch_two = false;
  bool switch_three = false;

  bool spotifyPickup = false;
  bool discordPickup = false;
  
  static auto refreshTimer = std::chrono::steady_clock::now();

  if (!serial.openPort()) {
    std::cerr << "Failed to open port " << serial.getDevice() << std::endl;
    return 1;
  }

  if (!serial.configurePort()) {
    std::cerr << "Failed to configure port " << serial.getDevice() << std::endl;
    return 1;
  }

  if (!serial2.openPort()) {
    std::cerr << "Failed to open port " << serial2.getDevice() << std::endl;
    return 1;
  }

  if (!serial2.configurePort()) {
    std::cerr << "Failed to configure port " << serial2.getDevice() << std::endl;
    return 1;
  }

  int fd1 = serial.getFD();
  int fd2 = serial2.getFD();
  fd_set readfds;

  int maxfd;
  int activity;

  std::thread t(async_worker);

  std::cout << "Boards Detected, Worker Thread Started, Initialization Complete! Waiting for data...\n";

  while (true) {

    FD_ZERO(&readfds);
    FD_SET(fd1, &readfds);
    FD_SET(fd2, &readfds);

    maxfd = (fd1 > fd2 ? fd1 : fd2) + 1;
    activity = select(maxfd, &readfds, NULL, NULL, NULL);

    if (activity < 0) {
      std::cerr << "select error: " << strerror(errno) << "\n";
      continue;
    }

    // Arduino 1 (Knobs)
    if (FD_ISSET(fd1, &readfds)) {
      data = serial.readLine();

      //std::cout << data << std::endl;

      if (!data.empty()) {

          p = data.find(":");
          if (p == std::string::npos) continue;
            
          knobString = data.substr(0, p);
          valueString = data.substr(p + 1);
          valueString.erase(valueString.find_last_not_of(" \n\r\t") + 1);

          if (!knobString.empty() && isNum(knobString)) {
            knobID = stoi(knobString);
          } else
            knobID = -1;

          if (!valueString.empty() && isNum(valueString)) {
            knobValue = stoi(valueString);
          }

          switch (knobID) {
          case KNOB_SPOTIFY_ID:

            if (SPOTIFY_SINK != -1 && VLC_SINK == -1) {

              if (spotifyPickup) {
                if (knobValue == spotify.volume.current) {
                  std::cout << "Spotify Caught!" << std::endl;
                  spotifyPickup = false;
                } else
                  break;
              }

              if (knobValue != spotify.volume.current) {

                spotify.volume.target = knobValue;
                notify_worker();

              } else
                break;
            }

            if (SPOTIFY_SINK == -1 && VLC_SINK != -1) {
              if (knobValue != vlc.volume.current) {
                vlc.volume.target = knobValue;
                notify_worker();
              } else
                break;
            }

            break;

          case KNOB_SPEAKER_ID:
            
            if (knobValue != speaker.volume.current) {
              
              speaker.volume.target = knobValue;
              notify_worker();

            } else
              break;
            break;

            // Buttons are read from the same device as knobs so share variables
            case BUTTON_1_ID:
              if (knobValue == SINGLE_CLICK) {
                try {
                  exec_cmd(std::string("playerctl play-pause").c_str());
                } catch (const std::runtime_error &e) {
                  std::cout << "Playerctl error" << std::endl;
                }
              }
            break;

            case BUTTON_2_ID:
              if (knobValue == SINGLE_CLICK) {
                try {
                  exec_cmd(std::string("playerctl next").c_str());
                } catch (const std::runtime_error &e) {
                  std::cout << "Playerctl error" << std::endl;
                }
              } else if (knobValue == DOUBLE_CLICK) {
                try {
                  exec_cmd(std::string("playerctl previous").c_str());
                } catch (const std::runtime_error &e) {
                  std::cout << "Playerctl error" << std::endl;
                }
              }
            break;

          }
      }
    }

    // Arduino 2 (Switches)
    if (FD_ISSET(fd2, &readfds)) {
      data = serial2.readLine();
      if (!data.empty()) {
          
          //std::cout << data << std::endl;
          
          p = data.find(":");
          if (p == std::string::npos) continue;

          switchString = data.substr(0, p);
          switchValueString = data.substr(p + 1);

          if (!switchString.empty()) {
            switchID = stoi(switchString);
          } else
            switchID = -1;

          if (!switchValueString.empty()) {
            switchValue = stoi(switchValueString);
          } else
            switchValue = -1;

          switch (switchID) {

          case 1:
            if (switchValue == 1) {
              exec_cmd(std::string("pactl set-default-sink "
                               "alsa_output.pci-0000_00_1f.3.analog-stereo").c_str()); // Speakers
              switch_one = true;
            } else if (switchValue == 0) {
              exec_cmd(std::string("pactl set-default-sink "
                               "alsa_output.usb-Focusrite_Scarlett_Solo_USB_Y76QPCX21354BF-00.HiFi__Line__sink").c_str()); // Headphones
              switch_one = false;
            }
            break;

          case 2:
            if (switchValue == 1) {
              exec_cmd("amixer set Capture nocap");
              switch_two = true;
            } else if (switchValue == 0) {
              exec_cmd("amixer set Capture cap");
              switch_two = false;
            }
            break;

          case 3:
            if (switchValue == 1) {
              std::cout << "Switch 3 True" << std::endl;
              switch_three = true;
            } else if (switchValue == 0) {
              std::cout << "Switch 3 False" << std::endl;
              switch_three = false;
            }
            break;

          }
        
      }
    }

    if (std::chrono::steady_clock::now() - refreshTimer > std::chrono::seconds(2)) {
      Spotify::get_all_sinks(sink);
      refreshTimer = std::chrono::steady_clock::now();
    }
  }

  return 0;
}
