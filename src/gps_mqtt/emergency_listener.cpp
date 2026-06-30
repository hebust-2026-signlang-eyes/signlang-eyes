#include "emergency_listener.hpp"

#include "common/ipc_utils.hpp"
#include "iox2/bb/duration.hpp"
#include "iox2/bb/static_function.hpp"
#include "iox2/iceoryx2.hpp"
#include "spdlog/spdlog.h"

#include <stdexcept>
#include <utility>

namespace signlang::gps_mqtt {

  namespace {

    auto to_state_event(std::uint64_t value) -> StateEvent {
      if (value <= static_cast<std::uint64_t>(StateEvent::Shutdown)) {
        return static_cast<StateEvent>(value);
      }
      return StateEvent::None;
    }

    auto is_emergency_event(StateEvent event) -> bool {
      return event == StateEvent::EmergencyDetected || event == StateEvent::SosPressed ||
          event == StateEvent::FaultDetected;
    }

  } // namespace

  EmergencyListener::EmergencyListener(const std::string& service_name, const std::string& node_name,
                                       EventCallback callback) :
      service_name_{service_name}, node_name_{node_name}, callback_{std::move(callback)} {}

  EmergencyListener::~EmergencyListener() { stop(); }

  void EmergencyListener::start() {
    if (running_.exchange(true)) {
      return;
    }
    stop_requested_ = false;
    thread_ = std::thread{&EmergencyListener::run, this};
  }

  void EmergencyListener::stop() {
    stop_requested_ = true;
    if (thread_.joinable()) {
      thread_.join();
    }
    running_ = false;
  }

  auto EmergencyListener::poll(std::uint64_t timeout_ms) -> bool {
    if (!running_)
      return false;
    // Events are delivered via callback in the background thread.
    // This function is intentionally simple and can be used to throttle.
    (void)timeout_ms;
    return true;
  }

  void EmergencyListener::run() {
    iox2::set_log_level_from_env_or(iox2::LogLevel::Warn);

    auto node_name = iox2::NodeName::create(node_name_.c_str());
    if (!node_name.has_value()) {
      throw std::runtime_error("Failed to create iceoryx2 node name: " + node_name_);
    }

    auto node = iox2::NodeBuilder{}
                    .name(std::move(node_name.value()))
                    .signal_handling_mode(iox2::SignalHandlingMode::Disabled)
                    .create<iox2::ServiceType::Ipc>();
    if (!node.has_value()) {
      throw std::runtime_error("Failed to create iceoryx2 node for gps_mqtt emergency listener");
    }

    auto service = node.value()
                       .service_builder(signlang::common::ipc::service_name_from_string(service_name_))
                       .event()
                       .open_or_create();
    if (!service.has_value()) {
      throw std::runtime_error("Failed to open or create iceoryx2 emergency event service: " + service_name_);
    }

    auto listener = service.value().listener_builder().create();
    if (!listener.has_value()) {
      throw std::runtime_error("Failed to create iceoryx2 emergency event listener");
    }

    spdlog::info("Emergency listener started on service '{}' with node_id '{}'", service_name_, node_name_);

    while (!stop_requested_) {
      auto event_received = false;
      iox2::bb::StaticFunction<void(iox2::EventActivation)> callback{[&](iox2::EventActivation activation) {
        event_received = true;
        const auto event = to_state_event(activation.id().as_value());
        if (is_emergency_event(event) && callback_) {
          callback_(EmergencyEvent{event, activation.count()});
        }
      }};

      const auto result = listener.value().timed_wait(callback, iox2::bb::Duration::from_millis(500));
      if (!result.has_value()) {
        spdlog::warn("Emergency listener wait failed");
      }
    }

    running_ = false;
  }

} // namespace signlang::gps_mqtt
