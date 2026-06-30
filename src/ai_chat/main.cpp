#include "ai_chat_client.hpp"
#include "ai_chat_result.hpp"
#include "common/runtime.hpp"
#include "iceoryx_gateway.hpp"
#include "program_options.hpp"
#include "spdlog/spdlog.h"
#include "state_machine/app_state.hpp"

#include <cstring>
#include <string>

namespace signlang::ai_chat {

  auto gesture_name_to_string(const signlang_det::SignlangResult& result) -> std::string {
    const auto& buffer = result.gesture_name;
    const auto length = std::string_view{buffer.data(), buffer.size()};
    const auto end = length.find('\0');
    if (end == std::string_view::npos) {
      return std::string{length};
    }
    return std::string{length.substr(0, end)};
  }

  auto is_ai_mode_enabled(state_machine::AppState state) -> bool { return state == state_machine::AppState::SignLanguageAi; }

  void copy_to_buffer(const std::string& src, std::span<char> dst) {
    const auto len = std::min(src.size(), dst.size() - 1);
    std::memcpy(dst.data(), src.data(), len);
    dst[len] = '\0';
  }

} // namespace signlang::ai_chat

auto main(int argc, char** argv) -> int {
  using signlang::ai_chat::AiChatClient;
  using signlang::ai_chat::AiChatResult;
  using signlang::ai_chat::copy_to_buffer;
  using signlang::ai_chat::gesture_name_to_string;
  using signlang::ai_chat::IpcAiChatResultPublisher;
  using signlang::ai_chat::IpcAppStateMonitor;
  using signlang::ai_chat::IpcSignlangResultSubscriber;
  using signlang::ai_chat::is_ai_mode_enabled;
  using signlang::ai_chat::parse_program_options;
  using signlang::ai_chat::ProgramOptions;
  using signlang::state_machine::app_state_name;

  return signlang::runtime::run_module(argc, argv, parse_program_options, [&](const auto& options) {
    spdlog::info("Starting ai_chat");
    spdlog::info("AI base URL: {}", options.base_url);
    spdlog::info("AI model: {}", options.model);

    auto state_monitor = IpcAppStateMonitor{options.state_event_service_name, options.state_blackboard_service_name};
    auto result_subscriber =
        IpcSignlangResultSubscriber{options.signlang_result_service_name, options.subscriber_buffer_size};
    auto result_publisher = IpcAiChatResultPublisher{options.ai_chat_result_service_name, options.publisher_history_size};

    AiChatClient ai_client{AiChatClient::Config{
        .api_key = options.api_key,
        .base_url = options.base_url,
        .model = options.model,
        .system_prompt = options.system_prompt,
        .ca_cert_path = options.ca_cert_path,
        .max_history_rounds = options.max_history_rounds,
        .read_timeout = std::chrono::seconds{options.read_timeout_seconds},
        .max_retries = options.max_retries,
    }};

    auto was_enabled = is_ai_mode_enabled(state_monitor.current_state());
    spdlog::info("Initial AI mode enabled: {}", was_enabled);

    std::uint32_t request_id = 0;

    while (!signlang::runtime::shutdown_requested()) {
      (void)state_monitor.wait_for_state_change(options.state_poll_ms);
      const auto current_state = state_monitor.current_state();
      const auto enabled = is_ai_mode_enabled(current_state);

      if (enabled != was_enabled) {
        spdlog::info("AI mode changed: {} -> {} (state: {})", was_enabled, enabled, app_state_name(current_state));
        was_enabled = enabled;
        if (!enabled) {
          ai_client.clear_history();
        }
      }

      if (!enabled) {
        continue;
      }

      result_subscriber.receive_latest([&](const auto& result) {
        if (!result.recognized) {
          return;
        }

        const auto gesture_name = gesture_name_to_string(result);
        if (gesture_name.empty()) {
          return;
        }

        spdlog::info("AI chat request from gesture_id={}: {}", result.gesture_id, gesture_name);
        const auto [success, response_text] = ai_client.chat(gesture_name);

        AiChatResult chat_result{};
        chat_result.success = success;
        chat_result.request_id = ++request_id;
        if (success) {
          copy_to_buffer(response_text, chat_result.response);
          spdlog::info("AI chat response #{}: {}", chat_result.request_id, response_text);
        } else {
          copy_to_buffer(response_text, chat_result.error);
          spdlog::error("AI chat failed #{}: {}", chat_result.request_id, response_text);
        }

        result_publisher.publish(chat_result);
      });
    }

    spdlog::info("ai_chat shutting down");
    return 0;
  });
}
