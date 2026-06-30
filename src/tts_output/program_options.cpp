#include "program_options.hpp"

#include "common/logging_cli.hpp"
#include "cxxopts.hpp"

#include <stdexcept>
#include <string>

namespace signlang::tts_output {

  auto parse_program_options(int argc, char** argv) -> ProgramOptionsParseResult {
    cxxopts::Options options{"signlang_eyes_tts_output",
                             "Text-to-speech output for sign language recognition results."};

    // clang-format off
    options.add_options()
        ("state-event-service", "iceoryx2 event service name for app state change notifications",
         cxxopts::value<std::string>())
        ("state-blackboard-service", "iceoryx2 blackboard service name for app state storage",
         cxxopts::value<std::string>())
        ("signlang-result-service", "signlang_det result publish-subscribe service name",
         cxxopts::value<std::string>())
        ("tts-engine", "TTS backend: null, piper",
         cxxopts::value<std::string>()->default_value(kDefaultTtsEngine))
        ("tts-model", "Path to TTS model (required when --tts-engine=piper)",
         cxxopts::value<std::string>())
        ("tts-config", "Path to TTS model config; defaults to <model_path>.json",
         cxxopts::value<std::string>())
        ("subscriber-buffer", "iceoryx2 subscriber queue size",
         cxxopts::value<std::uint64_t>()->default_value(std::to_string(kDefaultSubscriberBufferSize)))
        ("duplicate-suppression-ms", "Suppress repeated same gesture_id within this window; 0 disables",
         cxxopts::value<std::uint32_t>()->default_value(std::to_string(kDefaultDuplicateSuppressionMs)))
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
        parsed_options.count("signlang-result-service") == 0) {
      throw std::runtime_error("Options --state-event-service, --state-blackboard-service, and "
                               "--signlang-result-service are required.\n\n" +
                               options.help());
    }

    const auto subscriber_buffer_size = parsed_options["subscriber-buffer"].as<std::uint64_t>();
    if (subscriber_buffer_size == 0) {
      throw std::runtime_error("--subscriber-buffer must be greater than 0");
    }

    const auto duplicate_suppression_ms = parsed_options["duplicate-suppression-ms"].as<std::uint32_t>();

    const auto state_poll_ms = parsed_options["state-poll-ms"].as<std::uint64_t>();
    if (state_poll_ms == 0) {
      throw std::runtime_error("--state-poll-ms must be greater than 0");
    }

    const auto tts_engine_name = parsed_options["tts-engine"].as<std::string>();
    auto tts_engine_type = TtsEngineType::Null;
    if (tts_engine_name == "null") {
      tts_engine_type = TtsEngineType::Null;
    } else if (tts_engine_name == "piper") {
      tts_engine_type = TtsEngineType::Piper;
    } else {
      throw std::runtime_error("Invalid --tts-engine '" + tts_engine_name + "'. Expected: null, piper.");
    }

    std::string tts_model_path;
    if (parsed_options.count("tts-model") != 0) {
      tts_model_path = parsed_options["tts-model"].as<std::string>();
    }
    if (tts_engine_type == TtsEngineType::Piper && tts_model_path.empty()) {
      throw std::runtime_error("--tts-model is required when --tts-engine=piper");
    }

    std::string tts_config_path;
    if (parsed_options.count("tts-config") != 0) {
      tts_config_path = parsed_options["tts-config"].as<std::string>();
    }

    return ProgramOptions{
        .state_event_service_name = parsed_options["state-event-service"].as<std::string>(),
        .state_blackboard_service_name = parsed_options["state-blackboard-service"].as<std::string>(),
        .signlang_result_service_name = parsed_options["signlang-result-service"].as<std::string>(),
        .tts_model_path = std::move(tts_model_path),
        .tts_config_path = std::move(tts_config_path),
        .subscriber_buffer_size = subscriber_buffer_size,
        .duplicate_suppression_ms = duplicate_suppression_ms,
        .state_poll_ms = state_poll_ms,
        .tts_engine_type = tts_engine_type,
        .logging = signlang::logging::parse_cli_options(parsed_options),
    };
  }

} // namespace signlang::tts_output
