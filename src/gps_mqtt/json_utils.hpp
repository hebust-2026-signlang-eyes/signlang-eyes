#ifndef SIGNLANG_EYES_GPS_MQTT_JSON_UTILS_HPP
#define SIGNLANG_EYES_GPS_MQTT_JSON_UTILS_HPP

#include "gps_data.hpp"

#include <string>

namespace signlang::gps_mqtt {

  [[nodiscard]] auto gps_report_to_json(const GpsReport& report) -> std::string;
  [[nodiscard]] auto emergency_alert_to_json(const EmergencyAlert& alert) -> std::string;
  [[nodiscard]] auto heartbeat_to_json(const DeviceStatus& status) -> std::string;
  [[nodiscard]] auto online_state_to_json(const OnlineState& state) -> std::string;

} // namespace signlang::gps_mqtt

#endif // SIGNLANG_EYES_GPS_MQTT_JSON_UTILS_HPP
