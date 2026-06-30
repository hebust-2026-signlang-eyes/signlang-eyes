#include "null_tts_engine.hpp"

#include "spdlog/spdlog.h"

namespace signlang::tts_output {

  void NullTtsEngine::speak(const std::string& text) {
    spdlog::info("[TTS] {} ", text);
  }

} // namespace signlang::tts_output
