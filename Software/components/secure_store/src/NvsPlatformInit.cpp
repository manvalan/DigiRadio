/**
 * @file    NvsPlatformInit.cpp
 * @brief   Encrypted NVS partition bring-up implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-07
 */

#include "secure_store/NvsPlatformInit.hpp"

#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

namespace secure_store {

namespace {
constexpr char kTag[] = "NvsPlatformInit";

/**
 * @brief    isRecoverableNvsInitError — true when erase + re-init may help.
 *
 * @dname    isRecoverableNvsInitError
 * @param    err  Return code from nvs_flash_init().
 * @return   true for layout/encryption mismatch errors.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] bool isRecoverableNvsInitError(esp_err_t err) noexcept
{
    if (err == ESP_ERR_NVS_NO_FREE_PAGES
        || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        return true;
    }
#if !CONFIG_NVS_ENCRYPTION
    // Plain-NVS build after encrypted-NVS flash: erase once to migrate.
    return err == ESP_ERR_NVS_WRONG_ENCRYPTION
           || err == ESP_ERR_NVS_CORRUPT_KEY_PART;
#else
    return err == ESP_ERR_NVS_CORRUPT_KEY_PART;
#endif
}

/**
 * @brief    logEncryptionMode — log active NVS security Kconfig (no secrets).
 *
 * @dname    logEncryptionMode
 * @pubstate none; INFO log only.
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
void logEncryptionMode() noexcept
{
#if CONFIG_NVS_ENCRYPTION
    ESP_LOGI(kTag, "NVS encryption enabled");
#if CONFIG_SECURE_FLASH_ENC_ENABLED
    ESP_LOGI(kTag, "Flash encryption enabled (development=%d)",
             static_cast<int>(CONFIG_SECURE_FLASH_ENCRYPTION_MODE_DEVELOPMENT));
#else
    ESP_LOGW(kTag, "NVS encryption without flash encryption — check Kconfig");
#endif
#else
    ESP_LOGI(kTag, "NVS plain mode (encryption off — current bring-up default)");
#endif
}

} // namespace

std::expected<void, NvsInitError> initEncryptedStorage() noexcept
{
    logEncryptionMode();

    esp_err_t err = nvs_flash_init();
    if (err == ESP_OK) {
        return {};
    }

    if (isRecoverableNvsInitError(err)) {
        ESP_LOGW(kTag,
                 "NVS partition incompatible (0x%x) — erasing (stored Wi-Fi "
                 "and presets will be cleared)",
                 static_cast<unsigned>(err));
        err = nvs_flash_erase();
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "nvs_flash_erase failed (0x%x)", static_cast<unsigned>(err));
            return std::unexpected(NvsInitError::EraseFailed);
        }
        err = nvs_flash_init();
    }

    if (err != ESP_OK) {
        ESP_LOGE(kTag, "nvs_flash_init failed (0x%x)", static_cast<unsigned>(err));
        return std::unexpected(NvsInitError::InitFailed);
    }

    return {};
}

} // namespace secure_store
