#include "SerialPort.h"
#include "Spotify.h"
#include <cmath>
#include <csignal>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <cstring>
#include <regex>

using namespace std;

/* OPTIMIZATION LIST (from chatgpt)
Avoid spawning shell commands (use libpulse).
Avoid constant string allocations.
Extract repeated parsing logic.
Add exception / error handling for malformed serial data.
Add shutdown handling & timeouts.
*/

// g++ -std=c++17 -Iinclude src/main.cpp src/SerialPort.cpp src/spotify.cpp -o build/test_SwitchboardController
// g++ -std=c++17 -Iinclude src/test_main.cpp src/SerialPort.cpp src/spotify.cpp -o build/test_sinks

//void handleSIGINT(int signal) {
//
//}

int main(int argc, char *argv[]) {

  SerialPort serial("/dev/ttyUSB1", 9600);
  SerialPort serial2("/dev/ttyUSB0", 9600);
  std::string data;

  size_t p;
  string knobString;
  string valueString;
  string switchString;
  string switchValueString;

  int knob;
  int value;
  double percent;
  int switchID;
  int switchValue;

  bool switch3;
  bool spotifyPickup = false;
  bool discordPickup = false;

  int spotifyPercent = 40;
  int discordPercent = 120;

  Spotify spot;
  int* sink;

  std::regex numRegex(R"(^\d+$)");

  if (!serial.openPort()) {
    std::cerr << "Failed to open port.\n";
    return 1;
  }

  if (!serial.configurePort()) {
    std::cerr << "Failed to configure port.\n";
    return 1;
  }

  if (!serial2.openPort()) {
    std::cerr << "Failed to open port.\n";
    return 1;
  }

  if (!serial2.configurePort()) {
    std::cerr << "Failed to configure port.\n";
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
        std::cout << data;
        
        p = data.find(":");
        if (p == string::npos) continue;
        knobString = data.substr(0, p);
        valueString = data.substr(p + 1);
        valueString.erase(valueString.find_last_not_of(" \n\r\t") + 1);

        if (!knobString.empty()) {
            knob = stoi(knobString);
        } else knob = -1;

        if (!valueString.empty() && std::regex_match(valueString, numRegex)) {
          value = stoi(valueString);
          percent = floor((value / 1023.0f) * 100);
        } else value = -1;

        switch(knob) {
        case 20:
          sink = spot.get_all_sinks();

          if (switch3 == true) {
            if (sink[0] != -1) {
              percent += 30;

              cout << "Pd: " << discordPercent << endl;
              cout << "P: " << percent << endl;
              cout << "M: " << discordPickup << "\n" << endl;


              if (discordPickup) {
                if (percent == discordPercent) {
                  cout << "Discord Caught!" << endl;
                  discordPickup = false;
                } else {
                  break;
                  break;
                }
              }

              if (percent != discordPercent) {
                exec(std::string("pactl set-sink-input-volume " + to_string(sink[0]) + " " + to_string(percent) + "%").c_str());
                discordPercent = percent;
              }
              
              
            } else {
              if (sink[1] != -1) {

                cout << "Ps: " << spotifyPercent << endl;
                cout << "P: " << percent << endl;
                cout << "M: " << spotifyPickup << "\n" << endl;
        
                if (spotifyPickup) {
                  if (percent == spotifyPercent) {
                    cout << "Spotify Caught!" << endl;
                    spotifyPickup = false;
                  } else {
                    break;
                  }
                }

                if (percent != spotifyPercent) {
                  exec(std::string("pactl set-sink-input-volume " + to_string(sink[1]) + " " + to_string(percent) + "%").c_str());
                  spotifyPercent = percent;
                }

              }
            }
          } else { // switch3 == false
            if (sink[1] != -1) {

              cout << "Ps: " << spotifyPercent << endl;
              cout << "P: " << percent << endl;
              cout << "M: " << spotifyPickup << "\n" << endl;;

              if (spotifyPickup) {
                  if (percent == spotifyPercent) {
                    cout << "Spotify Caught!" << endl;
                    spotifyPickup = false;
                  } else {
                    break;
                  }
                }

                if (percent != spotifyPercent) {
                  exec(std::string("pactl set-sink-input-volume " + to_string(sink[1]) + " " + to_string(percent) + "%").c_str());
                  spotifyPercent = percent;
                }
              
            }
          }
          
          break;

        case 14:
          exec(std::string("pactl set-sink-volume alsa_output.pci-0000_00_1f.3.analog-stereo " + to_string(percent) + "%").c_str());
          break;
        }

      }
    }

    // Arduino 2 (Switches)
    if (FD_ISSET(fd2, &readfds)) {
      data = serial2.readData();
      if (!data.empty()) {
        std::cout << data;

        p = data.find(":");
        if (p == string::npos) continue;
        
        switchString = data.substr(0, p);
        switchValueString = data.substr(p + 1);

        if(!switchString.empty()) {
            switchID = stoi(switchString);
        } else switchID = -1;

        if (!switchValueString.empty()) {
            switchValue = stoi(switchValueString);
        } else switchValue = -1;

        switch(switchID) {

            case 1: 
                if (switchValue == 1) {
                    exec(std::string("pactl set-default-sink alsa_output.pci-0000_00_1f.3.analog-stereo").c_str()); // Speakers
                } else if (switchValue == 0) {
                    exec(std::string("pactl set-default-sink alsa_output.usb-Focusrite_Scarlett_Solo_USB_Y76QPCX21354BF-00.HiFi__Line1__sink").c_str()); // Headphones
                }
                break;
            
            case 2: 
                if (switchValue == 1) {
                    exec("amixer set Capture nocap");
                } else if (switchValue == 0) {
                    exec("amixer set Capture cap");
                }
                break;

            case 3: 
                if (switchValue == 1) {
                    cout << "Switch 3 True" << endl;
                    switch3 = true;
                    discordPickup = true;
                } else if (switchValue == 0) {
                    cout << "Switch 3 False" << endl;
                    switch3 = false;
                    
                    if (sink[0] != -1) {
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
