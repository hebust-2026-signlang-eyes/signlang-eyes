#ifndef SIGNLANG_EYES_GPS_MQTT_PROGRAM_OPTIONS_HPP
#define SIGNLANG_EYES_GPS_MQTT_PROGRAM_OPTIONS_HPP

#include "common/logging_cli.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace signlang::gps_mqtt {

  constexpr auto kDefaultGpsPort = "/dev/ttyS0";
  constexpr auto kDefaultGpsBaudRate = 9600;
  constexpr auto kDefaultGpsPublishHz = 1;
  constexpr auto kDefaultHeartbeatIntervalSec = 300;
  constexpr auto kDefaultMqttKeepAliveSec = 60;
  constexpr auto kDefaultMqttConnectTimeoutSec = 5;
  constexpr auto kDefaultEmergencyDebounceSec = 30;
  constexpr auto kDefaultStateCoordinationService = "state_coordination";
  constexpr auto kDefaultEmergencyNodeId = "emergency_bridge";

  struct ProgramUsage {
    std::string text;
  };

  struct ProgramOptions {
    // Serial GPS
    std::string gps_port;
    int gps_baud_rate;

    // MQTT broker
    std::string mqtt_server_uri;
    std::string mqtt_client_id;
    std::string mqtt_username;
    std::string mqtt_password;
    std::string device_id;
    std::chrono::seconds mqtt_keep_alive;
    std::chrono::seconds mqtt_connect_timeout;
    bool mqtt_clean_session;

    // Publish tuning
    int gps_publish_hz;
    int heartbeat_interval_sec;
    int emergency_debounce_sec;

    // iceoryx2 emergency
    std::string state_coordination_service;
    std::string emergency_node_id;

    // System telemetry (battery %, temperature °C, signal %)
    std::uint8_t sys_bat;
    std::int8_t sys_temp;
    std::int8_t sys_sig;

    // System info for heartbeat
    std::string firmware_version;

    signlang::logging::Options logging;
  };

  using ProgramOptionsParseResult = std::variant<ProgramOptions, ProgramUsage>;

  [[nodiscard]] auto parse_program_options(int argc, char** argv) -> ProgramOptionsParseResult;

} // namespace signlang::gps_mqtt

#endif // SIGNLANG_EYES_GPS_MQTT_PROGRAM_OPTIONS_HPP
