#ifndef SIGNLANG_EYES_AI_CHAT_ICEORYX_GATEWAY_HPP
#define SIGNLANG_EYES_AI_CHAT_ICEORYX_GATEWAY_HPP

#include "ai_chat_result.hpp"
#include "signlang_det/signlang_result.hpp"
#include "state_machine/app_state.hpp"

#include "iox2/iceoryx2.hpp"

#include <cstdint>
#include <functional>
#include <string>

namespace signlang::ai_chat {

  // Monitors the global application state published by the state_machine module.
  class IpcAppStateMonitor {
  public:
    IpcAppStateMonitor(const std::string& event_service_name, const std::string& blackboard_service_name);

    IpcAppStateMonitor(const IpcAppStateMonitor&) = delete;
    auto operator=(const IpcAppStateMonitor&) -> IpcAppStateMonitor& = delete;
    IpcAppStateMonitor(IpcAppStateMonitor&&) = delete;
    auto operator=(IpcAppStateMonitor&&) -> IpcAppStateMonitor& = delete;

    // Block up to timeout_ms waiting for a state-change notification.
    // Returns true if an event arrived, false on timeout. Cached state is refreshed.
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
      throw std::runtime_error("Failed to receive signlang result sample through iceoryx2 in ai_chat");
    }

    if (!latest_sample.value().has_value()) {
      return false;
    }

    while (true) {
      auto next_sample = subscriber_.receive();
      if (!next_sample.has_value()) {
        throw std::runtime_error("Failed to receive signlang result sample through iceoryx2 in ai_chat");
      }
      if (!next_sample.value().has_value()) {
        break;
      }
      latest_sample = std::move(next_sample);
    }

    handler(latest_sample.value().value().payload());
    return true;
  }

  // Publisher for AI chat results consumed by downstream modules (e.g. tts_output).
  class IpcAiChatResultPublisher {
  public:
    IpcAiChatResultPublisher(const std::string& service_name, std::uint64_t publisher_history_size);

    IpcAiChatResultPublisher(const IpcAiChatResultPublisher&) = delete;
    auto operator=(const IpcAiChatResultPublisher&) -> IpcAiChatResultPublisher& = delete;
    IpcAiChatResultPublisher(IpcAiChatResultPublisher&&) = delete;
    auto operator=(IpcAiChatResultPublisher&&) -> IpcAiChatResultPublisher& = delete;

    void publish(const AiChatResult& result);

  private:
    static auto create_node() -> iox2::Node<iox2::ServiceType::Ipc>;
    static auto create_publisher(const iox2::Node<iox2::ServiceType::Ipc>& node, const std::string& service_name,
                                 std::uint64_t history_size)
        -> iox2::Publisher<iox2::ServiceType::Ipc, AiChatResult, void>;

    iox2::Node<iox2::ServiceType::Ipc> node_;
    iox2::Publisher<iox2::ServiceType::Ipc, AiChatResult, void> publisher_;
  };

} // namespace signlang::ai_chat

#endif // SIGNLANG_EYES_AI_CHAT_ICEORYX_GATEWAY_HPP
