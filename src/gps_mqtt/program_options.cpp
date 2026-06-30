#include "program_options.hpp"

#include "common/logging_cli.hpp"
#include "cxxopts.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace signlang::gps_mqtt {

  auto parse_program_options(int argc, char** argv) -> ProgramOptionsParseResult {
    cxxopts::Options options{"signlang_eyes_gps_mqtt",
                             "GPS NMEA parser and MQTT telemetry bridge."};

    // clang-format off
    options.add_options()
        ("gps-port", "Serial port for GPS receiver (e.g. /dev/ttyS0)",
         cxxopts::value<std::string>()->default_value(kDefaultGpsPort))
        ("gps-baud-rate", "Serial baud rate",
         cxxopts::value<int>()->default_value(std::to_string(kDefaultGpsBaudRate)))
        ("mqtt-server-uri", "MQTT broker URI (e.g. mqtt://localhost:1883)",
         cxxopts::value<std::string>())
        ("mqtt-client-id", "MQTT client identifier",
         cxxopts::value<std::string>())
        ("mqtt-username", "MQTT username (optional)",
         cxxopts::value<std::string>())
        ("mqtt-password", "MQTT password (optional)",
         cxxopts::value<std::string>())
        ("device-id", "Device identifier used in MQTT topics",
         cxxopts::value<std::string>())
        ("mqtt-keep-alive", "MQTT keep-alive interval in seconds",
         cxxopts::value<int>()->default_value(std::to_string(kDefaultMqttKeepAliveSec)))
        ("mqtt-connect-timeout", "MQTT connect timeout in seconds",
         cxxopts::value<int>()->default_value(std::to_string(kDefaultMqttConnectTimeoutSec)))
        ("gps-publish-hz", "GPS telemetry publish frequency",
         cxxopts::value<int>()->default_value(std::to_string(kDefaultGpsPublishHz)))
        ("heartbeat-interval", "Heartbeat publish interval in seconds",
         cxxopts::value<int>()->default_value(std::to_string(kDefaultHeartbeatIntervalSec)))
        ("emergency-debounce", "Minimum seconds between duplicate emergency alerts",
         cxxopts::value<int>()->default_value(std::to_string(kDefaultEmergencyDebounceSec)))
        ("state-coordination-service", "iceoryx2 event service for emergency state events",
         cxxopts::value<std::string>()->default_value(kDefaultStateCoordinationService))
        ("emergency-node-id", "iceoryx2 node id for emergency listener",
         cxxopts::value<std::string>()->default_value(kDefaultEmergencyNodeId))
        ("firmware-version", "Firmware version reported in heartbeat",
         cxxopts::value<std::string>()->default_value("v1.0.0"))
        ("sys-bat", "Battery percentage for telemetry/heartbeat",
         cxxopts::value<int>()->default_value("0"))
        ("sys-temp", "Device temperature in °C for telemetry/heartbeat",
         cxxopts::value<int>()->default_value("0"))
        ("sys-sig", "Network signal strength percentage for telemetry/heartbeat",
         cxxopts::value<int>()->default_value("0"))
        ("h,help", "Print usage");
    // clang-format on
    signlang::logging::add_cli_options(options);

    const auto parsed_options = options.parse(argc, argv);
    if (parsed_options.count("help") != 0) {
      return ProgramUsage{.text = options.help()};
    }

    if (parsed_options.count("mqtt-server-uri") == 0 || parsed_options.count("mqtt-client-id") == 0 ||
        parsed_options.count("device-id") == 0) {
      throw std::runtime_error("Options --mqtt-server-uri, --mqtt-client-id, and --device-id are required.\n\n" +
                               options.help());
    }

    const auto gps_baud_rate = parsed_options["gps-baud-rate"].as<int>();
    if (gps_baud_rate <= 0) {
      throw std::runtime_error("--gps-baud-rate must be positive");
    }

    const auto gps_publish_hz = parsed_options["gps-publish-hz"].as<int>();
    if (gps_publish_hz <= 0) {
      throw std::runtime_error("--gps-publish-hz must be positive");
    }

    const auto heartbeat_interval = parsed_options["heartbeat-interval"].as<int>();
    if (heartbeat_interval <= 0) {
      throw std::runtime_error("--heartbeat-interval must be positive");
    }

    const auto emergency_debounce = parsed_options["emergency-debounce"].as<int>();
    if (emergency_debounce < 0) {
      throw std::runtime_error("--emergency-debounce must be non-negative");
    }

    const auto keep_alive = parsed_options["mqtt-keep-alive"].as<int>();
    if (keep_alive <= 0) {
      throw std::runtime_error("--mqtt-keep-alive must be positive");
    }

    const auto connect_timeout = parsed_options["mqtt-connect-timeout"].as<int>();
    if (connect_timeout <= 0) {
      throw std::runtime_error("--mqtt-connect-timeout must be positive");
    }

    return ProgramOptions{
        .gps_port = parsed_options["gps-port"].as<std::string>(),
        .gps_baud_rate = gps_baud_rate,
        .mqtt_server_uri = parsed_options["mqtt-server-uri"].as<std::string>(),
        .mqtt_client_id = parsed_options["mqtt-client-id"].as<std::string>(),
        .mqtt_username = parsed_options.count("mqtt-username") ? parsed_options["mqtt-username"].as<std::string>()
                                                               : std::string{},
        .mqtt_password = parsed_options.count("mqtt-password") ? parsed_options["mqtt-password"].as<std::string>()
                                                               : std::string{},
        .device_id = parsed_options["device-id"].as<std::string>(),
        .mqtt_keep_alive = std::chrono::seconds{keep_alive},
        .mqtt_connect_timeout = std::chrono::seconds{connect_timeout},
        .mqtt_clean_session = true,
        .gps_publish_hz = gps_publish_hz,
        .heartbeat_interval_sec = heartbeat_interval,
        .emergency_debounce_sec = emergency_debounce,
        .state_coordination_service = parsed_options["state-coordination-service"].as<std::string>(),
        .emergency_node_id = parsed_options["emergency-node-id"].as<std::string>(),
        .sys_bat = static_cast<std::uint8_t>(std::clamp(parsed_options["sys-bat"].as<int>(), 0, 100)),
        .sys_temp = static_cast<std::int8_t>(std::clamp(parsed_options["sys-temp"].as<int>(), -128, 127)),
        .sys_sig = static_cast<std::int8_t>(std::clamp(parsed_options["sys-sig"].as<int>(), 0, 100)),
        .firmware_version = parsed_options["firmware-version"].as<std::string>(),
        .logging = signlang::logging::parse_cli_options(parsed_options),
    };
  }

} // namespace signlang::gps_mqtt
