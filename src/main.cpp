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
#include <sys/epoll.h>

// #define DEBUG
// Uncomment above to enable debug output

// clear && g++ -std=c++20 -Iinclude src/main.cpp src/SerialPort.cpp src/spotify.cpp -o build/test_SwitchboardController
// valgrind --leak-check=full --show-leak-kinds=all -s ./build/test_SwitchboardController
// cp ./build/SwitchboardController $HOME/.local/bin/SwitchboardController

const int KNOB_SPEAKER_ID = 0;
const int KNOB_SPOTIFY_ID = 1;
const int KNOB_YOUTUBE_ID = 4;

const int SWITCH_ONE_ID = 7;
const int SWITCH_TWO_ID = 8;
const int SWITCH_THREE_ID = 9;

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

void apply_volume(VolumeChannel& channel) {
  int target = channel.volume.target.load();
  int current = channel.volume.current.load();

  if (target == current) return;

  if (channel.type == VolumeType::Sink && channel.sinkID && *channel.sinkID != -1) {
    change_sink_volume(target, channel.volume.current, *channel.sinkID, sink);
  } else if (channel.type == VolumeType::Speaker) {
    change_speaker_volume(target, channel.volume.current, channel.speaker);
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
      nextRefresh = std::chrono::steady_clock::now() + std::chrono::seconds(2);
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

void serial_init(SerialPort& serial) {
  if (!serial.openPort()) {
    std::cerr << "Failed to open port " << serial.getDevice() << std::endl;
    exit(1);
  }

  if (!serial.configurePort()) {
    std::cerr << "Failed to configure port " << serial.getDevice() << std::endl;
    exit(1);
  }
}

void anti_thrash_check(int& counter, auto& last) {
  counter++;
    if (counter > 750) {
      try {
        exec_cmd(std::string("systemctl --user restart InoSwitchboardController.service").c_str());
      } catch (const std::runtime_error &e) {
        std::cout << "Failed to restart controller service" << std::endl;
      }
    }
    auto now = std::chrono::steady_clock::now();
    if (now - last > std::chrono::seconds(1)) {
        //std::cout << "Loop frequency: " << counter << " Hz" << std::endl;
        counter = 0;
        last = now;
    }
}


int main(int argc, char *argv[]) {

  int loop_count = 0;
  auto last_print = std::chrono::steady_clock::now();

  SerialPort serial("/dev/ttyUSB1", 9600);
  SerialPort serial2("/dev/ttyUSB0", 9600);

  int deviceID;
  int deviceValue;

  bool spotifyPickup = false;
  bool discordPickup = false;
  bool workDone = false;

  serial_init(serial);
  serial_init(serial2);

  fd_set readfds;
  int fd1 = serial.getFD();
  int fd2 = serial2.getFD();

  int epoll_fd = epoll_create1(0);

  struct epoll_event event{};
  struct epoll_event triggered_events[5];

  event.events = EPOLLIN;
  event.data.ptr = &serial;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, serial.getFD(), &event);

  event.data.ptr = &serial2; 
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, serial2.getFD(), &event);


  std::thread t(async_worker);
  Spotify::get_all_sinks(sink, sink_lock);
  std::cout << "Boards Detected, Worker Thread Started, Initialization Complete! Waiting for data...\n";

  while (true) {
    
    workDone = false;
    anti_thrash_check(loop_count, last_print);

    int n = epoll_wait(epoll_fd, triggered_events, 5, -1);
    for (int i = 0; i < n; i++) {

        SerialPort* port = static_cast<SerialPort*>(triggered_events[i].data.ptr);
        
        std::string data = port->readLine();
        if (data.empty()) continue;
        workDone = true;

        size_t p = data.find(":");
        if (p == std::string::npos) continue;

        std::string knobString = data.substr(0, p);
        std::string valueString = data.substr(p + 1);
        valueString.erase(valueString.find_last_not_of(" \n\r\t") + 1);

        if (!knobString.empty() && is_num(knobString)) {
          deviceID = stoi(knobString);
        } else
          deviceID = -1;

        if (!valueString.empty() && is_num(valueString)) {
          deviceValue = stoi(valueString);
        }

        switch (deviceID) {
          case KNOB_SPEAKER_ID:
            if (deviceValue != speaker.volume.current) {
              speaker.volume.target = deviceValue;
              notify_worker();
            } else
              break;
            break;
          
          case KNOB_SPOTIFY_ID:
            if (SPOTIFY_SINK != -1 && VLC_SINK == -1) {

              if (spotifyPickup) {
                if (deviceValue == spotify.volume.current) {
                  std::cout << "Spotify Caught!" << std::endl;
                  spotifyPickup = false;
                } else
                  break;
              }

              if (deviceValue != spotify.volume.current) {

                spotify.volume.target = deviceValue;
                notify_worker();

              } else
                break;
            }

            if (SPOTIFY_SINK == -1 && VLC_SINK != -1) {
              if (deviceValue != vlc.volume.current) {
                vlc.volume.target = deviceValue;
                notify_worker();
              } else
                break;
            }
            break;

          case KNOB_YOUTUBE_ID:
            deviceValue += 10;
            if (YOUTUBE_SINK != -1) {
              if (deviceValue != youtube.volume.current) {
                youtube.volume.target = deviceValue;
                notify_worker();
              } else
                break;
            }
            break;

          case SWITCH_ONE_ID:
            if (deviceValue == 1) {
              exec_cmd(std::string("pactl set-default-sink "
                                   "alsa_output.pci-0000_00_1f.3.analog-stereo")
                           .c_str()); // Speakers
            } else if (deviceValue == 0) {
              exec_cmd(
                  std::string("pactl set-default-sink "
                              "alsa_output.usb-Focusrite_Scarlett_Solo_USB_"
                              "Y76QPCX21354BF-00.HiFi__Line__sink")
                      .c_str()); // Headphones
            }
            break;

          case SWITCH_TWO_ID:
            if (deviceValue == 1) {
              exec_cmd("amixer set Capture nocap");
            } else if (deviceValue == 0) {
              exec_cmd("amixer set Capture cap");
            }
            break;
          
          case SWITCH_THREE_ID:
            if (deviceValue == 1) {
              try {
                exec_cmd("wlcrosshairctl show 2>/dev/null");
              } catch (const std::exception &e) {
                std::cout << "Failed to show wlcrosshair. Is it running?"
                          << std::endl;
              }
            } else if (deviceValue == 0) {
              try {
                exec_cmd("wlcrosshairctl hide 2>/dev/null");
              } catch (const std::exception &e) {
                std::cout << "Failed to hide wlcrosshair. Is it running?"
                          << std::endl;
              }
            }
            break;

          case BUTTON_1_ID:
            if (deviceValue == SINGLE_CLICK) {
              try {
                exec_cmd(std::string("playerctl play-pause").c_str());
              } catch (const std::runtime_error &e) {
                std::cout << "Playerctl error" << std::endl;
              }
            } else if (deviceValue == DOUBLE_CLICK) {
              try {
                exec_cmd(
                    std::string("systemctl --user restart spotifyd.service")
                        .c_str());
                std::cout << "Refreshing Spotify" << std::endl;
              } catch (const std::runtime_error &e) {
                std::cout << "Refresh Spotify Error" << std::endl;
              }
            } else if (deviceValue == LONG_CLICK) {
              try {
                exec_cmd(std::string("systemctl --user restart "
                                     "InoSwitchboardController.service")
                             .c_str());
              } catch (const std::runtime_error &e) {
                std::cout << "Failed to restart controller service"
                          << std::endl;
              }
            }
            break;
          
          case BUTTON_2_ID:
            if (deviceValue == SINGLE_CLICK) {
              try {
                exec_cmd(std::string("playerctl next").c_str());
              } catch (const std::runtime_error &e) {
                std::cout << "Playerctl error" << std::endl;
              }
            } else if (deviceValue == DOUBLE_CLICK) {
              try {
                exec_cmd(std::string("playerctl previous").c_str());
              } catch (const std::runtime_error &e) {
                std::cout << "Playerctl error" << std::endl;
              }
            }
            break;
        }
    }

    if (!workDone) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  
  return 0;
}
