#ifndef SIGNLANG_EYES_GPS_MQTT_EMERGENCY_LISTENER_HPP
#define SIGNLANG_EYES_GPS_MQTT_EMERGENCY_LISTENER_HPP

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace signlang::gps_mqtt {

  // Internal emergency state events received from the iceoryx2 state
  // coordination service. Values must match the system-wide definition.
  enum class StateEvent : std::uint32_t {
    None = 0,
    Startup = 1,
    ModeNormal = 2,
    ModeHandsign = 3,
    EmergencyDetected = 4,
    SosPressed = 5,
    FaultDetected = 6,
    AckReceived = 7,
    Recover = 8,
    Shutdown = 9,
  };

  struct EmergencyEvent {
    StateEvent event;
    std::uint64_t count;
  };

  // Listens to the iceoryx2 `state_coordination` / `system_state` event
  // service and notifies the application when an emergency-related event
  // is received.
  class EmergencyListener {
  public:
    using EventCallback = std::function<void(const EmergencyEvent& event)>;

    EmergencyListener(const std::string& service_name, const std::string& node_name, EventCallback callback);

    EmergencyListener(const EmergencyListener&) = delete;
    auto operator=(const EmergencyListener&) -> EmergencyListener& = delete;
    EmergencyListener(EmergencyListener&&) = delete;
    auto operator=(EmergencyListener&&) -> EmergencyListener& = delete;

    ~EmergencyListener();

    void start();
    void stop();

    // Poll for newly arrived events with the given timeout (milliseconds).
    // Returns true if events were processed.
    auto poll(std::uint64_t timeout_ms) -> bool;

  private:
    void run();

    std::string service_name_;
    std::string node_name_;
    EventCallback callback_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::thread thread_;
  };

} // namespace signlang::gps_mqtt

#endif // SIGNLANG_EYES_GPS_MQTT_EMERGENCY_LISTENER_HPP
