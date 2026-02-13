cmake_minimum_required(VERSION 3.10)
project(SwitchboardController VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

set(SOURCES
    src/main.cpp
    src/VolumeWorker.cpp
    src/Switchboard.cpp
    src/AudioManager.cpp
    src/SerialPort.cpp
    src/Utils.cpp
)

find_package(Threads REQUIRED)

add_executable(test_SwitchboardController ${SOURCES})

target_link_libraries(test_SwitchboardController PRIVATE Threads::Threads)