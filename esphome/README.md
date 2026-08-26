# ESPHome wrapper for MicroLink

This directory is an addition to the upstream
[CamM2325/microlink](https://github.com/CamM2325/microlink) tree. It exposes
MicroLink as an ESPHome external component so an ESPHome node can join a
tailnet directly, and be reached from Home Assistant over its `100.x` address
with no NAT traversal, subnet router, or VPN client on the far side.

## Why the fork exists

One change outside this directory:
`components/microlink/components/wireguard_lwip/CMakeLists.txt` renames four
poly1305 symbols. ESPHome's `api:` encryption links `esphome/noise-c`, which
vendors the same poly1305-donna code as `wireguard_lwip`; both export
`poly1305_init` / `poly1305_update` / `poly1305_finish` and the
`poly1305_context` type unprefixed, so the two cannot otherwise be linked into
one image. See the comment in that file for the full collision analysis.

## Why this works at all

`ml_wg_mgr.c` registers the WireGuard tunnel as a real lwIP netif -- it splices
itself into `netif_list`, sets `input = tcpip_input`, assigns the `100.x` VPN
address with a /10 netmask, and brings the interface up. It deliberately does
*not* call `netif_set_default()`, so only `100.64.0.0/10` goes down the tunnel
and ordinary traffic keeps using WiFi.

ESPHome's API server binds `INADDR_ANY:6053`. lwIP matches an INADDR_ANY
listener against every netif, so inbound TCP arriving over the tunnel is
accepted with no changes to ESPHome at all, and replies route back out via the
/10. The same is true of ESPHome's OTA server on :3232 -- which means OTA to a
device that is somewhere else entirely.

## Status

Link-test cut. It brings the tunnel up and does nothing else. The question this
version exists to answer is whether the two noise implementations coexist in
one binary; diagnostics sensors, `microlink_rebind()` on WiFi reconnect, and
`priority_peer` come after that answer is yes.

## Usage

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/taylorcoffelt/microlink
      ref: esphome
    components: [microlink]

microlink:
  auth_key: !secret tailscale_auth_key
  device_name: "baby-monitor"
```

ESPHome probes `esphome/components/` before `components/`, so it finds the
wrapper here and ignores the IDF component tree of the same name.
