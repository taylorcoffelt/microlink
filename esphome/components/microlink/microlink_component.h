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
  void set_auto_start(bool auto_start) { this->auto_start_ = auto_start; }

  void setup() override;
  void loop() override;
  void dump_config() override;

  // AFTER_WIFI (200.0) orders us after the WiFi component's setup, which is not
  // the same thing as having a network. The real start is gated in loop().
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  microlink_t *handle() const { return this->ml_; }
  bool started() const { return this->started_; }

  // Request the tunnel, for use with auto_start: false.
  //
  // Latches rather than starting inline: loop() still waits for
  // network::is_connected(), so this is safe to call from an automation that
  // fires before the device has an address - the start happens as soon as one
  // exists. Idempotent; calling it once running is a no-op.
  //
  // There is deliberately no counterpart. microlink_start() creates MicroLink's
  // own FreeRTOS tasks and splices a netif into lwIP's global list; tearing
  // that down at runtime is a far bigger change than gating the start, so a
  // tunnel that has come up stays up until the next reboot.
  void start() { this->want_start_ = true; }

  // Erase the node keys cached in NVS so the next boot registers from scratch.
  //
  // Needed when the control plane answers "node not found": the cached machine
  // key refers to a node the tailnet no longer has - a one-shot auth key that
  // was already consumed, or a node deleted from the admin console. MicroLink
  // will otherwise re-present the stale key forever, cycling between
  // registering and reconnecting.
  //
  // microlink_factory_reset() only takes effect before microlink_init(), so
  // call this and then reboot.
  void reset_keys() { microlink_factory_reset(); }

 protected:
  void start_();

  const char *auth_key_{nullptr};
  const char *device_name_{nullptr};
  uint8_t max_peers_{8};
  bool auto_start_{true};
  bool want_start_{false};
  bool started_{false};
  microlink_t *ml_{nullptr};
};

}  // namespace microlink
}  // namespace esphome

#endif  // USE_ESP32
