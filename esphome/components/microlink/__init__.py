"""ESPHome wrapper for MicroLink - a Tailscale client for the ESP32.

Link-test cut. This brings the tunnel up and does nothing else. Its purpose is
to answer one question: can MicroLink's bundled wireguard_lwip and ESPHome's
noise-c (pulled in by `api:` encryption) coexist in a single image? Diagnostics,
rebind-on-reconnect and priority peer come after that answer is yes.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32
from esphome.const import CONF_ID

CODEOWNERS = ["@taylorcoffelt"]
DEPENDENCIES = ["esp32", "wifi"]

CONF_AUTH_KEY = "auth_key"
CONF_DEVICE_NAME = "device_name"
CONF_MAX_PEERS = "max_peers"

microlink_ns = cg.esphome_ns.namespace("microlink")
MicroLinkComponent = microlink_ns.class_("MicroLinkComponent", cg.Component)

# The fork carrying the poly1305 symbol rename. Without it, wireguard_lwip and
# esphome/noise-c both export unprefixed poly1305_init/update/finish and the
# link fails on duplicate symbols.
MICROLINK_REPO = "https://github.com/taylorcoffelt/microlink"
MICROLINK_REF = "esphome"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MicroLinkComponent),
            cv.Required(CONF_AUTH_KEY): cv.string_strict,
            cv.Optional(CONF_DEVICE_NAME): cv.string_strict,
            cv.Optional(CONF_MAX_PEERS, default=8): cv.int_range(min=1, max=64),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
    # NB: cv.only_with_esp_idf no longer exists in current ESPHome - only the
    # only_with_arduino preset survives alongside the generic form below.
    cv.only_with_framework("esp-idf"),
)


async def to_code(config):
    # Two registrations, not one. Upstream keeps wireguard_lwip at
    # components/microlink/components/wireguard_lwip and symlinks it to the top
    # level for its own examples; ESP-IDF does not recurse into a component's
    # own components/ directory, so each is registered by path.
    esp32.add_idf_component(
        name="microlink",
        repo=MICROLINK_REPO,
        ref=MICROLINK_REF,
        path="components/microlink",
    )
    esp32.add_idf_component(
        name="wireguard_lwip",
        repo=MICROLINK_REPO,
        ref=MICROLINK_REF,
        path="components/microlink/components/wireguard_lwip",
    )

    # Needed the moment the tunnel actually runs; inert for a link test.
    # TLS to the control plane and DERP:
    esp32.add_idf_sdkconfig_option("CONFIG_MBEDTLS_CERTIFICATE_BUNDLE", True)
    esp32.add_idf_sdkconfig_option(
        "CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_CMN", True
    )
    # MapResponse payloads exceed one MTU:
    esp32.add_idf_sdkconfig_option("CONFIG_LWIP_IP4_FRAG", True)
    esp32.add_idf_sdkconfig_option("CONFIG_LWIP_IP4_REASSEMBLY", True)
    # MicroLink's H2/JSON buffers are PSRAM-backed (512KB each by default):
    esp32.add_idf_sdkconfig_option(
        "CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY", True
    )

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_auth_key(config[CONF_AUTH_KEY]))
    if CONF_DEVICE_NAME in config:
        cg.add(var.set_device_name(config[CONF_DEVICE_NAME]))
    cg.add(var.set_max_peers(config[CONF_MAX_PEERS]))
