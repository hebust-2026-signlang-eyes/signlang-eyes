#include "ai_chat_client.hpp"

#include "httplib.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cstdlib>
#include <sys/stat.h>
#include <thread>

namespace signlang::ai_chat {

  namespace {

    auto diagnose_error(const httplib::Result& res) -> std::string {
      if (res) {
        return "HTTP " + std::to_string(res->status);
      }
      switch (res.error()) {
      case httplib::Error::Connection:
        return "TCP connection failed";
      case httplib::Error::ConnectionTimeout:
        return "connection timeout";
      case httplib::Error::SSLConnection:
        return "SSL handshake failed";
      case httplib::Error::SSLLoadingCerts:
        return "SSL certificate loading failed";
      case httplib::Error::SSLServerVerification:
        return "SSL server certificate verification failed";
      case httplib::Error::ProxyConnection:
        return "proxy connection failed";
      case httplib::Error::Read:
        return "read timeout";
      case httplib::Error::Write:
        return "write failed";
      default:
        return "unknown network error (code " + std::to_string(static_cast<int>(res.error())) + ")";
      }
    }

    auto is_retryable(const httplib::Result& res) -> bool {
      if (!res)
        return true;
      const int status = res->status;
      return status == 429 || status == 408 || (status >= 500 && status < 600);
    }

    class LineBuffer {
    public:
      void append(const char* data, std::size_t len) { buffer_.append(data, len); }
      void clear() { buffer_.clear(); }

      auto getline(std::string& line) -> bool {
        const auto pos = buffer_.find('\n');
        if (pos == std::string::npos) {
          return false;
        }
        line = buffer_.substr(0, pos);
        if (!line.empty() && line.back() == '\r') {
          line.pop_back();
        }
        buffer_.erase(0, pos + 1);
        return true;
      }

    private:
      std::string buffer_;
    };

  } // namespace

  AiChatClient::AiChatClient(Config config) : config_{std::move(config)} {
    if (config_.api_key.empty()) {
      throw std::invalid_argument("AI API key is empty");
    }
    if (config_.ca_cert_path.empty()) {
      config_.ca_cert_path = find_ca_cert_path();
    }
    if (!config_.system_prompt.empty()) {
      messages_.push_back({"system", config_.system_prompt});
    }
  }

  void AiChatClient::clear_history() {
    std::lock_guard<std::mutex> lock{mutex_};
    messages_.clear();
    if (!config_.system_prompt.empty()) {
      messages_.push_back({"system", config_.system_prompt});
    }
  }

  auto AiChatClient::chat(const std::string& user_input) -> std::pair<bool, std::string> {
    if (user_input.empty()) {
      return {false, "empty input"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    messages_.push_back({"user", user_input});
    trim_history();

    httplib::Client cli{config_.base_url};
    cli.set_read_timeout(static_cast<time_t>(config_.read_timeout.count()), 0);
    cli.set_follow_location(true);

    if (!config_.ca_cert_path.empty()) {
      cli.set_ca_cert_path(config_.ca_cert_path.c_str());
    } else {
      spdlog::warn("[AI] CA certificate not found; SSL verification may fail");
    }

    nlohmann::json request_body = {
        {"model", config_.model},
        {"messages", nlohmann::json::array()},
        {"stream", true},
        {"temperature", 0.7},
    };
    for (const auto& msg : messages_) {
      request_body["messages"].push_back({{"role", msg.role}, {"content", msg.content}});
    }
    const std::string payload = request_body.dump();

    httplib::Headers headers = {
        {"Content-Type", "application/json"},
        {"Authorization", "Bearer " + config_.api_key},
    };

    std::string full_reply;
    LineBuffer buffer;

    const auto process_sse_line = [&full_reply](const std::string& line) {
      if (line.rfind("data: ", 0) != 0)
        return;
      const std::string data = line.substr(6);
      if (data == "[DONE]")
        return;
      try {
        const auto j = nlohmann::json::parse(data);
        const auto choices = j.value("choices", nlohmann::json::array());
        if (choices.empty())
          return;
        const auto content = choices[0].value("delta", nlohmann::json::object()).value("content", "");
        full_reply += content;
      } catch (const std::exception&) {
        // Ignore incomplete JSON slices during streaming.
      }
    };

    for (int attempt = 0; attempt <= config_.max_retries; ++attempt) {
      full_reply.clear();
      buffer.clear();

      if (attempt > 0) {
        const std::chrono::seconds delay{1 << (attempt - 1)};
        spdlog::warn("[AI] request failed, retrying in {}s (attempt {}/{})", delay.count(), attempt,
                     config_.max_retries);
        std::this_thread::sleep_for(delay);
      }

      const auto res = cli.Post("/chat/completions", headers, payload, "application/json",
                                [&buffer, &process_sse_line](const char* data, std::size_t data_length) -> bool {
                                  buffer.append(data, data_length);
                                  std::string line;
                                  while (buffer.getline(line)) {
                                    process_sse_line(line);
                                  }
                                  return true;
                                });

      if (res && res->status == 200) {
        if (!full_reply.empty()) {
          messages_.push_back({"assistant", full_reply});
        }
        return {true, full_reply};
      }

      if (!is_retryable(res)) {
        std::string err = diagnose_error(res);
        if (res && res->status == 401) {
          err += " (invalid API key)";
        }
        return {false, err};
      }

      if (attempt == config_.max_retries) {
        return {false, diagnose_error(res) + " (retries exhausted)"};
      }
    }

    return {false, "unknown error"};
  }

  void AiChatClient::trim_history() {
    std::vector<Message> system_msgs;
    std::vector<Message> chat_msgs;
    for (const auto& m : messages_) {
      if (m.role == "system") {
        system_msgs.push_back(m);
      } else {
        chat_msgs.push_back(m);
      }
    }

    const std::size_t keep = config_.max_history_rounds * 2;
    if (chat_msgs.size() > keep) {
      chat_msgs.erase(chat_msgs.begin(), chat_msgs.end() - keep);
    }

    messages_ = std::move(system_msgs);
    messages_.insert(messages_.end(), chat_msgs.begin(), chat_msgs.end());
  }

  auto AiChatClient::find_ca_cert_path() -> std::string {
    const char* candidates[] = {
        "/etc/ssl/certs/ca-certificates.crt", // Ubuntu/Debian
        "/etc/pki/tls/certs/ca-bundle.crt", // RHEL/CentOS/Fedora
        "/etc/ssl/cert.pem", // Alpine/macOS
        "/etc/ssl/certs", // directory
        "/system/etc/security/cacerts", // Android
        nullptr,
    };
    for (int i = 0; candidates[i] != nullptr; ++i) {
      struct stat st{};
      if (stat(candidates[i], &st) == 0) {
        return std::string{candidates[i]};
      }
    }
    return "";
  }

} // namespace signlang::ai_chat
