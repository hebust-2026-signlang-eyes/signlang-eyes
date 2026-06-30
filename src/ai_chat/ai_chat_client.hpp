#ifndef SIGNLANG_EYES_AI_CHAT_CLIENT_HPP
#define SIGNLANG_EYES_AI_CHAT_CLIENT_HPP

#include "json.hpp"

#include <chrono>
#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace signlang::ai_chat {

  // Thin wrapper around DeepSeek-compatible chat-completion API.
  // Uses cpp-httplib with OpenSSL for HTTPS and maintains a bounded message
  // history so that follow-up gestures can be interpreted in context.
  class AiChatClient {
  public:
    struct Config {
      std::string api_key;
      std::string base_url{"https://api.deepseek.com"};
      std::string model{"deepseek-chat"};
      std::string system_prompt{"你是简洁助手。规则：1.只用中文或英文回答，禁止emoji、颜文字、ASCII艺术。"
                                "2.禁止Markdown格式。3.每句话不超过20字，总回答控制在3句话以内。"
                                "4.直接给结论，不要解释过程。"};
      std::string ca_cert_path;
      std::size_t max_history_rounds{5};
      std::chrono::seconds read_timeout{120};
      int max_retries{3};
    };

    explicit AiChatClient(Config config);

    AiChatClient(const AiChatClient&) = delete;
    auto operator=(const AiChatClient&) -> AiChatClient& = delete;

    // Send user input to the AI and return the complete text response.
    // On failure returns {false, error_message}.
    [[nodiscard]] auto chat(const std::string& user_input) -> std::pair<bool, std::string>;

    void clear_history();

  private:
    struct Message {
      std::string role;
      std::string content;
    };

    void trim_history();
    [[nodiscard]] static auto find_ca_cert_path() -> std::string;

    Config config_;
    std::vector<Message> messages_;
    std::mutex mutex_;
  };

} // namespace signlang::ai_chat

#endif // SIGNLANG_EYES_AI_CHAT_CLIENT_HPP
