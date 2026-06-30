#include "common/runtime.hpp"
#include "emergency_listener.hpp"
#include "gps_data.hpp"
#include "json_utils.hpp"
#include "mqtt_client.hpp"
#include "nmea_parser.hpp"
#include "program_options.hpp"
#include "serial_reader.hpp"
#include "spdlog/spdlog.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace signlang::gps_mqtt {

  namespace {

    auto current_timestamp() -> std::uint32_t {
      return static_cast<std::uint32_t>(std::time(nullptr));
    }

    auto uptime_seconds() -> std::uint32_t {
      static const auto start = std::chrono::steady_clock::now();
      return static_cast<std::uint32_t>(
          std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count());
    }

    auto build_heartbeat(const ProgramOptions& options) -> DeviceStatus {
      return DeviceStatus{
          .ts = current_timestamp(),
          .uptime = uptime_seconds(),
          .bat = options.sys_bat,
          .temp = options.sys_temp,
          .sig = options.sys_sig,
          .gnss_status = "unknown",
          .firmware = options.firmware_version,
          .mem_free = 0,
      };
    }

    auto build_emergency_alert(StateEvent event, const GpsState& state, const ProgramOptions& options)
        -> EmergencyAlert {
      EmergencyAlert alert;
      alert.ts = current_timestamp();
      alert.loc = state.fix;
      alert.motion = state.motion;

      switch (event) {
      case StateEvent::SosPressed:
        alert.lvl = EmergencyLevel::Serious;
        alert.type = "sos";
        alert.trigger = "btn_sos";
        alert.desc = "Manual SOS button pressed";
        break;
      case StateEvent::FaultDetected:
        alert.lvl = EmergencyLevel::Normal;
        alert.type = "comm_loss";
        alert.trigger = "auto";
        alert.desc = "System fault detected";
        break;
      case StateEvent::EmergencyDetected:
      default:
        alert.lvl = EmergencyLevel::Serious;
        alert.type = "crash";
        alert.trigger = "auto";
        alert.desc = "Emergency event detected";
        break;
      }
      (void)options;
      return alert;
    }

    struct LastEmergency {
      std::chrono::steady_clock::time_point time;
      StateEvent event;
    };

  } // namespace

} // namespace signlang::gps_mqtt

auto main(int argc, char** argv) -> int {
  using signlang::gps_mqtt::build_emergency_alert;
  using signlang::gps_mqtt::build_heartbeat;
  using signlang::gps_mqtt::current_timestamp;
  using signlang::gps_mqtt::EmergencyAlert;
  using signlang::gps_mqtt::EmergencyEvent;
  using signlang::gps_mqtt::EmergencyListener;
  using signlang::gps_mqtt::GpsReport;
  using signlang::gps_mqtt::MqttClient;
  using signlang::gps_mqtt::NmeaParser;
  using signlang::gps_mqtt::parse_program_options;
  using signlang::gps_mqtt::ProgramOptions;
  using signlang::gps_mqtt::SerialReader;
  using signlang::gps_mqtt::StateEvent;

  return signlang::runtime::run_module(argc, argv, parse_program_options, [&](const auto& options) {
    spdlog::info("Starting gps_mqtt");
    spdlog::info("GPS port: {} @ {}", options.gps_port, options.gps_baud_rate);
    spdlog::info("MQTT broker: {}", options.mqtt_server_uri);
    spdlog::info("Device ID: {}", options.device_id);

    NmeaParser parser;
    SerialReader serial_reader{options.gps_port, options.gps_baud_rate,
                               [&](const std::string& line) { parser.parse(line); }};
    serial_reader.start();

    MqttClient mqtt_client{MqttClient::Config{
        .server_uri = options.mqtt_server_uri,
        .client_id = options.mqtt_client_id,
        .device_id = options.device_id,
        .username = options.mqtt_username,
        .password = options.mqtt_password,
        .connect_timeout = options.mqtt_connect_timeout,
        .keep_alive = options.mqtt_keep_alive,
        .clean_session = options.mqtt_clean_session,
    }};

    mqtt_client.set_command_callback(
        [](const std::string& topic, const std::string& payload) {
          spdlog::info("Received command on {}: {}", topic, payload);
          // TODO: dispatch command to vehicle control logic
        });

    mqtt_client.connect();

    std::mutex last_emergency_mutex;
    std::optional<signlang::gps_mqtt::LastEmergency> last_emergency;

    EmergencyListener emergency_listener{
        options.state_coordination_service,
        options.emergency_node_id,
        [&](const EmergencyEvent& event) {
          spdlog::warn("Emergency event received: id={}", static_cast<std::uint32_t>(event.event));

          std::lock_guard lock{last_emergency_mutex};
          const auto now = std::chrono::steady_clock::now();
          if (last_emergency.has_value() && last_emergency->event == event.event) {
            const auto elapsed =
                std::chrono::duration_cast<std::chrono::seconds>(now - last_emergency->time).count();
            if (elapsed < options.emergency_debounce_sec) {
              spdlog::info("Emergency debounced ({}s < {}s)", elapsed, options.emergency_debounce_sec);
              return;
            }
          }

          const auto state = parser.get_state();
          const auto alert = build_emergency_alert(event.event, state, options);
          mqtt_client.publish_emergency(alert);
          last_emergency = signlang::gps_mqtt::LastEmergency{now, event.event};
        }};
    emergency_listener.start();

    const auto gps_period = std::chrono::milliseconds{1000 / options.gps_publish_hz};
    auto last_gps_publish = std::chrono::steady_clock::now();
    auto last_heartbeat = std::chrono::steady_clock::now();

    while (!signlang::runtime::shutdown_requested()) {
      std::this_thread::sleep_for(std::chrono::milliseconds{20});

      const auto now = std::chrono::steady_clock::now();

      if (now - last_gps_publish >= gps_period) {
        last_gps_publish = now;
        auto report = parser.build_report(current_timestamp());
        report.sys.bat = options.sys_bat;
        report.sys.temp = options.sys_temp;
        report.sys.sig = options.sys_sig;
        mqtt_client.publish_gps(report);
      }

      if (std::chrono::duration_cast<std::chrono::seconds>(now - last_heartbeat).count() >=
          options.heartbeat_interval_sec) {
        last_heartbeat = now;
        mqtt_client.publish_heartbeat(build_heartbeat(options));
      }
    }

    spdlog::info("gps_mqtt shutting down");
    mqtt_client.disconnect();
    emergency_listener.stop();
    serial_reader.stop();
    return 0;
  });
}
