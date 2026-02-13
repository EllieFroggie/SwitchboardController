#pragma once
#include "Utils.h"
#include "VolumeWorker.h"
#include "SerialPort.h"
#include <sys/epoll.h>
#include <chrono>
#include <iostream>
#include <sys/epoll.h>

class Switchboard {
  public:
    enum class SubsystemState {
      NormalExit,
      RestartNeeded
    };

    static SubsystemState run_serial_subsystem();
};
