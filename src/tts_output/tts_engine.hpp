#ifndef SIGNLANG_EYES_TTS_OUTPUT_TTS_ENGINE_HPP
#define SIGNLANG_EYES_TTS_OUTPUT_TTS_ENGINE_HPP

#include <memory>
#include <string>

namespace signlang::tts_output {

  // Abstract interface for a text-to-speech engine.
  // Implementations must be thread-safe because speak() is called from the
  // module's single processing thread.
  class TtsEngine {
  public:
    virtual ~TtsEngine() = default;

    // Synchronously or asynchronously synthesize and play the given text.
    // The caller does not block on completion unless the implementation chooses to.
    virtual void speak(const std::string& text) = 0;
  };

  using TtsEnginePtr = std::unique_ptr<TtsEngine>;

} // namespace signlang::tts_output

#endif // SIGNLANG_EYES_TTS_OUTPUT_TTS_ENGINE_HPP
