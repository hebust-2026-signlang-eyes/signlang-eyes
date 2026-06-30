#include "serial_reader.hpp"

#include "spdlog/spdlog.h"

#include <array>
#include <cstring>
#include <stdexcept>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace signlang::gps_mqtt {

  namespace {

    auto baud_rate_to_speed(int baud_rate) -> speed_t {
      switch (baud_rate) {
      case 4800: return B4800;
      case 9600: return B9600;
      case 19200: return B19200;
      case 38400: return B38400;
      case 57600: return B57600;
      case 115200: return B115200;
      case 230400: return B230400;
      case 460800: return B460800;
      default: throw std::runtime_error("Unsupported baud rate: " + std::to_string(baud_rate));
      }
    }

  } // namespace

  SerialReader::SerialReader(const std::string& device, int baud_rate, LineCallback callback) :
      device_{device}, baud_rate_{baud_rate}, callback_{std::move(callback)} {}

  SerialReader::~SerialReader() { stop(); }

  void SerialReader::start() {
    if (running_.exchange(true)) {
      return;
    }

    fd_ = ::open(device_.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd_ < 0) {
      running_ = false;
      throw std::runtime_error("Failed to open serial port " + device_ + ": " + std::strerror(errno));
    }

    // Clear O_NDELAY to make read blocking with timeout.
    const auto flags = ::fcntl(fd_, F_GETFL, 0);
    if (flags >= 0) {
      (void)::fcntl(fd_, F_SETFL, flags & ~O_NDELAY);
    }

    termios tty{};
    if (::tcgetattr(fd_, &tty) != 0) {
      ::close(fd_);
      fd_ = -1;
      running_ = false;
      throw std::runtime_error("tcgetattr failed for " + device_ + ": " + std::strerror(errno));
    }

    ::cfsetospeed(&tty, baud_rate_to_speed(baud_rate_));
    ::cfsetispeed(&tty, baud_rate_to_speed(baud_rate_));

    tty.c_cflag &= ~PARENB;  // No parity
    tty.c_cflag &= ~CSTOPB;  // One stop bit
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;      // 8 bits
    tty.c_cflag |= CREAD | CLOCAL;

    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_oflag &= ~OPOST;

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;  // 100ms read timeout

    if (::tcsetattr(fd_, TCSANOW, &tty) != 0) {
      ::close(fd_);
      fd_ = -1;
      running_ = false;
      throw std::runtime_error("tcsetattr failed for " + device_ + ": " + std::strerror(errno));
    }

    stop_requested_ = false;
    thread_ = std::thread{&SerialReader::run, this};
  }

  void SerialReader::stop() {
    stop_requested_ = true;
    if (thread_.joinable()) {
      thread_.join();
    }
    if (fd_ >= 0) {
      ::close(fd_);
      fd_ = -1;
    }
    running_ = false;
  }

  auto SerialReader::is_running() const -> bool { return running_ && !stop_requested_; }

  void SerialReader::run() {
    std::string buffer;
    buffer.reserve(256);
    std::array<char, 256> read_buffer{};

    while (!stop_requested_) {
      const auto n = ::read(fd_, read_buffer.data(), read_buffer.size());
      if (n < 0) {
        if (errno == EAGAIN || errno == EINTR) {
          continue;
        }
        spdlog::error("Serial read error on {}: {}", device_, std::strerror(errno));
        break;
      }
      if (n == 0) {
        continue;
      }

      buffer.append(read_buffer.data(), static_cast<std::size_t>(n));

      std::size_t pos = 0;
      while (true) {
        const auto cr = buffer.find('\r', pos);
        const auto lf = buffer.find('\n', pos);
        if (cr == std::string::npos && lf == std::string::npos) {
          break;
        }
        const auto end = std::min(cr, lf);
        const auto line_end = (end == std::string::npos) ? std::max(cr, lf) : end;
        if (line_end > pos) {
          callback_(buffer.substr(pos, line_end - pos));
        }
        pos = line_end + 1;
        if (cr != std::string::npos && lf != std::string::npos && lf == cr + 1) {
          pos = lf + 1;
        }
      }

      if (pos > 0) {
        buffer.erase(0, pos);
      }
    }

    running_ = false;
  }

} // namespace signlang::gps_mqtt
