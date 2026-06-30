#ifndef SIGNLANG_EYES_GPS_MQTT_GPS_DATA_HPP
#define SIGNLANG_EYES_GPS_MQTT_GPS_DATA_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace signlang::gps_mqtt {

  // Fix quality: 0=invalid, 1=GPS, 2=DGPS, 3=PPS, 4=RTK fixed, 5=RTK float, 6=Dead reckoning
  enum class GpsFixType : std::uint8_t {
    Invalid = 0,
    GpsSingle = 1,
    Dgps = 2,
    Pps = 3,
    RtkFixed = 4,
    RtkFloat = 5,
    DeadReckoning = 6,
  };

  // NMEA GSA fix type: 1=no fix, 2=2D, 3=3D
  enum class GsaFixType : std::uint8_t {
    NoFix = 1,
    Fix2D = 2,
    Fix3D = 3,
  };

  struct GpsFix {
    double lat = 0.0;
    double lon = 0.0;
    double alt = 0.0;
    float hdop = 0.0f;
    GpsFixType fix_type = GpsFixType::Invalid;
    std::uint8_t sats = 0;
  };

  struct GpsMotion {
    float spd = 0.0f; // km/h
    std::uint16_t cog = 0; // degrees 0-360
    float acc_x = 0.0f;
    float acc_y = 0.0f;
    float acc_z = 0.0f;
  };

  struct GpsSystem {
    std::uint8_t bat = 0; // %
    std::int8_t temp = 0; // °C
    std::int8_t sig = 0; // %
  };

  struct GpsReport {
    std::uint32_t ts = 0;
    GpsFix fix{};
    GpsMotion motion{};
    GpsSystem sys{};
  };

  struct SatelliteInfo {
    std::string prn;
    std::uint8_t elev = 0;
    std::uint16_t azim = 0;
    std::int8_t snr = 0; // dB
  };

  struct GpsState {
    bool fix_valid = false;
    GpsFix fix{};
    GpsMotion motion{};
    std::string time;
    std::string date;
    std::uint8_t total_sats_visible = 0;
    std::vector<SatelliteInfo> sats_visible;
  };

  // Emergency alert
  enum class EmergencyLevel : std::uint8_t { Info = 0, Normal = 1, Serious = 2, Fatal = 3 };

  struct EmergencyAlert {
    std::uint32_t ts = 0;
    EmergencyLevel lvl = EmergencyLevel::Info;
    std::string type; // crash/sos/geo_fence/low_bat/comm_loss/tilt
    std::string trigger; // acc_shock/btn_sos/speed_over/fence_in/fence_out/auto
    GpsFix loc{};
    GpsMotion motion{};
    std::string desc;
    bool ack = false;
  };

  // Heartbeat / status
  struct DeviceStatus {
    std::uint32_t ts = 0;
    std::uint32_t uptime = 0;
    std::uint8_t bat = 0;
    std::int8_t temp = 0;
    std::int8_t sig = 0;
    std::string gnss_status;
    std::string firmware;
    std::uint32_t mem_free = 0;
  };

  // Online/offline will message
  struct OnlineState {
    std::uint32_t ts = 0;
    std::string state; // online/offline
    std::string reason;
  };

} // namespace signlang::gps_mqtt

#endif // SIGNLANG_EYES_GPS_MQTT_GPS_DATA_HPP
