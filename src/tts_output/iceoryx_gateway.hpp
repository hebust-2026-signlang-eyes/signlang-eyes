#ifndef SIGNLANG_EYES_TTS_OUTPUT_ICEORYX_GATEWAY_HPP
#define SIGNLANG_EYES_TTS_OUTPUT_ICEORYX_GATEWAY_HPP

#include "signlang_det/signlang_result.hpp"
#include "state_machine/app_state.hpp"

#include "iox2/iceoryx2.hpp"

#include <cstdint>
#include <string>

namespace signlang::tts_output {

  // Monitors the global application state published by the state_machine module.
  // Uses the Event service for change notifications and the Blackboard service
  // to read the current AppState value.
  class IpcAppStateMonitor {
  public:
    IpcAppStateMonitor(const std::string& event_service_name, const std::string& blackboard_service_name);

    IpcAppStateMonitor(const IpcAppStateMonitor&) = delete;
    auto operator=(const IpcAppStateMonitor&) -> IpcAppStateMonitor& = delete;
    IpcAppStateMonitor(IpcAppStateMonitor&&) = delete;
    auto operator=(IpcAppStateMonitor&&) -> IpcAppStateMonitor& = delete;

    // Block up to timeout_ms waiting for a state-change notification.
    // Returns true if an event arrived, false on timeout.
    // The cached state is refreshed in both cases.
    [[nodiscard]] auto wait_for_state_change(std::uint64_t timeout_ms) -> bool;

    [[nodiscard]] auto current_state() const -> state_machine::AppState;

  private:
    using StateEventService = iox2::PortFactoryEvent<iox2::ServiceType::Ipc>;
    using StateBlackboardService = iox2::PortFactoryBlackboard<iox2::ServiceType::Ipc, state_machine::AppStateKey>;

    static auto create_node() -> iox2::Node<iox2::ServiceType::Ipc>;
    static auto create_event_service(const iox2::Node<iox2::ServiceType::Ipc>& node, const std::string& service_name)
        -> StateEventService;
    static auto create_listener(const StateEventService& service) -> iox2::Listener<iox2::ServiceType::Ipc>;
    static auto create_blackboard_service(const iox2::Node<iox2::ServiceType::Ipc>& node,
                                          const std::string& service_name) -> StateBlackboardService;
    static auto create_reader(const StateBlackboardService& service)
        -> iox2::Reader<iox2::ServiceType::Ipc, state_machine::AppStateKey>;
    static auto create_state_entry(iox2::Reader<iox2::ServiceType::Ipc, state_machine::AppStateKey>& reader)
        -> iox2::EntryHandle<iox2::ServiceType::Ipc, state_machine::AppStateKey, state_machine::AppState>;

    void refresh_state();

    iox2::Node<iox2::ServiceType::Ipc> node_;
    StateEventService event_service_;
    iox2::Listener<iox2::ServiceType::Ipc> listener_;
    StateBlackboardService blackboard_service_;
    iox2::Reader<iox2::ServiceType::Ipc, state_machine::AppStateKey> reader_;
    iox2::EntryHandle<iox2::ServiceType::Ipc, state_machine::AppStateKey, state_machine::AppState> state_entry_;
    state_machine::AppState cached_state_;
  };

  // Subscriber for sign language recognition results published by signlang_det.
  class IpcSignlangResultSubscriber {
  public:
    IpcSignlangResultSubscriber(const std::string& service_name, std::uint64_t subscriber_buffer_size);

    IpcSignlangResultSubscriber(const IpcSignlangResultSubscriber&) = delete;
    auto operator=(const IpcSignlangResultSubscriber&) -> IpcSignlangResultSubscriber& = delete;
    IpcSignlangResultSubscriber(IpcSignlangResultSubscriber&&) = delete;
    auto operator=(IpcSignlangResultSubscriber&&) -> IpcSignlangResultSubscriber& = delete;

    // Drain the subscriber queue and invoke handler with the newest sample only.
    // Returns true if a sample was received, false if the queue was empty.
    template <typename Handler>
    auto receive_latest(Handler&& handler) -> bool;

  private:
    static auto create_node() -> iox2::Node<iox2::ServiceType::Ipc>;
    static auto create_subscriber(const iox2::Node<iox2::ServiceType::Ipc>& node, const std::string& service_name,
                                  std::uint64_t buffer_size)
        -> iox2::Subscriber<iox2::ServiceType::Ipc, signlang_det::SignlangResult, void>;

    iox2::Node<iox2::ServiceType::Ipc> node_;
    iox2::Subscriber<iox2::ServiceType::Ipc, signlang_det::SignlangResult, void> subscriber_;
  };

  template <typename Handler>
  auto IpcSignlangResultSubscriber::receive_latest(Handler&& handler) -> bool {
    auto latest_sample = subscriber_.receive();
    if (!latest_sample.has_value()) {
      throw std::runtime_error("Failed to receive signlang result sample through iceoryx2 in tts_output");
    }

    if (!latest_sample.value().has_value()) {
      return false;
    }

    while (true) {
      auto next_sample = subscriber_.receive();
      if (!next_sample.has_value()) {
        throw std::runtime_error("Failed to receive signlang result sample through iceoryx2 in tts_output");
      }
      if (!next_sample.value().has_value()) {
        break;
      }
      latest_sample = std::move(next_sample);
    }

    handler(latest_sample.value().value().payload());
    return true;
  }

} // namespace signlang::tts_output

#endif // SIGNLANG_EYES_TTS_OUTPUT_ICEORYX_GATEWAY_HPP
