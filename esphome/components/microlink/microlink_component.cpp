#include "microlink_component.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"
#include "esphome/components/network/util.h"

namespace esphome {
namespace microlink {

static const char *const TAG = "microlink";

void MicroLinkComponent::setup() {
  // Nothing to do yet. Starting here would have MicroLink resolving
  // controlplane.tailscale.com before the device has associated with anything,
  // producing a DNS error every couple of seconds for as long as WiFi is down.
  if (this->auto_start_) {
    ESP_LOGD(TAG, "Waiting for a network before starting the tunnel");
  } else {
    ESP_LOGD(TAG, "auto_start is off; idle until start() is called");
  }
}

void MicroLinkComponent::loop() {
  if (this->started_ || this->is_failed())
    return;
  // Either we start ourselves, or something asked us to. want_start_ is checked
  // here rather than acted on in start() so that a request made before the
  // device has an address is honoured as soon as it gets one.
  if (!this->auto_start_ && !this->want_start_)
    return;
  if (!network::is_connected())
    return;
  this->start_();
}

void MicroLinkComponent::start_() {
  microlink_config_t cfg = {};
  cfg.auth_key = this->auth_key_;
  cfg.device_name = this->device_name_;
  cfg.enable_derp = true;
  cfg.enable_disco = true;
  cfg.enable_stun = true;
  cfg.max_peers = this->max_peers_;

  this->ml_ = microlink_init(&cfg);
  if (this->ml_ == nullptr) {
    ESP_LOGE(TAG, "microlink_init() failed");
    this->mark_failed();
    return;
  }

  // Returns immediately. MicroLink's own FreeRTOS tasks drive the protocol, so
  // this never blocks the ESPHome loop and needs no watchdog feeding. Expect
  // roughly 15-20s from here to a usable tunnel.
  const esp_err_t err = microlink_start(this->ml_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "microlink_start() failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  this->started_ = true;
  ESP_LOGI(TAG, "Network is up; MicroLink started");
}

void MicroLinkComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MicroLink (Tailscale):");
  ESP_LOGCONFIG(TAG, "  Device name: %s",
                this->device_name_ != nullptr ? this->device_name_ : "(auto from MAC)");
  ESP_LOGCONFIG(TAG, "  Max peers: %u", this->max_peers_);
  ESP_LOGCONFIG(TAG, "  Auto start: %s", YESNO(this->auto_start_));
  ESP_LOGCONFIG(TAG, "  Started: %s", YESNO(this->started_));
  if (this->is_failed())
    ESP_LOGE(TAG, "  Setup failed");
}

}  // namespace microlink
}  // namespace esphome

#endif  // USE_ESP32
