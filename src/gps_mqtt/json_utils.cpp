#include "json_utils.hpp"

#include "json.hpp"

#include <iomanip>
#include <sstream>

namespace signlang::gps_mqtt {

  namespace {

    auto fix_type_to_int(GpsFixType type) -> int {
      switch (type) {
      case GpsFixType::GpsSingle: return 1;
      case GpsFixType::Dgps: return 2;
      case GpsFixType::Pps: return 3;
      case GpsFixType::RtkFixed: return 4;
      case GpsFixType::RtkFloat: return 5;
      case GpsFixType::DeadReckoning: return 6;
      default: return 0;
      }
    }

    auto float_to_string(double value, int precision) -> std::string {
      std::ostringstream oss;
      oss << std::fixed << std::setprecision(precision) << value;
      return oss.str();
    }

    using json = nlohmann::json;

  } // namespace

  auto gps_report_to_json(const GpsReport& report) -> std::string {
    json j;
    j["ts"] = report.ts;

    json fix;
    fix["lat"] = report.fix.lat;
    fix["lon"] = report.fix.lon;
    fix["alt"] = report.fix.alt;
    fix["hdop"] = report.fix.hdop;
    fix["fix_type"] = fix_type_to_int(report.fix.fix_type);
    fix["sats"] = report.fix.sats;
    j["fix"] = fix;

    json motion;
    motion["spd"] = report.motion.spd;
    motion["cog"] = report.motion.cog;
    motion["acc_x"] = report.motion.acc_x;
    motion["acc_y"] = report.motion.acc_y;
    motion["acc_z"] = report.motion.acc_z;
    j["motion"] = motion;

    json sys;
    sys["bat"] = report.sys.bat;
    sys["temp"] = report.sys.temp;
    sys["sig"] = report.sys.sig;
    j["sys"] = sys;

    return j.dump();
  }

  auto emergency_alert_to_json(const EmergencyAlert& alert) -> std::string {
    json j;
    j["ts"] = alert.ts;
    j["lvl"] = static_cast<int>(alert.lvl);
    j["type"] = alert.type;
    j["trigger"] = alert.trigger;

    json loc;
    loc["lat"] = alert.loc.lat;
    loc["lon"] = alert.loc.lon;
    loc["alt"] = alert.loc.alt;
    loc["cog"] = alert.motion.cog;
    loc["spd"] = alert.motion.spd;
    j["loc"] = loc;

    j["desc"] = alert.desc;
    j["ack"] = alert.ack;

    return j.dump();
  }

  auto heartbeat_to_json(const DeviceStatus& status) -> std::string {
    json j;
    j["ts"] = status.ts;
    j["uptime"] = status.uptime;
    j["bat"] = status.bat;
    j["temp"] = status.temp;
    j["sig"] = status.sig;
    j["gnss_status"] = status.gnss_status;
    j["firmware"] = status.firmware;
    j["mem_free"] = status.mem_free;
    return j.dump();
  }

  auto online_state_to_json(const OnlineState& state) -> std::string {
    json j;
    j["ts"] = state.ts;
    j["state"] = state.state;
    j["reason"] = state.reason;
    return j.dump();
  }

} // namespace signlang::gps_mqtt
