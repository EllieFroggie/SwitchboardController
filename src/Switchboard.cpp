#include "Switchboard.h"
#include <sys/epoll.h>


Switchboard::SubsystemState Switchboard::run_serial_subsystem() {

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

  int loop_count = 0;
  auto last_print = std::chrono::steady_clock::now();

  int deviceID = -1;
  int deviceValue = 0;

  bool spotifyPickup = false;
  bool discordPickup = false;

  SerialPort serial("/dev/ttyUSB1", 9600);
  SerialPort serial2("/dev/ttyUSB0", 9600);

  serial_init(serial);
  serial_init(serial2);

  int epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) return Switchboard::SubsystemState::RestartNeeded;

  struct epoll_event event{};
  struct epoll_event triggered_events[5];

  event.events = EPOLLIN;
  event.data.ptr = &serial;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, serial.getFD(), &event);

  event.data.ptr = &serial2; 
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, serial2.getFD(), &event);

  std::cout << "run_serial_subsystem(): Serial Subsystem ready." << std::endl;

  while(true) {

    if (Utils::anti_thrash_check(loop_count, last_print)) {
      close(epoll_fd);
      return Switchboard::SubsystemState::RestartNeeded;
    }

    int n = epoll_wait(epoll_fd, triggered_events, 5, 1000);

    for (int i = 0; i < n; i++) {
        SerialPort* port = static_cast<SerialPort*>(triggered_events[i].data.ptr);

        std::string data = port->readLine();
        if (data.empty()) continue;

        size_t p = data.find(":");
        if (p == std::string::npos) continue;

        std::string knobString = data.substr(0, p);
        std::string valueString = data.substr(p + 1);
        valueString.erase(valueString.find_last_not_of(" \n\r\t") + 1);

        if (!knobString.empty() && Utils::is_num(knobString)) {
          deviceID = stoi(knobString);
        } else
          deviceID = -1;

        if (!valueString.empty() && Utils::is_num(valueString)) {
          deviceValue = stoi(valueString);
        }

        switch (deviceID) {
          case KNOB_SPEAKER_ID:
            if (deviceValue != VolumeWorker::speaker.volume.current) {
              VolumeWorker::speaker.volume.target = deviceValue;
              VolumeWorker::notify_worker();
            } else
              break;
            break;
          
          case KNOB_SPOTIFY_ID:
            if (VolumeWorker::SPOTIFY_SINK != -1 && VolumeWorker::VLC_SINK == -1) {

              if (spotifyPickup) {
                if (deviceValue == VolumeWorker::spotify.volume.current) {
                  std::cout << "run_serial_subsystem(): Spotify Caught!" << std::endl;
                  spotifyPickup = false;
                } else
                  break;
              }

              if (deviceValue != VolumeWorker::spotify.volume.current) {
                VolumeWorker::spotify.volume.target = deviceValue;
                VolumeWorker::notify_worker();
              } else
                break;
            }

            if (VolumeWorker::SPOTIFY_SINK == -1 && VolumeWorker::VLC_SINK != -1) {
              if (deviceValue != VolumeWorker::vlc.volume.current) {
                VolumeWorker::vlc.volume.target = deviceValue;
                VolumeWorker::notify_worker();
              } else
                break;
            }
            break;

          case KNOB_YOUTUBE_ID:
            deviceValue += 10;
            if (VolumeWorker::YOUTUBE_SINK != -1) {
              if (deviceValue != VolumeWorker::youtube.volume.current) {
                VolumeWorker::youtube.volume.target = deviceValue;
                VolumeWorker::notify_worker();
              } else
                break;
            }
            break;

          case SWITCH_ONE_ID:
            if (deviceValue == 1) {
              Utils::exec_cmd("pactl set-default-sink alsa_output.pci-0000_00_1f.3.analog-stereo"); // Speakers
            } else if (deviceValue == 0) {
              Utils::exec_cmd("pactl set-default-sink alsa_output.usb-Focusrite_Scarlett_Solo_USB_Y76QPCX21354BF-00.HiFi__Line__sink"); // Headphones
            }
            break;

          case SWITCH_TWO_ID:
            if (deviceValue == 1) {
              Utils::exec_cmd("amixer set Capture nocap");
            } else if (deviceValue == 0) {
              Utils::exec_cmd("amixer set Capture cap");
            }
            break;
          
          case SWITCH_THREE_ID:
            if (deviceValue == 1) {
              Utils::exec_cmd("wlcrosshairctl show");
            } else if (deviceValue == 0) {
              Utils::exec_cmd("wlcrosshairctl hide");
            }
            break;

          case BUTTON_1_ID:
            if (deviceValue == SINGLE_CLICK) {
              Utils::exec_cmd("playerctl play-pause");
            } else if (deviceValue == DOUBLE_CLICK) {
              std::cout << "run_serial_subsystem(): Refreshing Spotify" << std::endl;
              Utils::exec_cmd("systemctl --user restart spotifyd.service");
            } else if (deviceValue == LONG_CLICK) {
              Utils::exec_cmd("systemctl --user restart InoSwitchboardController.service");
            }
            break;
          
          case BUTTON_2_ID:
            if (deviceValue == SINGLE_CLICK) {
              Utils::exec_cmd("playerctl next");
            } else if (deviceValue == DOUBLE_CLICK) {
              Utils::exec_cmd("playerctl previous");
            }
            break;
        }
    }
  }

  close(epoll_fd);
  return Switchboard::SubsystemState::NormalExit;
}