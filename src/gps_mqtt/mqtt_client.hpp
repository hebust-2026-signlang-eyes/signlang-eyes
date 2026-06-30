#ifndef SIGNLANG_EYES_GPS_MQTT_MQTT_CLIENT_HPP
#define SIGNLANG_EYES_GPS_MQTT_MQTT_CLIENT_HPP

#include "gps_data.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mqtt/async_client.h>
#include <string>

namespace signlang::gps_mqtt {

  // Wraps Paho MQTT async_client for GPS telemetry, emergency alerts,
  // heartbeats, and command subscription.
  class MqttClient {
  public:
    using CommandCallback = std::function<void(const std::string& topic, const std::string& payload)>;

    struct Config {
      std::string server_uri;
      std::string client_id;
      std::string device_id;
      std::string username;
      std::string password;
      std::chrono::seconds connect_timeout{5};
      std::chrono::seconds keep_alive{60};
      bool clean_session = true;
    };

    explicit MqttClient(Config config);

    MqttClient(const MqttClient&) = delete;
    auto operator=(const MqttClient&) -> MqttClient& = delete;
    MqttClient(MqttClient&&) = delete;
    auto operator=(MqttClient&&) -> MqttClient& = delete;

    ~MqttClient();

    // Blocking connect; waits until the connection is established or times out.
    void connect();

    // Disconnect cleanly and publish offline state.
    void disconnect();

    [[nodiscard]] auto is_connected() const -> bool;

    void set_command_callback(CommandCallback callback);

    void publish_gps(const GpsReport& report);
    void publish_emergency(const EmergencyAlert& alert);
    void publish_heartbeat(const DeviceStatus& status);
    void publish_online(const OnlineState& state);

    [[nodiscard]] auto gps_topic() const -> std::string;
    [[nodiscard]] auto emergency_topic() const -> std::string;
    [[nodiscard]] auto heartbeat_topic() const -> std::string;
    [[nodiscard]] auto online_topic() const -> std::string;
    [[nodiscard]] auto command_topic() const -> std::string;

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
  };

} // namespace signlang::gps_mqtt

#endif // SIGNLANG_EYES_GPS_MQTT_MQTT_CLIENT_HPP
