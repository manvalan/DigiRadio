/**
 * @file    NvsPlatformInit.hpp
 * @brief   Encrypted NVS partition bring-up for ISecureStore backends.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-07
 */
#pragma once

#include <expected>

namespace secure_store {

/**
 * @brief    NvsInitError — failure mode for encrypted NVS initialisation.
 *
 * @dname    NvsInitError
 * @return   n/a (type)
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
enum class NvsInitError {
    EraseFailed, ///< nvs_flash_erase failed during recovery.
    InitFailed,  ///< nvs_flash_init failed after recovery attempt.
};

/**
 * @brief    initEncryptedStorage — initialise default NVS (+ nvs_keys when enabled).
 *
 * @dname    initEncryptedStorage
 * @return   Ok on success, or NvsInitError describing the failure.
 * @pubstate Plain NVS by default (encryption off in sdkconfig.defaults).
 *           Erases and retries on layout or encryption-format mismatch.
 *           When CONFIG_NVS_ENCRYPTION is set, uses nvs_keys per ESP-IDF docs.
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
[[nodiscard]] std::expected<void, NvsInitError> initEncryptedStorage() noexcept;

} // namespace secure_store
