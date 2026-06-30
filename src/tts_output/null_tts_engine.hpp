#ifndef SIGNLANG_EYES_TTS_OUTPUT_NULL_TTS_ENGINE_HPP
#define SIGNLANG_EYES_TTS_OUTPUT_NULL_TTS_ENGINE_HPP

#include "tts_engine.hpp"

namespace signlang::tts_output {

  // TTS engine placeholder that logs the text instead of producing audio.
  // Useful for desktop testing or when no audio hardware is available.
  class NullTtsEngine final : public TtsEngine {
  public:
    void speak(const std::string& text) override;
  };

} // namespace signlang::tts_output

#endif // SIGNLANG_EYES_TTS_OUTPUT_NULL_TTS_ENGINE_HPP
