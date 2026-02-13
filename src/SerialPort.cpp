#include "SerialPort.h"
#include <sys/ioctl.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

SerialPort::SerialPort(const std::string& device, int baudRate)
    : device(device), baudRate(baudRate), fd(-1)
{
}

SerialPort::~SerialPort() {
    closePort();
}

int SerialPort::getFD() { 
    return this->fd; 
}

std::string SerialPort::getDevice() {
  return this->device;
}

bool SerialPort::openPort() {
  fd = open(device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd < 0) {
      std::cerr << "Error opening " << device << ": " 
                << strerror(errno) << "\n";
      return false;
  }
  return true;
}

bool SerialPort::configurePort() {
  memset(&tty, 0, sizeof tty);
  if (tcgetattr(fd, &tty) != 0) {
    std::cerr << "Error getting port attributes: "
              << strerror(errno) << "\n";
    return false;
  }

  // Set baud rate
  speed_t speed;
  switch (baudRate) {
    case 9600: speed = B9600; break;
    case 19200: speed = B19200; break;
    case 38400: speed = B38400; break;
    case 57600: speed = B57600; break;
    case 115200: speed = B115200; break;
    default:
        std::cerr << "Unsupported baud rate!\n";
        return false;
  }
  cfsetospeed(&tty, speed);
  cfsetispeed(&tty, speed);

  // 8N1 settings
  tty.c_cflag &= ~PARENB;
  tty.c_cflag &= ~CSTOPB;
  tty.c_cflag &= ~CSIZE;

  tty.c_cflag |= CS8;
  tty.c_cflag &= ~CRTSCTS;
  tty.c_cflag |= CREAD | CLOCAL;

  tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  tty.c_iflag &= ~(IXON | IXOFF | IXANY);
  tty.c_oflag &= ~OPOST;

  tty.c_cc[VMIN] = 1;
  tty.c_cc[VTIME] = 0;

  if (tcsetattr(fd, TCSANOW, &tty) != 0) {
      std::cerr << "Error setting port attributes: "
                << strerror(errno) << "\n";
      return false;
  }
  return true;
}

// changed to non-blocking because select is called in main
std::string SerialPort::readLine() {
  std::string data;
  char ch;

  while (read(fd, &ch, 1) > 0) {
      if (ch == '\n') break;
     data += ch;
  }
  return data;
}

void SerialPort::closePort() {
  if (fd >= 0) {
      close(fd);
      fd = -1;
  }
}

void serial_init(SerialPort& serial) {
  if (!serial.openPort()) {
    std::cerr << "serial_init(): Failed to open port " << serial.getDevice() << std::endl;
    exit(1);
  }

  if (!serial.configurePort()) {
    std::cerr << "serial_init(): Failed to configure port " << serial.getDevice() << std::endl;
    exit(1);
  }
}
