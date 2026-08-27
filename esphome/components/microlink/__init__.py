"""ESPHome wrapper for MicroLink - a Tailscale client for the ESP32.

Requires `network: enable_ipv6: true` - see the final validator below.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import esp32, socket
from esphome.const import CONF_ID

CODEOWNERS = ["@taylorcoffelt"]
DEPENDENCIES = ["esp32", "wifi", "network"]

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
    # Declare what MicroLink opens so ESPHome sizes CONFIG_LWIP_MAX_SOCKETS to
    # include it. Left alone, ESPHome counts only its own components.
    #   TCP: control-plane TLS (HTTP/2), DERP relay TLS
    #   UDP: magicsock (WireGuard + DISCO share one), STUN v4, STUN v6
    socket.consume_sockets(2, "microlink", socket.SocketType.TCP),
    socket.consume_sockets(3, "microlink", socket.SocketType.UDP),
)


def _validate_ipv6_enabled(config):
    """MicroLink is written against a dual-stack lwIP.

    ESP-IDF builds with LWIP_IPV6=1 by default; ESPHome does not - its network
    component defaults enable_ipv6 to False on esp32. With IPv6 off, lwIP
    collapses ip_addr_t to `struct ip4_addr` (so `.u_addr.ip4` does not exist),
    leaves `struct sockaddr_in6` an incomplete type, and #defines AF_INET6 to
    AF_UNSPEC. ml_wg_mgr.c, ml_stun.c and ml_net_io.c all fail to compile.
    """
    full = fv.full_config.get()
    network_config = full.get("network") or {}
    if not network_config.get("enable_ipv6", False):
        raise cv.Invalid(
            "The 'microlink' component requires lwIP IPv6, which ESPHome "
            "leaves off by default on ESP32. Add this to your configuration:\n\n"
            "network:\n"
            "  enable_ipv6: true\n\n"
            "MicroLink uses IPv6 for STUN and endpoint discovery. Beyond making "
            "it compile, an IPv6 endpoint frequently sidesteps NAT altogether, "
            "which is the difference between a direct WireGuard path and a DERP "
            "relay on carrier networks."
        )
    return config


FINAL_VALIDATE_SCHEMA = _validate_ipv6_enabled


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

    # ml_noise.c uses mbedTLS for the Noise transport cipher. ESP-IDF defaults
    # all three of these to n - standard TLS gets by on AES-GCM - and
    # CHACHAPOLY depends on the other two.
    esp32.add_idf_sdkconfig_option("CONFIG_MBEDTLS_CHACHA20_C", True)
    esp32.add_idf_sdkconfig_option("CONFIG_MBEDTLS_POLY1305_C", True)
    esp32.add_idf_sdkconfig_option("CONFIG_MBEDTLS_CHACHAPOLY_C", True)
    esp32.add_idf_sdkconfig_option("CONFIG_MBEDTLS_HKDF_C", True)

    # TLS to the control plane and DERP:
    esp32.add_idf_sdkconfig_option("CONFIG_MBEDTLS_CERTIFICATE_BUNDLE", True)
    esp32.add_idf_sdkconfig_option(
        "CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_CMN", True
    )

    # MapResponse payloads exceed one MTU:
    esp32.add_idf_sdkconfig_option("CONFIG_LWIP_IP4_FRAG", True)
    esp32.add_idf_sdkconfig_option("CONFIG_LWIP_IP4_REASSEMBLY", True)

    # Keep internal SRAM for MicroLink's tasks by pushing WiFi/lwIP buffers to
    # PSRAM, and give the TCP/IP mailbox room so bursts are not dropped.
    esp32.add_idf_sdkconfig_option("CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP", True)
    esp32.add_idf_sdkconfig_option("CONFIG_LWIP_SO_RCVBUF", True)
    esp32.add_idf_sdkconfig_option("CONFIG_LWIP_TCPIP_RECVMBOX_SIZE", 64)

    # MicroLink's H2/JSON buffers are PSRAM-backed (512KB each by default).
    esp32.add_idf_sdkconfig_option(
        "CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM", True
    )

    # Watchdog headroom, matching what every upstream MicroLink example ships
    # ("Watchdog (extended for TLS handshakes)").
    #
    # I previously left this at ESPHome's 5s default, reasoning that MicroLink
    # runs in its own FreeRTOS tasks and never stalls the ESPHome loop, so a
    # genuine hang ought to surface. That was the wrong analysis: the task
    # watchdog also watches the per-core IDLE tasks. ml_coord is pinned to core
    # 1 at priority 5, ml_wg_mgr sits above it at priority 7 on the same core,
    # and parse_peers_from_map_response() contains no yield of any kind - so a
    # large MapResponse starves IDLE1 and panics, no matter how healthy the
    # ESPHome loop is. hex_to_bytes() calling sscanf once per byte (64 calls per
    # peer for the keys alone) makes that parse far slower than it looks.
    #
    # This buys the headroom back. The proper fix is to yield inside the peer
    # loop and stop using sscanf as a hex parser, both in ml_coord.c.
    esp32.add_idf_sdkconfig_option("CONFIG_ESP_TASK_WDT_TIMEOUT_S", 30)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_auth_key(config[CONF_AUTH_KEY]))
    if CONF_DEVICE_NAME in config:
        cg.add(var.set_device_name(config[CONF_DEVICE_NAME]))
    cg.add(var.set_max_peers(config[CONF_MAX_PEERS]))
