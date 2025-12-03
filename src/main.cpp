#include "SerialPort.h"
#include "Spotify.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <cstring>

using namespace std;

/* OPTIMIZATION LIST (from chatgpt)
Avoid spawning shell commands (use libpulse).
Avoid constant string allocations.
Extract repeated parsing logic.
Add exception / error handling for malformed serial data.
Add shutdown handling & timeouts.
*/

// g++ -std=c++17 -Iinclude src/main.cpp src/SerialPort.cpp src/spotify.cpp -o build/test_SwitchboardController

int main(int argc, char *argv[]) {

  SerialPort serial("/dev/ttyUSB1", 9600);
  SerialPort serial2("/dev/ttyUSB0", 9600);
  string newData;
  string newData2;
  string serialData;
  string serialData2;

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

  string command;
  string speakers = "alsa_output.pci-0000_00_1f.3.analog-stereo ";
  string headphones = "alsa_output.usb-Focusrite_Scarlett_Solo_USB_Y76QPCX21354BF-00.HiFi__Line1__sink";

  Spotify spot;
  int sink;

  int maxfd;
  int activity;

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

    // Arduino 1
    if (FD_ISSET(fd1, &readfds)) {
      std::string data = serial.readData();
      if (!data.empty()) {
        std::cout << data;
        
        p = data.find(":");
        if (p == string::npos) continue;
        knobString = data.substr(0, p);
        valueString = data.substr(p + 1);

        if (!knobString.empty()) {
            knob = stoi(knobString);
        } else knob = -1;

        if (!valueString.empty()) {
            value = stoi(valueString);
            percent = floor((value / 1023.0f) * 100);
        } else value = -1;

        switch(knob) {
        case 20:
          sink = spot.get_sink();
          if (sink != -1) {
            command = "pactl set-sink-input-volume " + to_string(sink) + " " + to_string(percent) + "%";
            std::string result = exec(command.c_str());
          } else cout << "Spotify Not Detected!" << endl;
          break;

        case 14:
          command = "pactl set-sink-volume " + speakers + to_string(percent) + "%";
          exec(command.c_str());
          break;
        }

      }
    }

    // Arduino 2
    if (FD_ISSET(fd2, &readfds)) {
      std::string data = serial2.readData();
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
                    command = "pactl set-default-sink " + speakers;
                    exec(command.c_str());
                } else if (switchValue == 0) {
                    command = "pactl set-default-sink " + headphones;
                    exec(command.c_str());
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
                    cout << "Switch 3 Unused, exec() to bind" << endl;
                } else if (switchValue == 0) {
                    cout << "Switch 3 Unused, exec() to bind" << endl;
                }
                break;

        }

      }
    }
  }

  return 0;
}
