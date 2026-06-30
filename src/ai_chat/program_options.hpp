#ifndef SIGNLANG_EYES_AI_CHAT_PROGRAM_OPTIONS_HPP
#define SIGNLANG_EYES_AI_CHAT_PROGRAM_OPTIONS_HPP

#include "common/logging.hpp"

#include <cstdint>
#include <string>
#include <variant>

namespace signlang::ai_chat {

  constexpr auto kDefaultSubscriberBufferSize = std::uint64_t{2};
  constexpr auto kDefaultPublisherHistorySize = std::uint64_t{2};
  constexpr auto kDefaultStatePollMs = std::uint64_t{50};
  constexpr auto kDefaultMaxHistoryRounds = std::size_t{5};
  constexpr auto kDefaultReadTimeoutSeconds = std::uint32_t{120};
  constexpr auto kDefaultMaxRetries = 3;

  struct ProgramOptions {
    std::string state_event_service_name;
    std::string state_blackboard_service_name;
    std::string signlang_result_service_name;
    std::string ai_chat_result_service_name;

    // AI client configuration
    std::string api_key;
    std::string base_url{"https://api.deepseek.com"};
    std::string model{"deepseek-chat"};
    std::string system_prompt;
    std::string ca_cert_path;
    std::size_t max_history_rounds{kDefaultMaxHistoryRounds};
    std::uint32_t read_timeout_seconds{kDefaultReadTimeoutSeconds};
    int max_retries{kDefaultMaxRetries};

    // IPC tuning
    std::uint64_t subscriber_buffer_size;
    std::uint64_t publisher_history_size;
    std::uint64_t state_poll_ms;

    signlang::logging::Options logging;
  };

  struct ProgramUsage {
    std::string text;
  };

  using ProgramOptionsParseResult = std::variant<ProgramOptions, ProgramUsage>;

  auto parse_program_options(int argc, char** argv) -> ProgramOptionsParseResult;

} // namespace signlang::ai_chat

#endif // SIGNLANG_EYES_AI_CHAT_PROGRAM_OPTIONS_HPP
