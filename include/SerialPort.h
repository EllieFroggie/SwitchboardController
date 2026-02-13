#pragma once

#include <string>
#include <termios.h>

class SerialPort {
public:
    SerialPort(const std::string& device, int baudRate);
    ~SerialPort();

    bool openPort();
    bool configurePort();
    int getFD();
    std::string getDevice();
    std::string readLine();
    bool serialDataAvailable();
    void closePort();
    
private:
    int fd;                    // file descriptor for the port
    std::string device;        // device name (/dev/ttyACM0)
    int baudRate;              // baud rate (9600, 115200, ...)
    termios tty;               // termios configuration structure
};

void serial_init(SerialPort& serial);
