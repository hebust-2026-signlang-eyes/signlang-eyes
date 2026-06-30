#include "mqtt_client.hpp"

#include "json_utils.hpp"
#include "spdlog/spdlog.h"

#include <thread>

namespace signlang::gps_mqtt {

  namespace {

    auto make_topic(const std::string& prefix, const std::string& device_id, const std::string& suffix)
        -> std::string {
      return prefix + device_id + suffix;
    }

  } // namespace

  class MqttClient::Impl : public virtual mqtt::callback, public virtual mqtt::iaction_listener {
  public:
    explicit Impl(Config config) :
        config_{std::move(config)},
        client_{config_.server_uri, config_.client_id},
        gps_topic_{make_topic("vehicle/", config_.device_id, "/telemetry/gps")},
        emergency_topic_{make_topic("vehicle/", config_.device_id, "/emergency/alert")},
        heartbeat_topic_{make_topic("vehicle/", config_.device_id, "/status/heartbeat")},
        online_topic_{make_topic("vehicle/", config_.device_id, "/status/online")},
        command_topic_{make_topic("server/", config_.device_id, "/command/control")} {}

    void connect() {
      client_.set_callback(*this);

      auto will = mqtt::will_options{online_topic_, online_state_to_json({0, "offline", "unexpected"}), 1, true};

      auto conn_opts = mqtt::connect_options_builder()
                           .clean_session(config_.clean_session)
                           .keep_alive_interval(config_.keep_alive)
                           .connect_timeout(config_.connect_timeout)
                           .will(std::move(will))
                           .finalize();

      if (!config_.username.empty()) {
        conn_opts.set_user_name(config_.username);
      }
      if (!config_.password.empty()) {
        conn_opts.set_password(config_.password);
      }

      conn_opts_ = conn_opts;
      client_.connect(conn_opts_)->wait();
      connected_ = true;
      subscribe_to_commands();
      publish_online({static_cast<std::uint32_t>(std::time(nullptr)), "online", ""});
    }

    void disconnect() {
      if (!connected_) return;
      publish_online({static_cast<std::uint32_t>(std::time(nullptr)), "offline", "graceful"});
      std::this_thread::sleep_for(std::chrono::milliseconds{100});
      client_.disconnect()->wait();
      connected_ = false;
    }

    [[nodiscard]] auto is_connected() const -> bool { return connected_; }

    void set_command_callback(CommandCallback callback) {
      std::lock_guard lock{callback_mutex_};
      command_callback_ = std::move(callback);
    }

    void publish_gps(const GpsReport& report) {
      publish(gps_topic_, gps_report_to_json(report), 1, false);
    }

    void publish_emergency(const EmergencyAlert& alert) {
      publish(emergency_topic_, emergency_alert_to_json(alert), 2, true);
    }

    void publish_heartbeat(const DeviceStatus& status) {
      publish(heartbeat_topic_, heartbeat_to_json(status), 1, false);
    }

    void publish_online(const OnlineState& state) {
      publish(online_topic_, online_state_to_json(state), 1, true);
    }

    [[nodiscard]] auto gps_topic() const -> std::string { return gps_topic_; }
    [[nodiscard]] auto emergency_topic() const -> std::string { return emergency_topic_; }
    [[nodiscard]] auto heartbeat_topic() const -> std::string { return heartbeat_topic_; }
    [[nodiscard]] auto online_topic() const -> std::string { return online_topic_; }
    [[nodiscard]] auto command_topic() const -> std::string { return command_topic_; }

  private:
    void publish(const std::string& topic, const std::string& payload, int qos, bool retained) {
      try {
        client_.publish(topic, payload, qos, retained);
      } catch (const mqtt::exception& e) {
        spdlog::warn("MQTT publish failed (topic={}): {}", topic, e.what());
      }
    }

    void subscribe_to_commands() {
      try {
        client_.subscribe(command_topic_, 1, nullptr, *this);
      } catch (const mqtt::exception& e) {
        spdlog::warn("MQTT command subscription failed: {}", e.what());
      }
    }

    void reconnect() {
      std::this_thread::sleep_for(std::chrono::seconds{2});
      try {
        client_.connect(conn_opts_, nullptr, *this);
      } catch (const mqtt::exception& e) {
        spdlog::error("MQTT reconnect failed: {}", e.what());
      }
    }

    // mqtt::callback
    void connection_lost(const std::string& cause) override {
      connected_ = false;
      spdlog::warn("MQTT connection lost: {}", cause.empty() ? "unknown" : cause);
      reconnect();
    }

    void message_arrived(mqtt::const_message_ptr msg) override {
      if (!msg) return;
      std::lock_guard lock{callback_mutex_};
      if (command_callback_) {
        command_callback_(msg->get_topic(), msg->to_string());
      }
    }

    void delivery_complete(mqtt::delivery_token_ptr /*tok*/) override {}

    void connected(const std::string& /*cause*/) override {
      connected_ = true;
      spdlog::info("MQTT connected to {}", config_.server_uri);
      subscribe_to_commands();
      publish_online({static_cast<std::uint32_t>(std::time(nullptr)), "online", ""});
    }

    // mqtt::iaction_listener
    void on_failure(const mqtt::token& /*tok*/) override { spdlog::warn("MQTT action failed"); }

    void on_success(const mqtt::token& /*tok*/) override {}

    Config config_;
    mqtt::async_client client_;
    mqtt::connect_options conn_opts_;
    std::string gps_topic_;
    std::string emergency_topic_;
    std::string heartbeat_topic_;
    std::string online_topic_;
    std::string command_topic_;
    CommandCallback command_callback_;
    mutable std::mutex callback_mutex_;
    std::atomic<bool> connected_{false};
  };

  MqttClient::MqttClient(Config config) : impl_{std::make_unique<Impl>(std::move(config))} {}

  MqttClient::~MqttClient() = default;

  void MqttClient::connect() { impl_->connect(); }

  void MqttClient::disconnect() { impl_->disconnect(); }

  auto MqttClient::is_connected() const -> bool { return impl_->is_connected(); }

  void MqttClient::set_command_callback(CommandCallback callback) { impl_->set_command_callback(std::move(callback)); }

  void MqttClient::publish_gps(const GpsReport& report) { impl_->publish_gps(report); }

  void MqttClient::publish_emergency(const EmergencyAlert& alert) { impl_->publish_emergency(alert); }

  void MqttClient::publish_heartbeat(const DeviceStatus& status) { impl_->publish_heartbeat(status); }

  void MqttClient::publish_online(const OnlineState& state) { impl_->publish_online(state); }

  auto MqttClient::gps_topic() const -> std::string { return impl_->gps_topic(); }
  auto MqttClient::emergency_topic() const -> std::string { return impl_->emergency_topic(); }
  auto MqttClient::heartbeat_topic() const -> std::string { return impl_->heartbeat_topic(); }
  auto MqttClient::online_topic() const -> std::string { return impl_->online_topic(); }
  auto MqttClient::command_topic() const -> std::string { return impl_->command_topic(); }

} // namespace signlang::gps_mqtt
