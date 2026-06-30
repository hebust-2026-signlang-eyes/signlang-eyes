#include "alsa_player.hpp"

#include "spdlog/spdlog.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace signlang::tts_output {

  AlsaPlayer::AlsaPlayer() : handle_{nullptr} {}

  AlsaPlayer::~AlsaPlayer() { drain_and_close(); }

  bool AlsaPlayer::init(int sample_rate) {
    if (handle_ != nullptr) {
      drain_and_close();
    }

    int rc = snd_pcm_open(&handle_, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) {
      spdlog::error("[ALSA] 无法打开默认音频设备: {}", snd_strerror(rc));
      handle_ = nullptr;
      return false;
    }

    snd_pcm_hw_params_t* params = nullptr;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle_, params);
    snd_pcm_hw_params_set_access(handle_, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle_, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(handle_, params, 1);

    int dir = 0;
    unsigned int rate = static_cast<unsigned int>(sample_rate);
    snd_pcm_hw_params_set_rate_near(handle_, params, &rate, &dir);

    snd_pcm_uframes_t frames = 1024;
    snd_pcm_hw_params_set_period_size_near(handle_, params, &frames, &dir);

    rc = snd_pcm_hw_params(handle_, params);
    if (rc < 0) {
      spdlog::error("[ALSA] 无法设置硬件参数: {}", snd_strerror(rc));
      snd_pcm_close(handle_);
      handle_ = nullptr;
      return false;
    }

    return true;
  }

  void AlsaPlayer::play(const float* samples, std::size_t num_samples) {
    if (handle_ == nullptr || samples == nullptr || num_samples == 0) {
      return;
    }

    convert_buffer_.resize(num_samples);
    for (std::size_t i = 0; i < num_samples; ++i) {
      float s = std::clamp(samples[i], -1.0f, 1.0f);
      convert_buffer_[i] = static_cast<int16_t>(s * 32767.0f);
    }

    const int16_t* ptr = convert_buffer_.data();
    std::size_t remaining = num_samples;

    while (remaining > 0) {
      snd_pcm_sframes_t rc = snd_pcm_writei(handle_, ptr, remaining);
      if (rc == -EAGAIN) {
        continue;
      }
      if (rc < 0) {
        rc = snd_pcm_recover(handle_, static_cast<int>(rc), 0);
        if (rc < 0) {
          spdlog::error("[ALSA] 播放恢复失败: {}", snd_strerror(static_cast<int>(rc)));
          break;
        }
        continue;
      }
      ptr += rc;
      remaining -= static_cast<std::size_t>(rc);
    }
  }

  void AlsaPlayer::drain_and_close() {
    if (handle_ != nullptr) {
      snd_pcm_drain(handle_);
      snd_pcm_close(handle_);
      handle_ = nullptr;
    }
  }

  void AlsaPlayer::close() {
    if (handle_ != nullptr) {
      snd_pcm_close(handle_);
      handle_ = nullptr;
    }
  }

  bool AlsaPlayer::is_open() const { return handle_ != nullptr; }

} // namespace signlang::tts_output
