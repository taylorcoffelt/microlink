#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"

extern "C" {
#include "microlink.h"
}

namespace esphome {
namespace microlink {

class MicroLinkComponent : public Component {
 public:
  void set_auth_key(const char *auth_key) { this->auth_key_ = auth_key; }
  void set_device_name(const char *device_name) { this->device_name_ = device_name; }
  void set_max_peers(uint8_t max_peers) { this->max_peers_ = max_peers; }

  void setup() override;
  void dump_config() override;

  // AFTER_WIFI (200.0): WiFi is up, the API has not connected yet.
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  microlink_t *handle() const { return this->ml_; }

 protected:
  const char *auth_key_{nullptr};
  const char *device_name_{nullptr};
  uint8_t max_peers_{8};
  microlink_t *ml_{nullptr};
};

}  // namespace microlink
}  // namespace esphome

#endif  // USE_ESP32
