#include "iceoryx_gateway.hpp"

#include <chrono>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace signlang::speech_asr {
  namespace {

    auto service_name_from_string(const std::string& service_name) -> iox2::ServiceName {
      const auto parsed_service_name = iox2::ServiceName::create(service_name.c_str());
      if (!parsed_service_name.has_value()) {
        throw std::runtime_error("Invalid iceoryx2 service name: " + service_name);
      }

      return parsed_service_name.value();
    }

    auto steady_timestamp_ns() -> std::uint64_t {
      const auto now = std::chrono::steady_clock::now().time_since_epoch();
      return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
    }

  } // namespace

  IpcAudioSubscriber::IpcAudioSubscriber(const std::string& service_name, std::uint64_t subscriber_buffer_size) :
      node_{create_node()}, subscriber_{create_subscriber(node_, service_name, subscriber_buffer_size)} {}

  auto IpcAudioSubscriber::receive_available(AudioRingBuffer& ring_buffer) -> AudioReceiveStats {
    AudioReceiveStats stats{};

    auto receive_result = subscriber_.receive();
    if (!receive_result.has_value()) {
      throw std::runtime_error("Failed to receive iceoryx2 audio frame sample");
    }

    auto sample = std::move(receive_result.value());
    while (sample.has_value()) {
      if (ring_buffer.push(sample->payload())) {
        ++stats.accepted_count;
      } else {
        ++stats.rejected_count;
      }

      receive_result = subscriber_.receive();
      if (!receive_result.has_value()) {
        throw std::runtime_error("Failed to receive iceoryx2 audio frame sample");
      }
      sample = std::move(receive_result.value());
    }

    return stats;
  }

  auto IpcAudioSubscriber::create_node() -> iox2::Node<iox2::ServiceType::Ipc> {
    iox2::set_log_level_from_env_or(iox2::LogLevel::Warn);

    auto node = iox2::NodeBuilder().create<iox2::ServiceType::Ipc>();
    if (!node.has_value()) {
      throw std::runtime_error("Failed to create iceoryx2 IPC audio subscriber node");
    }

    return std::move(node.value());
  }

  auto IpcAudioSubscriber::create_subscriber(const iox2::Node<iox2::ServiceType::Ipc>& node,
                                             const std::string& service_name,
                                             std::uint64_t subscriber_buffer_size)
      -> iox2::Subscriber<iox2::ServiceType::Ipc, signlang::audio_frontend::AudioFrame, void> {
    auto service =
        node.service_builder(service_name_from_string(service_name))
            .publish_subscribe<signlang::audio_frontend::AudioFrame>()
            .open_or_create();
    if (!service.has_value()) {
      throw std::runtime_error("Failed to open or create iceoryx2 audio service: " + service_name);
    }

    auto subscriber = service.value().subscriber_builder().buffer_size(subscriber_buffer_size).create();
    if (!subscriber.has_value()) {
      throw std::runtime_error("Failed to create iceoryx2 audio subscriber for service: " + service_name);
    }

    return std::move(subscriber.value());
  }

  IpcResultPublisher::IpcResultPublisher(const std::string& service_name) :
      node_{create_node()}, publisher_{create_publisher(node_, service_name)} {}

  void IpcResultPublisher::publish(const SpeechAsrResult& result) {
    auto loan_result = publisher_.loan_uninit();
    if (!loan_result.has_value()) {
      throw std::runtime_error("Failed to loan iceoryx2 speech ASR result sample");
    }

    auto loaned_sample = std::move(loan_result.value());
    auto initialized_sample = loaned_sample.write_payload(SpeechAsrResult{result});
    const auto send_result = iox2::send(std::move(initialized_sample));
    if (!send_result.has_value()) {
      throw std::runtime_error("Failed to publish speech ASR result through iceoryx2");
    }
  }

  auto IpcResultPublisher::create_node() -> iox2::Node<iox2::ServiceType::Ipc> {
    iox2::set_log_level_from_env_or(iox2::LogLevel::Warn);

    auto node = iox2::NodeBuilder().create<iox2::ServiceType::Ipc>();
    if (!node.has_value()) {
      throw std::runtime_error("Failed to create iceoryx2 IPC ASR result publisher node");
    }

    return std::move(node.value());
  }

  auto IpcResultPublisher::create_publisher(const iox2::Node<iox2::ServiceType::Ipc>& node,
                                            const std::string& service_name)
      -> iox2::Publisher<iox2::ServiceType::Ipc, SpeechAsrResult, void> {
    auto service = node.service_builder(service_name_from_string(service_name))
                       .publish_subscribe<SpeechAsrResult>()
                       .open_or_create();
    if (!service.has_value()) {
      throw std::runtime_error("Failed to open or create iceoryx2 speech ASR result service: " + service_name);
    }

    auto publisher = service.value().publisher_builder().create();
    if (!publisher.has_value()) {
      throw std::runtime_error("Failed to create iceoryx2 speech ASR result publisher for service: " + service_name);
    }

    return std::move(publisher.value());
  }

  IpcEnableClient::IpcEnableClient(const std::string& service_name, std::chrono::milliseconds response_timeout) :
      node_{create_node()}, client_{create_client(node_, service_name)}, response_timeout_{response_timeout} {}

  auto IpcEnableClient::query_enabled(std::uint64_t sequence_number) -> AsrEnableResponse {
    const AsrEnableRequest request{
        .sequence_number = sequence_number,
        .timestamp_ns = steady_timestamp_ns(),
    };

    auto pending_response_result = client_.send_copy(request);
    if (!pending_response_result.has_value()) {
      throw std::runtime_error("Failed to send iceoryx2 speech ASR enable request");
    }

    auto pending_response = std::move(pending_response_result.value());
    const auto deadline = std::chrono::steady_clock::now() + response_timeout_;
    while (std::chrono::steady_clock::now() < deadline) {
      auto response_result = pending_response.receive();
      if (!response_result.has_value()) {
        throw std::runtime_error("Failed to receive iceoryx2 speech ASR enable response");
      }

      auto response = std::move(response_result.value());
      if (response.has_value()) {
        return response->payload();
      }

      if (!pending_response.is_connected()) {
        break;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return AsrEnableResponse{
        .sequence_number = 0,
        .request_sequence_number = request.sequence_number,
        .timestamp_ns = steady_timestamp_ns(),
        .enabled = false,
        .language = AsrLanguage::English,
    };
  }

  auto IpcEnableClient::create_node() -> iox2::Node<iox2::ServiceType::Ipc> {
    iox2::set_log_level_from_env_or(iox2::LogLevel::Warn);

    auto node = iox2::NodeBuilder().create<iox2::ServiceType::Ipc>();
    if (!node.has_value()) {
      throw std::runtime_error("Failed to create iceoryx2 IPC ASR enable client node");
    }

    return std::move(node.value());
  }

  auto IpcEnableClient::create_client(const iox2::Node<iox2::ServiceType::Ipc>& node,
                                      const std::string& service_name) -> EnableClient {
    auto service = node.service_builder(service_name_from_string(service_name))
                       .request_response<AsrEnableRequest, AsrEnableResponse>()
                       .max_response_buffer_size(1)
                       .max_active_requests_per_client(1)
                       .max_loaned_requests(1)
                       .open_or_create();
    if (!service.has_value()) {
      throw std::runtime_error("Failed to open or create iceoryx2 speech ASR enable service: " + service_name);
    }

    auto client = service.value().client_builder().create();
    if (!client.has_value()) {
      throw std::runtime_error("Failed to create iceoryx2 speech ASR enable client for service: " + service_name);
    }

    return std::move(client.value());
  }

} // namespace signlang::speech_asr
