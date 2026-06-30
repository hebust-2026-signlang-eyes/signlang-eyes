#include "piper_tts_engine.hpp"

#include "spdlog/spdlog.h"

#include <stdexcept>

namespace signlang::tts_output {

  PiperTtsEngine::PiperTtsEngine(const std::string& model_path, const std::string& config_path) :
      model_path_{model_path},
      config_path_{config_path},
      synth_{nullptr} {
    if (model_path_.empty()) {
      throw std::invalid_argument("Piper model path is empty");
    }

    spdlog::info("Initializing piper-tts with model: {}", model_path_);
    synth_ = piper_create(model_path_.c_str(),
                          config_path_.empty() ? nullptr : config_path_.c_str(),
                          nullptr);
    if (synth_ == nullptr) {
      throw std::runtime_error("Failed to load piper model: " + model_path_);
    }
    spdlog::info("Piper model loaded successfully");
  }

  PiperTtsEngine::~PiperTtsEngine() {
    player_.drain_and_close();
    if (synth_ != nullptr) {
      piper_free(synth_);
      synth_ = nullptr;
    }
  }

  void PiperTtsEngine::speak(const std::string& text) {
    if (text.empty() || synth_ == nullptr) {
      return;
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const auto options = piper_default_synthesize_options(synth_);
    if (piper_synthesize_start(synth_, text.c_str(), &options) != PIPER_OK) {
      spdlog::error("[Piper] 合成启动失败: {}", text);
      return;
    }

    spdlog::info("[Piper] synthesizing: {}", text);

    piper_audio_chunk chunk{};
    bool player_initialized = false;

    int ret = 0;
    while ((ret = piper_synthesize_next(synth_, &chunk)) != PIPER_DONE) {
      if (ret != PIPER_OK) {
        spdlog::error("[Piper] 合成过程中出错");
        break;
      }

      if (chunk.num_samples == 0) {
        continue;
      }

      if (!player_initialized) {
        if (!player_.init(chunk.sample_rate)) {
          spdlog::error("[ALSA] 播放器初始化失败，跳过本次合成");
          break;
        }
        player_initialized = true;
      }

      player_.play(chunk.samples, chunk.num_samples);
    }

    if (player_initialized) {
      player_.drain_and_close();
    }
  }

} // namespace signlang::tts_output
