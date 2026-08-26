# Status

## Link test: PASSED

Built clean on ESPHome 2026.8.1 / ESP-IDF 5.5.5, target esp32s3, with
`api:` encryption enabled - so `esphome/noise-c` and MicroLink's bundled
`wireguard_lwip` are linked into the same image with no duplicate symbols.
That was the question this branch existed to answer.

Fits comfortably: ESPHome's 8MB layout gives 3840K per OTA slot.

### What it took

Three problems, none of them the one that killed
[alfs/tailscale-iot](https://github.com/alfs/tailscale-iot) (whose vendored
noise-c collides with ESPHome's own in `api_frame_helper_noise.cpp`):

1. **poly1305 duplicate symbols.** Both `wireguard_lwip` and `esphome/noise-c`
   vendor poly1305-donna and export `poly1305_init` / `_update` / `_finish`
   plus `poly1305_context` unprefixed. Renamed ours via PRIVATE compile
   definitions - see `components/microlink/components/wireguard_lwip/CMakeLists.txt`.
   blake2s and chacha needed nothing: noise-c uses `BLAKE2s_*` and `chacha_*`,
   which don't collide with `blake2s_*` and `chacha20_*`.

2. **lwIP IPv6 off.** MicroLink is written against a dual-stack lwIP. ESP-IDF
   defaults `LWIP_IPV6=1`; ESPHome defaults `enable_ipv6` to False on esp32,
   which collapses `ip_addr_t` to `struct ip4_addr`, leaves `sockaddr_in6`
   incomplete and `#define`s `AF_INET6` to `AF_UNSPEC`. Requires
   `network: enable_ipv6: true` - now enforced by a FINAL_VALIDATE_SCHEMA that
   says so, instead of a thousand lines of lwIP header errors.

3. **mbedTLS ChaCha20-Poly1305 off.** ESP-IDF defaults `MBEDTLS_CHACHA20_C`,
   `MBEDTLS_POLY1305_C` and `MBEDTLS_CHACHAPOLY_C` all to `n`; `ml_noise.c`
   needs them for the Noise transport cipher. MicroLink's standalone examples
   set these in `sdkconfig.defaults`, a file that does not exist when it is
   consumed as a component - so the wrapper sets them.

## Runtime: UNTESTED

Nothing below has been observed on hardware.

- Registration against the control plane, and whether the node gets a 100.x
- Whether ESPHome's API server on `INADDR_ANY:6053` actually accepts inbound
  TCP over the tunnel netif. The theory is sound - `ml_wg_mgr.c` splices a real
  lwIP netif into `netif_list` with `input = tcpip_input` - but theory is all
  it is so far.
- Memory under load, with MicroLink's PSRAM-backed 512KB H2/JSON buffers
  alongside ESPHome.
- Whether the declared socket budget (2 TCP, 3 UDP via
  `socket.consume_sockets`) is actually enough.
- Whether a direct WireGuard path forms from behind a phone hotspot, or
  everything ends up on DERP relay.

## Next

Once runtime is confirmed: diagnostics text_sensors (VPN IP, peer count,
direct-vs-DERP), `microlink_rebind()` hooked to ESPHome's WiFi reconnect so
roaming keeps the same 100.x address, and `priority_peer` pinned to the Home
Assistant host.
