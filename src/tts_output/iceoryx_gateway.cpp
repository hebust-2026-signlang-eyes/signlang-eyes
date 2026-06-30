#include "iceoryx_gateway.hpp"

#include "common/ipc_utils.hpp"
#include "iox2/bb/duration.hpp"
#include "iox2/bb/static_function.hpp"
#include "spdlog/spdlog.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace signlang::tts_output {

  namespace {

    auto app_state_key() -> state_machine::AppStateKey { return state_machine::default_app_state_key(); }

  } // namespace

  IpcAppStateMonitor::IpcAppStateMonitor(const std::string& event_service_name,
                                         const std::string& blackboard_service_name) :
      node_{create_node()}, event_service_{create_event_service(node_, event_service_name)},
      listener_{create_listener(event_service_)},
      blackboard_service_{create_blackboard_service(node_, blackboard_service_name)},
      reader_{create_reader(blackboard_service_)}, state_entry_{create_state_entry(reader_)},
      cached_state_{state_machine::AppState::Normal} {
    refresh_state();
  }

  auto IpcAppStateMonitor::current_state() const -> state_machine::AppState { return cached_state_; }

  auto IpcAppStateMonitor::wait_for_state_change(std::uint64_t timeout_ms) -> bool {
    auto event_received = false;
    iox2::bb::StaticFunction<void(iox2::EventActivation)> callback{
        [&](iox2::EventActivation /*activation*/) { event_received = true; }};

    const auto result = listener_.timed_wait(callback, iox2::bb::Duration::from_millis(timeout_ms));
    if (!result.has_value()) {
      // On error still refresh the blackboard so we don't stay stale.
      refresh_state();
      return false;
    }

    // Refresh state regardless of whether an event arrived. This makes us robust
    // against missed notifications and lets callers use a periodic timeout as a
    // fallback heartbeat.
    refresh_state();
    return event_received;
  }

  void IpcAppStateMonitor::refresh_state() {
    auto value = state_entry_.get();
    cached_state_ = *value;
  }

  auto IpcAppStateMonitor::create_node() -> iox2::Node<iox2::ServiceType::Ipc> {
    iox2::set_log_level_from_env_or(iox2::LogLevel::Warn);

    auto node =
        iox2::NodeBuilder().signal_handling_mode(iox2::SignalHandlingMode::Disabled).create<iox2::ServiceType::Ipc>();
    if (!node.has_value()) {
      throw std::runtime_error("Failed to create iceoryx2 node for tts_output state monitor");
    }
    return std::move(node.value());
  }

  auto IpcAppStateMonitor::create_event_service(const iox2::Node<iox2::ServiceType::Ipc>& node,
                                                const std::string& service_name) -> StateEventService {
    auto service =
        node.service_builder(signlang::common::ipc::service_name_from_string(service_name)).event().open_or_create();
    if (!service.has_value()) {
      throw std::runtime_error("Failed to open or create iceoryx2 app state event service in tts_output: " +
                               service_name);
    }
    return std::move(service.value());
  }

  auto IpcAppStateMonitor::create_listener(const StateEventService& service) -> iox2::Listener<iox2::ServiceType::Ipc> {
    auto listener = service.listener_builder().create();
    if (!listener.has_value()) {
      throw std::runtime_error("Failed to create iceoryx2 app state listener in tts_output");
    }
    return std::move(listener.value());
  }

  auto IpcAppStateMonitor::create_blackboard_service(const iox2::Node<iox2::ServiceType::Ipc>& node,
                                                     const std::string& service_name) -> StateBlackboardService {
    auto opened_service = node.service_builder(signlang::common::ipc::service_name_from_string(service_name))
                              .blackboard_opener<state_machine::AppStateKey>()
                              .open();
    if (!opened_service.has_value()) {
      throw std::runtime_error("Failed to open iceoryx2 app state blackboard service in tts_output: " + service_name);
    }
    return std::move(opened_service.value());
  }

  auto IpcAppStateMonitor::create_reader(const StateBlackboardService& service)
      -> iox2::Reader<iox2::ServiceType::Ipc, state_machine::AppStateKey> {
    auto reader = service.reader_builder().create();
    if (!reader.has_value()) {
      throw std::runtime_error("Failed to create iceoryx2 app state blackboard reader in tts_output");
    }
    return std::move(reader.value());
  }

  auto IpcAppStateMonitor::create_state_entry(iox2::Reader<iox2::ServiceType::Ipc, state_machine::AppStateKey>& reader)
      -> iox2::EntryHandle<iox2::ServiceType::Ipc, state_machine::AppStateKey, state_machine::AppState> {
    auto entry = reader.entry<state_machine::AppState>(app_state_key());
    if (!entry.has_value()) {
      throw std::runtime_error("Failed to create iceoryx2 app state blackboard entry reader in tts_output");
    }
    return std::move(entry.value());
  }

  IpcSignlangResultSubscriber::IpcSignlangResultSubscriber(const std::string& service_name,
                                                           std::uint64_t subscriber_buffer_size) :
      node_{create_node()}, subscriber_{create_subscriber(node_, service_name, subscriber_buffer_size)} {}

  auto IpcSignlangResultSubscriber::create_node() -> iox2::Node<iox2::ServiceType::Ipc> {
    iox2::set_log_level_from_env_or(iox2::LogLevel::Warn);

    auto node =
        iox2::NodeBuilder().signal_handling_mode(iox2::SignalHandlingMode::Disabled).create<iox2::ServiceType::Ipc>();
    if (!node.has_value()) {
      throw std::runtime_error("Failed to create iceoryx2 node for tts_output signlang result subscriber");
    }
    return std::move(node.value());
  }

  auto IpcSignlangResultSubscriber::create_subscriber(const iox2::Node<iox2::ServiceType::Ipc>& node,
                                                      const std::string& service_name, std::uint64_t buffer_size)
      -> iox2::Subscriber<iox2::ServiceType::Ipc, signlang_det::SignlangResult, void> {
    auto service = node.service_builder(signlang::common::ipc::service_name_from_string(service_name))
                       .publish_subscribe<signlang_det::SignlangResult>()
                       .open_or_create();
    if (!service.has_value()) {
      throw std::runtime_error("Failed to open signlang result service in tts_output: " + service_name);
    }

    auto subscriber = service.value().subscriber_builder().buffer_size(buffer_size).create();
    if (!subscriber.has_value()) {
      throw std::runtime_error("Failed to create tts_output signlang result subscriber");
    }
    return std::move(subscriber.value());
  }

} // namespace signlang::tts_output
