#include "SerialPort.h"
#include "Spotify.h"
#include <cctype>
#include <cstring>
#include <iostream>
#include <regex>
#include <string>

// g++ -std=c++17 -Iinclude src/main.cpp src/SerialPort.cpp src/spotify.cpp -o build/test_SwitchboardController
// watch -n 0.5 systemctl --user status InoSwitchboardController.service

bool isNum(const std::string& str) {
  return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit);
}


int main(int argc, char *argv[]) {

  SerialPort serial("/dev/ttyUSB1", 9600);
  SerialPort serial2("/dev/ttyUSB0", 9600);
  std::string data;
  std::string noRepeat = "";

  size_t p;
  std::string knobString;
  std::string valueString;
  std::string switchString;
  std::string switchValueString;

  const int KNOB_SPEAKER_ID = 14;
  const int KNOB_SPOTIFY_ID = 20; 

  int knob;
  double percent;
  int switchID;
  int switchValue;

  bool switch3 = false;
  bool spotifyPickup = false;
  bool discordPickup = false;

  int spotifyPercent = 40;
  int discordPercent = 120;

  int repeat = 0;

  Spotify spot;
  std::array<int, 3> sink = {-1, -1, -1};

  std::regex numRegex(R"(^\d+$)");

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

  int maxfd;
  int activity;

  fd_set readfds;

  std::cout << "Connected! Waiting for data...\n";

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
      data = serial.readData();
      if (!data.empty()) {
        if (data != noRepeat) {
          
          std::cout << data;
          p = data.find(":");
          if (p == std::string::npos) continue;
            
          knobString = data.substr(0, p);
          valueString = data.substr(p + 1);
          valueString.erase(valueString.find_last_not_of(" \n\r\t") + 1);

          if (!knobString.empty() && isNum(knobString)) {
            knob = stoi(knobString);
          } else
            knob = -1;

          if (!valueString.empty() && isNum(valueString)) {
            percent = stoi(valueString);
          }

          switch (knob) {
          case KNOB_SPOTIFY_ID:
            sink = spot.get_all_sinks();

            if (switch3 == true) {
              if (sink[0] != -1) {
                percent += 30;

                if (discordPickup) {
                  if (percent == discordPercent) {
                    std::cout << "Discord Caught!" << std::endl;
                    discordPickup = false;
                  } else
                    break;
                }

                if (percent != discordPercent) {
                  exec_cmd(std::string("pactl set-sink-input-volume " +
                                   std::to_string(sink[0]) + " " +
                                   std::to_string(percent) + "%")
                           .c_str());
                  discordPercent = percent;
                  std::cout << "Discord Change: " << percent << "%" << std::endl;
                } else
                  break;

              } else {
                if (sink[1] != -1) {

                  if (spotifyPickup) {
                    if (percent == spotifyPercent) {
                      std::cout << "Spotify Caught!" << std::endl;
                      spotifyPickup = false;
                    } else
                      break;
                  }

                  if (percent != spotifyPercent) {
                    try {
                      exec_cmd(std::string("pactl set-sink-input-volume " +
                                       std::to_string(sink[1]) + " " +
                                       std::to_string(percent) + "%")
                               .c_str());
                    } catch (int e) {
                      sink[1] = -1;
                      break;
                    }
                    spotifyPercent = percent;
                    std::cout << "Spotify Change: " << percent << "%" << std::endl;
                  } else
                    break;
                }
              }
            } else { // switch3 == false
              if (sink[1] != -1) {

                if (spotifyPickup) {
                  if (percent == spotifyPercent) {
                    std::cout << "Spotify Caught!" << std::endl;
                    spotifyPickup = false;
                  } else
                    break;
                }

                if (percent != spotifyPercent) {

                  try {
                    exec_cmd(std::string("pactl set-sink-input-volume " +
                                     std::to_string(sink[1]) + " " +
                                     std::to_string(percent) + "%")
                             .c_str());
                  } catch (int e) {
                    sink[1] = -1;
                    break;
                  }

                  spotifyPercent = percent;
                  std::cout << "Spotify Change: " << percent << "%" << std::endl;
                } else
                  break;
              }
            }
            break;

          case KNOB_SPEAKER_ID:

            if (percent != repeat) {
              exec_cmd(std::string("pactl set-sink-volume "
                               "alsa_output.pci-0000_00_1f.3.analog-stereo " +
                               std::to_string(percent) + "%")
                       .c_str());
              repeat = percent;
              std::cout << "Speaker Change: " << percent << "%" << std::endl;
            } else
              break;
            break;
          }
          noRepeat = data;
        }
      }
    }

    // Arduino 2 (Switches)
    if (FD_ISSET(fd2, &readfds)) {
      data = serial2.readData();
      if (!data.empty()) {
        if (data != noRepeat) {
          
          std::cout << data;
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
                               "alsa_output.pci-0000_00_1f.3.analog-stereo")
                       .c_str()); // Speakers
            } else if (switchValue == 0) {
              exec_cmd(std::string("pactl set-default-sink "
                               "alsa_output.usb-Focusrite_Scarlett_Solo_USB_"
                               "Y76QPCX21354BF-00.HiFi__Line1__sink")
                       .c_str()); // Headphones
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
              std::cout << "Switch 3 True" << std::endl;
              std::cout << sink[0] << std::endl;
              switch3 = true;
              discordPickup = true;
            } else if (switchValue == 0) {
              std::cout << "Switch 3 False" << std::endl;
              switch3 = false;

              if (sink[0] != -1) {
                spotifyPickup = true;
              }
            }
            break;
          }
          noRepeat = data;
        }
      }
    }
  }

  return 0;
}
