#include "common/runtime.hpp"
#include "iceoryx_gateway.hpp"
#include "null_tts_engine.hpp"
#include "piper_tts_engine.hpp"
#include "program_options.hpp"
#include "spdlog/spdlog.h"
#include "state_machine/app_state.hpp"
#include "tts_engine.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace signlang::tts_output {

  namespace {

    auto create_tts_engine(const ProgramOptions& options) -> TtsEnginePtr {
      switch (options.tts_engine_type) {
      case TtsEngineType::Piper:
        return std::make_unique<PiperTtsEngine>(options.tts_model_path, options.tts_config_path);
      case TtsEngineType::Null:
        [[fallthrough]];
      default:
        return std::make_unique<NullTtsEngine>();
      }
    }

    // Extract a std::string from the fixed-size char array used in SignlangResult.
    auto gesture_name_to_string(const signlang_det::SignlangResult& result) -> std::string {
      const auto& buffer = result.gesture_name;
      const auto length = std::string_view{buffer.data(), buffer.size()};
      const auto end = length.find('\0');
      if (end == std::string_view::npos) {
        return std::string{length};
      }
      return std::string{length.substr(0, end)};
    }

    // True when the module should react to sign language recognition results.
    // Only SignLanguageChat is enabled; SignLanguageAi and all other states are ignored.
    auto is_translation_mode_enabled(state_machine::AppState state) -> bool {
      return state == state_machine::AppState::SignLanguageChat;
    }

    struct Deduplicator {
      std::uint32_t duplicate_suppression_ms;
      std::optional<std::uint32_t> last_gesture_id;
      std::chrono::steady_clock::time_point last_speech_time;

      explicit Deduplicator(std::uint32_t suppression_ms) : duplicate_suppression_ms{suppression_ms} {}

      // Returns true if this result should be spoken, false if it is a duplicate
      // within the suppression window.
      auto should_speak(const signlang_det::SignlangResult& result) -> bool {
        if (!result.recognized || result.gesture_id == 0) {
          return false;
        }

        const auto now = std::chrono::steady_clock::now();
        if (duplicate_suppression_ms > 0 && last_gesture_id.has_value() &&
            last_gesture_id.value() == result.gesture_id) {
          const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_speech_time).count();
          if (elapsed_ms < static_cast<std::int64_t>(duplicate_suppression_ms)) {
            return false;
          }
        }

        last_gesture_id = result.gesture_id;
        last_speech_time = now;
        return true;
      }
    };

  } // namespace

} // namespace signlang::tts_output

auto main(int argc, char** argv) -> int {
  using signlang::tts_output::create_tts_engine;
  using signlang::tts_output::Deduplicator;
  using signlang::tts_output::gesture_name_to_string;
  using signlang::tts_output::IpcAppStateMonitor;
  using signlang::tts_output::IpcSignlangResultSubscriber;
  using signlang::tts_output::is_translation_mode_enabled;
  using signlang::tts_output::parse_program_options;
  using signlang::tts_output::ProgramOptions;
  using signlang::state_machine::app_state_name;

  return signlang::runtime::run_module(argc, argv, parse_program_options, [&](const auto& options) {
    spdlog::info("Starting tts_output");
    spdlog::info("State event service: {}", options.state_event_service_name);
    spdlog::info("State blackboard service: {}", options.state_blackboard_service_name);
    spdlog::info("Signlang result service: {}", options.signlang_result_service_name);

    auto state_monitor =
        IpcAppStateMonitor{options.state_event_service_name, options.state_blackboard_service_name};
    auto result_subscriber = IpcSignlangResultSubscriber{options.signlang_result_service_name,
                                                         options.subscriber_buffer_size};
    auto tts_engine = create_tts_engine(options);
    auto deduplicator = Deduplicator{options.duplicate_suppression_ms};

    auto was_enabled = is_translation_mode_enabled(state_monitor.current_state());
    spdlog::info("Initial translation mode enabled: {}", was_enabled);

    while (!signlang::runtime::shutdown_requested()) {
      // Wait for a state change or heartbeat timeout.
      (void)state_monitor.wait_for_state_change(options.state_poll_ms);
      const auto current_state = state_monitor.current_state();
      const auto enabled = is_translation_mode_enabled(current_state);

      if (enabled != was_enabled) {
        spdlog::info("Translation mode changed: {} -> {} (state: {})", was_enabled, enabled,
                     app_state_name(current_state));
        was_enabled = enabled;
        if (!enabled) {
          // Reset deduplication when leaving the mode so the next entry starts fresh.
          deduplicator = Deduplicator{options.duplicate_suppression_ms};
        }
      }

      if (!enabled) {
        continue;
      }

      // In SignLanguageChat mode: drain the latest result and speak it.
      result_subscriber.receive_latest([&](const auto& result) {
        const auto name = gesture_name_to_string(result);
        if (!deduplicator.should_speak(result)) {
          spdlog::debug("Suppressing duplicate TTS for gesture_id={}: {}", result.gesture_id, name);
          return;
        }

        spdlog::info("Speaking gesture_id={}: {}", result.gesture_id, name);
        tts_engine->speak(name);
      });
    }

    spdlog::info("tts_output shutting down");
    return 0;
  });
}
