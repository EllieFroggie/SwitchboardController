#include "SerialPort.h"
#include "Spotify.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <atomic>
#include <cctype>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>

// #define DEBUG
// Uncomment above to enable debug output

// clear && g++ -std=c++17 -Iinclude src/main.cpp src/SerialPort.cpp src/spotify.cpp -o build/test_SwitchboardController
// watch -n 0.5 systemctl --user status InoSwitchboardController.service
// cp ./build/SwitchboardController $HOME/.local/bin/SwitchboardController

const int KNOB_SPEAKER_ID = 0;
const int KNOB_SPOTIFY_ID = 1;
const int KNOB_YOUTUBE_ID = 4;

const int BUTTON_1_ID =  6;
const int BUTTON_2_ID = 5;

const int SINGLE_CLICK = 0;
const int DOUBLE_CLICK = 1;
const int LONG_CLICK = 2;

int init = 3;

std::array<int, 4> sink = {-1, -1, -1, -1}; // 0 Vesktop, 1 Spotify, 2 Youtube, 3 VLC
bool sink_lock = false;

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

VolumeChannel youtube {
  VolumeType::Sink,
  {40, 40},
  &YOUTUBE_SINK
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

      #ifdef DEBUG
      if (knobValue != youtube.volume.current) {
              youtube.volume.target = knobValue;
              notify_worker();
            
        exec_cmd(std::string("pactl set-sink-input-volume " +
                           std::to_string(sinkId) + " " +
                           std::to_string(readPercent) + "%")
                   .c_str());
      #endif

      #ifndef DEBUG
        exec_cmd(std::string("pactl set-sink-input-volume " +
                           std::to_string(sinkId) + " " +
                           std::to_string(readPercent) + "%  2>/dev/null")
                   .c_str());
      #endif
      

      knobPercent.store(readPercent);
      return;
    } catch (const std::runtime_error &e) {
      #ifdef DEBUG
        std::cout << "Error in change_sink_volume(): " << e.what() << std::endl;
      #endif
      sinkId = -1;
    }
  }
}

void change_speaker_volume(int readPercent, std::atomic<int> &knobPercent, std::string device) {
  try {
    exec_cmd(std::string("pactl set-sink-volume " + device + " " + std::to_string(readPercent) + "%").c_str());
    knobPercent.store(readPercent);
  } catch(const std::runtime_error& e) {
    #ifdef DEBUG
      std::cout << "Error in change_speaker_volume(): " << e.what() << std::endl;
    #endif

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
  auto nextRefresh = std::chrono::steady_clock::now() +  std::chrono::seconds(5);

  while (true) {
    
    workerCv.wait_until(lock, nextRefresh, [] { return workPending.load(); });
    
    bool doWork = workPending.exchange(false);
    bool doRefresh = std::chrono::steady_clock::now() >= nextRefresh;

    lock.unlock();

    if (doWork) {
      apply_volume(discord);
      apply_volume(spotify);
      apply_volume(vlc);
      apply_volume(youtube);
      apply_volume(speaker);
    }

    if (doRefresh && !sink_lock) {
      Spotify::get_all_sinks(sink, sink_lock);
      nextRefresh = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    }

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

bool is_num(const std::string& str) {

  return !str.empty() &&
         std::all_of(str.begin(), str.end(),
                     [](unsigned char c) { return std::isdigit(c); });
}

int main(int argc, char *argv[]) {

  SerialPort serial("/dev/ttyUSB1", 9600);
  SerialPort serial2("/dev/ttyUSB0", 9600);

  size_t p;

  int knobID;
  int knobValue;
  int switchID;
  int switchValue;

  bool spotifyPickup = false;
  bool discordPickup = false;
  
  bool workDone;

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

  #ifdef DEBUG
  int loop_count = 0;
  auto last_print = std::chrono::steady_clock::now();
  #endif

  std::cout << "Boards Detected, Worker Thread Started, Initialization Complete! Waiting for data...\n";
  std::thread t(async_worker);

  Spotify::get_all_sinks(sink, sink_lock);


  while (true) {

    workDone = false;

    FD_ZERO(&readfds);
    FD_SET(fd1, &readfds);
    FD_SET(fd2, &readfds);

    int maxfd = (fd1 > fd2 ? fd1 : fd2) + 1;
    int activity = select(maxfd, &readfds, NULL, NULL, NULL);

    if (activity < 0) {
      std::cerr << "select() error: " << strerror(errno) << "\n";
      continue;
    }

    #ifdef DEBUG // Loop Frequency Counter, used to figure out what's causing cpu thrashing
    loop_count++;
    auto now = std::chrono::steady_clock::now();
    if (now - last_print > std::chrono::seconds(1)) {
        std::cout << "Loop frequency: " << loop_count << " Hz" << std::endl;
        loop_count = 0;
        last_print = now;
    }
    #endif

    // Arduino 1 (Knobs)
    if (FD_ISSET(fd1, &readfds)) {
      std::string data = serial.readLine();

      if (!data.empty()) {

          workDone = true;
          
          // Dump the first 3 reads because it was doing weird stuff
          if (init > 0) {
            init--;
            continue;
          }

          p = data.find(":");
          if (p == std::string::npos) continue;
            
          std::string knobString = data.substr(0, p);
          std::string valueString = data.substr(p + 1);
          valueString.erase(valueString.find_last_not_of(" \n\r\t") + 1);

          if (!knobString.empty() && is_num(knobString)) {
            knobID = stoi(knobString);
          } else
            knobID = -1;

          if (!valueString.empty() && is_num(valueString)) {
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
              } else break;
            }
            break;

          case KNOB_SPEAKER_ID:
            if (knobValue != speaker.volume.current) {
              speaker.volume.target = knobValue;
              notify_worker();
            } else break;
            break;

          case KNOB_YOUTUBE_ID:
            knobValue += 10;
            if (YOUTUBE_SINK != -1) {
              if (knobValue != youtube.volume.current) {
                youtube.volume.target = knobValue;
                notify_worker();
              } else break;
            }
            break;
          
          // Buttons are read from the same device as knobs so share value variables
          case BUTTON_1_ID:
            if (knobValue == SINGLE_CLICK) {
              try {
                exec_cmd(std::string("playerctl play-pause").c_str());
              } catch (const std::runtime_error &e) {
                std::cout << "Playerctl error" << std::endl;
              }
            } else if (knobValue == DOUBLE_CLICK) {
              try {
                exec_cmd(std::string("systemctl --user restart spotifyd.service").c_str());
                std::cout << "Refreshing Spotify" << std::endl;
              } catch (const std::runtime_error &e) {
                std::cout << "Refresh Spotify Error" << std::endl;
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
      std::string data = serial2.readLine();
      if (!data.empty()) {

          workDone = true;  
          p = data.find(":");
          if (p == std::string::npos) continue;

          std::string switchString = data.substr(0, p);
          std::string switchValueString = data.substr(p + 1);

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
            } else if (switchValue == 0) {
              exec_cmd(std::string("pactl set-default-sink "
                               "alsa_output.usb-Focusrite_Scarlett_Solo_USB_Y76QPCX21354BF-00.HiFi__Line__sink").c_str()); // Headphones
            }
            break;

          case 2:
            if (switchValue == 1) {
              exec_cmd("amixer set Capture nocap");
            } else if (switchValue == 0) {
              exec_cmd("amixer set Capture cap");
            }
            break;

          case 3:
            if (switchValue == 1) {
              try {
                exec_cmd("wlcrosshairctl show 2>/dev/null");
              } catch (const std::exception& e) {
                std::cout << "Failed to show wlcrosshair. Is it running?" << std::endl;
              }
            } else if (switchValue == 0) {
              try {
                exec_cmd("wlcrosshairctl hide 2>/dev/null");
              } catch (const std::exception& e) {
                std::cout << "Failed to hide wlcrosshair. Is it running?" << std::endl;
              }
            }
            break;

          }
        
      }
    }

    if (!workDone) std::this_thread::sleep_for(std::chrono::milliseconds(1));

  }
  return 0;
}
