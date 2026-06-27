#include "app_state.hpp"

namespace signlang::state_machine {

  auto app_state_name(AppState state) -> const char* {
    switch (state) {
    case AppState::Normal:
      return "normal";
    case AppState::Asr:
      return "asr";
    case AppState::SignLanguageChat:
      return "sign_language_chat";
    case AppState::SignLanguageAi:
      return "sign_language_ai";
    case AppState::DangerousSound:
      return "dangerous_sound";
    }

    return "unknown";
  }

  auto app_state_from_name(std::string_view name) -> std::optional<AppState> {
    if (name == "normal") {
      return AppState::Normal;
    }
    if (name == "asr") {
      return AppState::Asr;
    }
    if (name == "sign_language_chat") {
      return AppState::SignLanguageChat;
    }
    if (name == "sign_language_ai") {
      return AppState::SignLanguageAi;
    }
    if (name == "dangerous_sound") {
      return AppState::DangerousSound;
    }

    return std::nullopt;
  }

  auto is_basic_app_state(AppState state) -> bool {
    switch (state) {
    case AppState::Normal:
    case AppState::Asr:
    case AppState::SignLanguageChat:
    case AppState::SignLanguageAi:
      return true;
    case AppState::DangerousSound:
      return false;
    }

    return false;
  }

} // namespace signlang::state_machine
