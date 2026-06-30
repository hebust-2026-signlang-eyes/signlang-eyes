#ifndef SIGNLANG_EYES_TTS_OUTPUT_PIPER_TTS_ENGINE_HPP
#define SIGNLANG_EYES_TTS_OUTPUT_PIPER_TTS_ENGINE_HPP

#include "alsa_player.hpp"
#include "piper/piper.h"
#include "tts_engine.hpp"

#include <mutex>
#include <string>

namespace signlang::tts_output {

  // TTS engine backed by piper-tts with ALSA playback.
  // The model path must point to the .onnx file; the config is loaded from
  // model_path + ".json" unless overridden.
  class PiperTtsEngine final : public TtsEngine {
  public:
    PiperTtsEngine(const std::string& model_path, const std::string& config_path);

    PiperTtsEngine(const PiperTtsEngine&) = delete;
    auto operator=(const PiperTtsEngine&) -> PiperTtsEngine& = delete;
    PiperTtsEngine(PiperTtsEngine&&) = delete;
    auto operator=(PiperTtsEngine&&) -> PiperTtsEngine& = delete;

    ~PiperTtsEngine() override;

    void speak(const std::string& text) override;

  private:
    std::string model_path_;
    std::string config_path_;
    piper_synthesizer* synth_;
    AlsaPlayer player_;
    std::mutex mutex_;
  };

} // namespace signlang::tts_output

#endif // SIGNLANG_EYES_TTS_OUTPUT_PIPER_TTS_ENGINE_HPP
