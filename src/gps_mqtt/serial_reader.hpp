#ifndef SIGNLANG_EYES_GPS_MQTT_SERIAL_READER_HPP
#define SIGNLANG_EYES_GPS_MQTT_SERIAL_READER_HPP

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace signlang::gps_mqtt {

  // Asynchronous line-oriented serial reader.
  // Invokes callback for each complete line received.
  class SerialReader {
  public:
    using LineCallback = std::function<void(const std::string& line)>;

    SerialReader(const std::string& device, int baud_rate, LineCallback callback);

    SerialReader(const SerialReader&) = delete;
    auto operator=(const SerialReader&) -> SerialReader& = delete;
    SerialReader(SerialReader&&) = delete;
    auto operator=(SerialReader&&) -> SerialReader& = delete;

    ~SerialReader();

    // Open the port and start the reading thread.
    void start();

    // Stop reading and close the port.
    void stop();

    [[nodiscard]] auto is_running() const -> bool;

  private:
    void run();

    std::string device_;
    int baud_rate_;
    LineCallback callback_;
    int fd_ = -1;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::thread thread_;
  };

} // namespace signlang::gps_mqtt

#endif // SIGNLANG_EYES_GPS_MQTT_SERIAL_READER_HPP
