#include "microlink_component.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

namespace esphome {
namespace microlink {

static const char *const TAG = "microlink";

void MicroLinkComponent::setup() {
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
  // setup() never blocks and the ESPHome loop is never starved -- no watchdog
  // feeding required. Expect roughly 15-20s from here to a usable tunnel.
  const esp_err_t err = microlink_start(this->ml_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "microlink_start() failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "MicroLink started; tunnel will come up in the background");
}

void MicroLinkComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MicroLink (Tailscale):");
  ESP_LOGCONFIG(TAG, "  Device name: %s",
                this->device_name_ != nullptr ? this->device_name_ : "(auto from MAC)");
  ESP_LOGCONFIG(TAG, "  Max peers: %u", this->max_peers_);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Setup failed");
  }
}

}  // namespace microlink
}  // namespace esphome

#endif  // USE_ESP32
