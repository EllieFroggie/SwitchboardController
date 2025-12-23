#include "SerialPort.h"
#include "Spotify.h"
#include <cctype>
#include <cstring>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>

// g++ -std=c++17 -Iinclude src/main.cpp src/SerialPort.cpp src/spotify.cpp -o build/test_SwitchboardController
// watch -n 0.5 systemctl --user status InoSwitchboardController.service

void change_sink_volume(int readPercent, int &knobPercent, int &sinkId, std::array<int, 3>& sinks) {
  try {
    exec_cmd(std::string("pactl set-sink-input-volume " +
                         std::to_string(sinkId) + " " +
                         std::to_string(readPercent) + "%")
                 .c_str());
    knobPercent = readPercent;
    return;
  } catch (const std::runtime_error &e) {
    sinkId = -1;
  }
}

void change_speaker_volume(int readPercent, int &knobPercent, std::string device) {
  try {
    exec_cmd(std::string("pactl set-sink-volume " + device + " " + std::to_string(readPercent) + "%").c_str());
    knobPercent = readPercent;
  } catch(const std::runtime_error& e) {
    std::cout << "Failed to set speaker volume with error: " << e.what() << std::endl;
  }
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

  int knob;
  int percent;
  int switchID;
  int switchValue;

  const int KNOB_SPEAKER_ID = 14;
  const int KNOB_SPOTIFY_ID = 20; 

  bool switch_one = false;
  bool switch_two = false;
  bool switch_three = false;

  bool spotifyPickup = false;
  bool discordPickup = false;

  int spotifyPercent = 40;
  int discordPercent = 120;
  int speakerPercent = 100;

  std::array<int, 3> sink = {-1, -1, -1}; // 0 Vesktop, 1 Spotify, 2 Youtube

  int &DISCORD_SINK = sink[0];
  int &SPOTIFY_SINK = sink[1];
  int &YOUTUBE_SINK = sink[2];

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

  std::cout << "Boards Detected, Setup Complete! Waiting for data...\n";

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

      std::cout << data << std::endl;

      if (!data.empty()) {

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

          Spotify::get_all_sinks(sink);

          switch (knob) {
          case KNOB_SPOTIFY_ID:
          
            if (switch_three == true) {
              if (DISCORD_SINK != -1) {
                percent += 30; // Volume boost because everyone in discord has trash mics

                if (discordPickup) {
                  if (percent == discordPercent) {
                    std::cout << "Discord Caught!" << std::endl;
                    discordPickup = false;
                  } else
                    break;
                }

                if (percent != discordPercent) {

                  change_sink_volume(percent, discordPercent, DISCORD_SINK, sink);
                  std::cout << "Discord Change: " << percent << "%" << std::endl;
                  
                } else
                  break;

              } else {
                if (SPOTIFY_SINK != -1) {

                  if (spotifyPickup) {
                    if (percent == spotifyPercent) {
                      std::cout << "Spotify Caught!" << std::endl;
                      spotifyPickup = false;
                    } else
                      break;
                  }

                  if (percent != spotifyPercent) {
 
                    change_sink_volume(percent, spotifyPercent, SPOTIFY_SINK, sink);
                    std::cout << "Spotify Change: " << percent << "%" << std::endl;

                  } else
                    break;
                }
              }
            } else { // switch3 == false
              if (SPOTIFY_SINK != -1) {

                if (spotifyPickup) {
                  if (percent == spotifyPercent) {
                    std::cout << "Spotify Caught!" << std::endl;
                    spotifyPickup = false;
                  } else
                    break;
                }

                if (percent != spotifyPercent) {

                  change_sink_volume(percent, spotifyPercent, SPOTIFY_SINK, sink);
                  std::cout << "Spotify Change: " << percent << "%" << std::endl;

                } else
                  break;
              }
            }
            break;

          case KNOB_SPEAKER_ID:

            if (percent != speakerPercent) {
              
              change_speaker_volume(percent, speakerPercent, "alsa_output.pci-0000_00_1f.3.analog-stereo");

              std::cout << "Speaker Change: " << percent << "%" << std::endl;
            } else
              break;
            break;
          }
        
      }
    }

    // Arduino 2 (Switches)
    if (FD_ISSET(fd2, &readfds)) {
      data = serial2.readLine();
      if (!data.empty()) {
          
          std::cout << data << std::endl;
          
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
                               "alsa_output.usb-Focusrite_Scarlett_Solo_USB_"
                               "Y76QPCX21354BF-00.HiFi__Line1__sink").c_str()); // Headphones
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
              discordPickup = true;
            } else if (switchValue == 0) {
              std::cout << "Switch 3 False" << std::endl;
              switch_three = false;

              if (DISCORD_SINK != -1) {
                spotifyPickup = true;
              }
            }
            break;

          }
        
      }
    }
  }

  return 0;
}
