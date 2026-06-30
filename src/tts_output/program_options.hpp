#ifndef SIGNLANG_EYES_TTS_OUTPUT_PROGRAM_OPTIONS_HPP
#define SIGNLANG_EYES_TTS_OUTPUT_PROGRAM_OPTIONS_HPP

#include "common/logging.hpp"

#include <cstdint>
#include <string>
#include <variant>

namespace signlang::tts_output {

  constexpr auto kDefaultSubscriberBufferSize = std::uint64_t{2};
  constexpr auto kDefaultDuplicateSuppressionMs = std::uint32_t{1000};
  constexpr auto kDefaultStatePollMs = std::uint64_t{50};
  constexpr const char* kDefaultTtsEngine = "null";

  enum class TtsEngineType : std::uint8_t {
    Null,
    Piper,
  };

  struct ProgramOptions {
    std::string state_event_service_name;
    std::string state_blackboard_service_name;
    std::string signlang_result_service_name;
    std::string tts_model_path;
    std::string tts_config_path;
    std::uint64_t subscriber_buffer_size;
    std::uint32_t duplicate_suppression_ms;
    std::uint64_t state_poll_ms;
    TtsEngineType tts_engine_type;
    signlang::logging::Options logging;
  };

  struct ProgramUsage {
    std::string text;
  };

  using ProgramOptionsParseResult = std::variant<ProgramOptions, ProgramUsage>;

  auto parse_program_options(int argc, char** argv) -> ProgramOptionsParseResult;

} // namespace signlang::tts_output

#endif // SIGNLANG_EYES_TTS_OUTPUT_PROGRAM_OPTIONS_HPP
