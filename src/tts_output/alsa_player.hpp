#ifndef SIGNLANG_EYES_TTS_OUTPUT_ALSA_PLAYER_HPP
#define SIGNLANG_EYES_TTS_OUTPUT_ALSA_PLAYER_HPP

#include <alsa/asoundlib.h>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace signlang::tts_output {

  // Minimal ALSA playback helper. Converts float PCM to S16_LE and writes
  // to the default ALSA playback device.
  class AlsaPlayer {
  public:
    AlsaPlayer();
    ~AlsaPlayer();

    AlsaPlayer(const AlsaPlayer&) = delete;
    auto operator=(const AlsaPlayer&) -> AlsaPlayer& = delete;
    AlsaPlayer(AlsaPlayer&&) = delete;
    auto operator=(AlsaPlayer&&) -> AlsaPlayer& = delete;

    // Initialize the ALSA device for the given sample rate (mono, S16_LE).
    // Returns false on failure.
    bool init(int sample_rate);

    // Play a buffer of interleaved float samples in [-1.0, 1.0].
    void play(const float* samples, std::size_t num_samples);

    // Drain any remaining samples and close the device.
    void drain_and_close();

    // Close the device without draining.
    void close();

    [[nodiscard]] bool is_open() const;

  private:
    snd_pcm_t* handle_;
    std::vector<int16_t> convert_buffer_;
  };

} // namespace signlang::tts_output

#endif // SIGNLANG_EYES_TTS_OUTPUT_ALSA_PLAYER_HPP
