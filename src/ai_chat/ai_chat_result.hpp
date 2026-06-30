#ifndef SIGNLANG_EYES_AI_CHAT_RESULT_HPP
#define SIGNLANG_EYES_AI_CHAT_RESULT_HPP

#include <array>
#include <cstdint>

namespace signlang::ai_chat {

  // Result of an AI chat request, published downstream for TTS or UI modules.
  struct AiChatResult {
    // True when the AI returned a usable response.
    bool success{false};

    // Monotonically increasing request id (reset per module run).
    std::uint32_t request_id{0};

    // AI-generated text to be spoken/displayed. Null-terminated.
    std::array<char, 512> response{};

    // Error message when success == false. Null-terminated.
    std::array<char, 256> error{};
  };

} // namespace signlang::ai_chat

#endif // SIGNLANG_EYES_AI_CHAT_RESULT_HPP
