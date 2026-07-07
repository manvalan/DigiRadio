# Flash and NVS encryption (T8)

DigiRadio stores Wi-Fi credentials, preset lists, audio profiles, and the last
preset index in the `digiradio` NVS namespace. Firmware **0.8.3+** enables:

| Layer | Kconfig | Effect |
|-------|---------|--------|
| Flash encryption | `CONFIG_SECURE_FLASH_ENC_ENABLED` | On-chip transparent flash ciphertext |
| Mode (default) | `CONFIG_SECURE_FLASH_ENCRYPTION_MODE_DEVELOPMENT` | Plaintext download still allowed for bring-up |
| NVS encryption | `CONFIG_NVS_ENCRYPTION` | XTS-AES over NVS entries; keys in `nvs_keys` partition |
| Partition table | `partitions.csv` | `nvs` @ 0x9000, `nvs_keys` @ 0xf000 |

Implementation: `secure_store::initEncryptedStorage()` (called from
`NetBootstrap` before any `NvsSecureStore` access). With `CONFIG_NVS_ENCRYPTION`,
ESP-IDF `nvs_flash_init()` loads or generates keys in the first `nvs_keys`
partition automatically (ESP-IDF v5.5 NVS Encryption guide).

**No encryption keys are stored in this repository.**

## First flash (virgin ESP32-S3)

```bash
cd Software
idf.py set-target esp32s3
idf.py erase-flash flash monitor
```

Provision Wi-Fi via SoftAP UI, save a preset, reboot — credentials should
survive `STA` reconnect.

## Migrating from plain NVS (pre-0.8.3 dev boards)

Encryption changes the on-flash layout. **Erase once** before using encrypted
firmware:

```bash
idf.py erase-flash flash
```

Users must re-provision Wi-Fi and presets after erase.

## Production release mode

After HIL sign-off, build with the production overlay
(`sdkconfig.defaults.production`) so flash encryption uses **RELEASE** mode.
This limits future plaintext downloads — follow Espressif’s flash encryption
checklist for ESP32-S3 before shipping units.

## HIL checklist (requires hardware — pending)

- [ ] Boot log shows `NvsPlatformInit: NVS encryption enabled` and flash encryption enabled
- [ ] `POST /api/wifi` → reboot → STA connects without re-provisioning
- [ ] Preset save/recall and `last_preset` survive power cycle
- [ ] Audio profile round-trip after reboot
- [ ] `espefuse.py summary` shows expected `SPI_BOOT_CRYPT_CNT` after first encrypted boot
- [ ] Optional: UART hex dump of NVS region shows non-plaintext SSID (do not log secrets in CI)

## References

- ESP-IDF v5.5 — [NVS Encryption (ESP32-S3)](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/api-reference/storage/nvs_encryption.html)
- ESP-IDF v5.5 — [Flash Encryption (ESP32-S3)](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/security/flash-encryption.html)
- `Software/partitions.csv`, `Software/sdkconfig.defaults`
