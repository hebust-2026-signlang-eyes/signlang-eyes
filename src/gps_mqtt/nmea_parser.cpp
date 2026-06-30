#include "nmea_parser.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstring>
#include <sstream>

namespace signlang::gps_mqtt {

  namespace {

    auto split_fields(std::string_view data) -> std::vector<std::string> {
      std::vector<std::string> fields;
      std::size_t start = 0;
      while (start < data.size()) {
        const auto end = data.find(',', start);
        if (end == std::string_view::npos) {
          fields.emplace_back(data.substr(start));
          break;
        }
        fields.emplace_back(data.substr(start, end - start));
        start = end + 1;
      }
      return fields;
    }

    auto parse_float(const std::string& s) -> std::optional<float> {
      if (s.empty()) return std::nullopt;
      try {
        return std::stof(s);
      } catch (...) {
        return std::nullopt;
      }
    }

    auto parse_double(const std::string& s) -> std::optional<double> {
      if (s.empty()) return std::nullopt;
      try {
        return std::stod(s);
      } catch (...) {
        return std::nullopt;
      }
    }

    auto parse_uint8(const std::string& s) -> std::optional<std::uint8_t> {
      if (s.empty()) return std::nullopt;
      try {
        const auto value = std::stoul(s);
        if (value > 255) return std::nullopt;
        return static_cast<std::uint8_t>(value);
      } catch (...) {
        return std::nullopt;
      }
    }

    auto parse_uint16(const std::string& s) -> std::optional<std::uint16_t> {
      if (s.empty()) return std::nullopt;
      try {
        const auto value = std::stoul(s);
        if (value > 65535) return std::nullopt;
        return static_cast<std::uint16_t>(value);
      } catch (...) {
        return std::nullopt;
      }
    }

    auto parse_int8(const std::string& s) -> std::optional<std::int8_t> {
      if (s.empty()) return std::nullopt;
      try {
        const auto value = std::stoi(s);
        if (value < -128 || value > 127) return std::nullopt;
        return static_cast<std::int8_t>(value);
      } catch (...) {
        return std::nullopt;
      }
    }

  } // namespace

  auto NmeaParser::verify_checksum(std::string_view line) -> bool {
    const auto star = line.find('*');
    if (star == std::string_view::npos || star + 2 >= line.size()) return false;

    std::uint8_t csum = 0;
    for (std::size_t i = 1; i < star; ++i) {
      csum ^= static_cast<std::uint8_t>(line[i]);
    }

    std::uint8_t expected = 0;
    for (std::size_t i = star + 1; i <= star + 2; ++i) {
      const char c = line[i];
      expected = static_cast<std::uint8_t>((expected << 4) +
                                           (std::isdigit(static_cast<unsigned char>(c)) ? (c - '0')
                                                                                        : (std::toupper(c) - 'A' + 10)));
    }
    return csum == expected;
  }

  auto NmeaParser::parse_coordinate(std::string_view value, char direction) -> std::optional<double> {
    if (value.empty() || direction == '\0') return std::nullopt;

    std::size_t degrees_len = 0;
    if (direction == 'N' || direction == 'S') {
      degrees_len = 2;
    } else if (direction == 'E' || direction == 'W') {
      degrees_len = 3;
    } else {
      return std::nullopt;
    }

    if (value.size() <= degrees_len) return std::nullopt;

    try {
      const auto degrees = std::stod(std::string{value.substr(0, degrees_len)});
      const auto minutes = std::stod(std::string{value.substr(degrees_len)});
      auto decimal = degrees + minutes / 60.0;
      if (direction == 'S' || direction == 'W') {
        decimal = -decimal;
      }
      return decimal;
    } catch (...) {
      return std::nullopt;
    }
  }

  auto NmeaParser::fix_type_from_quality(const std::string& quality) -> GpsFixType {
    if (quality.empty()) return GpsFixType::Invalid;
    switch (quality[0]) {
    case '1': return GpsFixType::GpsSingle;
    case '2': return GpsFixType::Dgps;
    case '3': return GpsFixType::Pps;
    case '4': return GpsFixType::RtkFixed;
    case '5': return GpsFixType::RtkFloat;
    case '6': return GpsFixType::DeadReckoning;
    default: return GpsFixType::Invalid;
    }
  }

  auto NmeaParser::parse(std::string_view line) -> bool {
    if (line.empty() || line.front() != '$') return false;
    if (!verify_checksum(line)) return false;

    const auto star = line.find('*');
    const auto data_part = line.substr(1, star - 1);
    const auto fields = split_fields(data_part);
    if (fields.empty()) return false;

    const auto& sentence_id = fields[0];
    if (sentence_id.size() < 3) return false;

    const auto talker = std::string{sentence_id.substr(0, 2)};
    const auto msg_type = std::string{sentence_id.substr(2)};

    std::lock_guard lock{mutex_};
    if (msg_type == "GGA") {
      parse_gga(fields);
    } else if (msg_type == "RMC") {
      parse_rmc(fields);
    } else if (msg_type == "GSV") {
      parse_gsv(talker, fields);
    } else {
      return false;
    }
    return true;
  }

  void NmeaParser::parse_gga(const std::vector<std::string>& f) {
    if (f.size() < 15) return;

    state_.time = f[1];
    const auto lat = parse_coordinate(f[2], f[3].empty() ? '\0' : f[3][0]);
    const auto lon = parse_coordinate(f[4], f[5].empty() ? '\0' : f[5][0]);
    state_.fix.fix_type = fix_type_from_quality(f[6]);
    state_.fix_valid = state_.fix.fix_type != GpsFixType::Invalid;
    if (const auto sats = parse_uint8(f[7]); sats.has_value()) {
      state_.fix.sats = sats.value();
    }
    if (const auto hdop = parse_float(f[8]); hdop.has_value()) {
      state_.fix.hdop = hdop.value();
    }
    if (const auto alt = parse_double(f[9]); alt.has_value()) {
      state_.fix.alt = alt.value();
    }
    if (lat.has_value()) state_.fix.lat = lat.value();
    if (lon.has_value()) state_.fix.lon = lon.value();
  }

  void NmeaParser::parse_rmc(const std::vector<std::string>& f) {
    if (f.size() < 12) return;

    state_.time = f[1];
    const auto lat = parse_coordinate(f[3], f[4].empty() ? '\0' : f[4][0]);
    const auto lon = parse_coordinate(f[5], f[6].empty() ? '\0' : f[6][0]);
    if (lat.has_value()) state_.fix.lat = lat.value();
    if (lon.has_value()) state_.fix.lon = lon.value();

    if (const auto speed_knots = parse_float(f[7]); speed_knots.has_value()) {
      state_.motion.spd = speed_knots.value() * 1.852f;
    }
    if (const auto cog = parse_float(f[8]); cog.has_value()) {
      state_.motion.cog = static_cast<std::uint16_t>(std::round(cog.value()));
    }

    if (f[9].size() == 6) {
      const auto d = f[9];
      const auto year = std::stoul(d.substr(4, 2));
      const auto full_year = (year < 50 ? 2000 : 1900) + year;
      state_.date = std::to_string(full_year) + "-" + d.substr(2, 2) + "-" + d.substr(0, 2);
    }
  }

  void NmeaParser::parse_gsv(std::string_view talker, const std::vector<std::string>& f) {
    if (f.size() < 4) return;

    const auto total_msgs = parse_uint8(f[1]).value_or(1);
    const auto msg_num = parse_uint8(f[2]).value_or(1);

    if (msg_num == 1) {
      gsv_buffer_.clear();
    }

    for (std::size_t i = 4; i + 3 < f.size(); i += 4) {
      if (f[i].empty()) continue;
      SatelliteInfo sat;
      sat.prn = f[i];
      if (const auto elev = parse_uint8(f[i + 1]); elev.has_value()) sat.elev = elev.value();
      if (const auto azim = parse_uint16(f[i + 2]); azim.has_value()) sat.azim = azim.value();
      if (const auto snr = parse_int8(f[i + 3]); snr.has_value()) sat.snr = snr.value();
      gsv_buffer_.push_back(std::move(sat));
    }

    if (msg_num == total_msgs) {
      state_.sats_visible = gsv_buffer_;
      state_.total_sats_visible = static_cast<std::uint8_t>(gsv_buffer_.size());
    }
  }

  auto NmeaParser::get_state() const -> GpsState {
    std::lock_guard lock{mutex_};
    return state_;
  }

  auto NmeaParser::build_report(std::uint32_t ts) const -> GpsReport {
    std::lock_guard lock{mutex_};
    GpsReport report;
    report.ts = ts;
    report.fix = state_.fix;
    report.motion = state_.motion;
    return report;
  }

} // namespace signlang::gps_mqtt
