#include "program_options.hpp"

#include "common/logging_cli.hpp"
#include "cxxopts.hpp"

#include <stdexcept>
#include <string>

namespace signlang::ai_chat {

  auto parse_program_options(int argc, char** argv) -> ProgramOptionsParseResult {
    cxxopts::Options options{"signlang_eyes_ai_chat",
                             "AI chat module for sign language recognition results in SignLanguageAi mode."};

    // clang-format off
    options.add_options()
        ("state-event-service", "iceoryx2 event service name for app state change notifications",
         cxxopts::value<std::string>())
        ("state-blackboard-service", "iceoryx2 blackboard service name for app state storage",
         cxxopts::value<std::string>())
        ("signlang-result-service", "signlang_det result publish-subscribe service name",
         cxxopts::value<std::string>())
        ("ai-chat-result-service", "ai_chat result publish-subscribe service name",
         cxxopts::value<std::string>())
        ("api-key", "DeepSeek/OpenAI-compatible API key",
         cxxopts::value<std::string>())
        ("base-url", "Chat completions API base URL",
         cxxopts::value<std::string>()->default_value("https://api.deepseek.com"))
        ("model", "Model name",
         cxxopts::value<std::string>()->default_value("deepseek-chat"))
        ("system-prompt", "System prompt sent with every request",
         cxxopts::value<std::string>())
        ("ca-cert", "Path to CA certificate bundle; auto-detected if omitted",
         cxxopts::value<std::string>())
        ("max-history-rounds", "Maximum conversation rounds retained for context",
         cxxopts::value<std::size_t>()->default_value(std::to_string(kDefaultMaxHistoryRounds)))
        ("read-timeout", "HTTP read timeout in seconds",
         cxxopts::value<std::uint32_t>()->default_value(std::to_string(kDefaultReadTimeoutSeconds)))
        ("max-retries", "Maximum retry attempts for failed requests",
         cxxopts::value<int>()->default_value(std::to_string(kDefaultMaxRetries)))
        ("subscriber-buffer", "iceoryx2 subscriber queue size",
         cxxopts::value<std::uint64_t>()->default_value(std::to_string(kDefaultSubscriberBufferSize)))
        ("publisher-history", "iceoryx2 publisher history size",
         cxxopts::value<std::uint64_t>()->default_value(std::to_string(kDefaultPublisherHistorySize)))
        ("state-poll-ms", "Maximum wait time between state checks in milliseconds",
         cxxopts::value<std::uint64_t>()->default_value(std::to_string(kDefaultStatePollMs)))
        ("h,help", "Print usage");
    // clang-format on
    signlang::logging::add_cli_options(options);

    const auto parsed_options = options.parse(argc, argv);
    if (parsed_options.count("help") != 0) {
      return ProgramUsage{.text = options.help()};
    }

    if (parsed_options.count("state-event-service") == 0 ||
        parsed_options.count("state-blackboard-service") == 0 ||
        parsed_options.count("signlang-result-service") == 0 ||
        parsed_options.count("ai-chat-result-service") == 0) {
      throw std::runtime_error("Options --state-event-service, --state-blackboard-service, "
                               "--signlang-result-service, and --ai-chat-result-service are required.\n\n" +
                               options.help());
    }

    if (parsed_options.count("api-key") == 0) {
      throw std::runtime_error("Option --api-key is required.\n\n" + options.help());
    }

    const auto subscriber_buffer_size = parsed_options["subscriber-buffer"].as<std::uint64_t>();
    if (subscriber_buffer_size == 0) {
      throw std::runtime_error("--subscriber-buffer must be greater than 0");
    }

    const auto publisher_history_size = parsed_options["publisher-history"].as<std::uint64_t>();
    if (publisher_history_size == 0) {
      throw std::runtime_error("--publisher-history must be greater than 0");
    }

    const auto state_poll_ms = parsed_options["state-poll-ms"].as<std::uint64_t>();
    if (state_poll_ms == 0) {
      throw std::runtime_error("--state-poll-ms must be greater than 0");
    }

    return ProgramOptions{
        .state_event_service_name = parsed_options["state-event-service"].as<std::string>(),
        .state_blackboard_service_name = parsed_options["state-blackboard-service"].as<std::string>(),
        .signlang_result_service_name = parsed_options["signlang-result-service"].as<std::string>(),
        .ai_chat_result_service_name = parsed_options["ai-chat-result-service"].as<std::string>(),
        .api_key = parsed_options["api-key"].as<std::string>(),
        .base_url = parsed_options["base-url"].as<std::string>(),
        .model = parsed_options["model"].as<std::string>(),
        .system_prompt = parsed_options.count("system-prompt") != 0
                             ? parsed_options["system-prompt"].as<std::string>()
                             : std::string{},
        .ca_cert_path =
            parsed_options.count("ca-cert") != 0 ? parsed_options["ca-cert"].as<std::string>() : std::string{},
        .max_history_rounds = parsed_options["max-history-rounds"].as<std::size_t>(),
        .read_timeout_seconds = parsed_options["read-timeout"].as<std::uint32_t>(),
        .max_retries = parsed_options["max-retries"].as<int>(),
        .subscriber_buffer_size = subscriber_buffer_size,
        .publisher_history_size = publisher_history_size,
        .state_poll_ms = state_poll_ms,
        .logging = signlang::logging::parse_cli_options(parsed_options),
    };
  }

} // namespace signlang::ai_chat
