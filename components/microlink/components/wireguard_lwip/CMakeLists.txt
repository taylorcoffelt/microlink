idf_component_register(
    SRCS
        "src/wireguard.c"
        "src/wireguardif.c"
        "src/crypto.c"
        "src/wireguard-platform-esp32.c"
        "src/crypto/refc/blake2s.c"
        "src/crypto/refc/chacha20.c"
        "src/crypto/refc/chacha20poly1305.c"
        "src/crypto/refc/poly1305-donna.c"
        "src/crypto/refc/x25519.c"
    INCLUDE_DIRS
        "src"
        "src/crypto/refc"
    REQUIRES
        lwip
        esp_timer
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    WIREGUARD_CRYPTO_REFC=1

    # --- ESPHome coexistence -------------------------------------------------
    # ESPHome's `api:` encryption links esphome/noise-c, which vendors the very
    # same poly1305-donna implementation this component does. Both export
    # unprefixed poly1305_init / poly1305_update / poly1305_finish and the
    # poly1305_context type, so linking them into a single image is a
    # duplicate-symbol error at link time.
    #
    # Rename ours at compile time. PRIVATE scopes this to this component's own
    # sources: poly1305-donna.c defines the symbols and chacha20poly1305.c is
    # the only caller, both compiled here, so the rename is internally
    # consistent and nothing leaks to consumers of our public headers.
    #
    # blake2s and chacha need no equivalent. noise-c exports BLAKE2s_reset /
    # BLAKE2s_update / BLAKE2s_finish and chacha_keysetup / chacha_ivsetup /
    # chacha_encrypt_bytes, which do not collide with our blake2s_* and
    # chacha20_* / hchacha20. microlink's own nacl_box.c has a third poly1305,
    # but every symbol there is static, so it is unaffected either way.
    poly1305_init=wg_poly1305_init
    poly1305_update=wg_poly1305_update
    poly1305_finish=wg_poly1305_finish
    poly1305_context=wg_poly1305_context
)

# Disable stringop-overread warning for x25519.c (false positive in GCC array bounds check)
set_source_files_properties(
    src/crypto/refc/x25519.c
    PROPERTIES COMPILE_FLAGS "-Wno-stringop-overread"
)
