#ifndef SIGNLANG_EYES_GPS_MQTT_NMEA_PARSER_HPP
#define SIGNLANG_EYES_GPS_MQTT_NMEA_PARSER_HPP

#include "gps_data.hpp"

#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace signlang::gps_mqtt {

  class NmeaParser {
  public:
    NmeaParser() = default;

    // Feed one NMEA line (without trailing newline). Returns true if the line was parsed.
    auto parse(std::string_view line) -> bool;

    // Thread-safe snapshot of current state.
    [[nodiscard]] auto get_state() const -> GpsState;

    // Extract a GpsReport from current state for telemetry publishing.
    [[nodiscard]] auto build_report(std::uint32_t ts) const -> GpsReport;

  private:
    mutable std::mutex mutex_;
    GpsState state_;
    std::vector<SatelliteInfo> gsv_buffer_;

    static auto verify_checksum(std::string_view line) -> bool;
    static auto parse_coordinate(std::string_view value, char direction) -> std::optional<double>;
    static auto fix_type_from_quality(const std::string& quality) -> GpsFixType;

    void parse_gga(const std::vector<std::string>& fields);
    void parse_rmc(const std::vector<std::string>& fields);
    void parse_gsv(std::string_view talker, const std::vector<std::string>& fields);
  };

} // namespace signlang::gps_mqtt

#endif // SIGNLANG_EYES_GPS_MQTT_NMEA_PARSER_HPP
