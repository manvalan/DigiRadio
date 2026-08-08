/**
 * @file    WebRadioService.hpp
 * @brief   Live, persisted config for the internet radio streaming task.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-08-08
 */
#pragma once

#include "core/ISecureStore.hpp"
#include "core/StoreError.hpp"
#include "core/WebRadioConfig.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <expected>

namespace webradio {

/**
 * @brief    WebRadioService — thread-safe streaming config for HTTP + task.
 *
 * @dname    WebRadioService
 * @return   n/a (type)
 * @pubstate Borrows core::ISecureStore for life. Owns a FreeRTOS mutex
 *           guarding an in-RAM copy of the config; the streaming task in
 *           main/web_radio_stream.cpp polls config() to react to changes
 *           without a reboot.
 *
 * @author   Michele Bigi
 * @date     2026-08-08
 */
class WebRadioService {
public:
    /**
     * @brief    WebRadioService — load persisted config or factory default.
     *
     * @dname    WebRadioService
     * @param    store  Secure store for persistence (must outlive this).
     * @pubstate reads store via loadWebRadioConfigJson(); creates mutex_.
     *
     * @author   Michele Bigi
     * @date     2026-08-08
     */
    explicit WebRadioService(core::ISecureStore& store);

    ~WebRadioService();

    WebRadioService(const WebRadioService&) = delete;
    WebRadioService& operator=(const WebRadioService&) = delete;

    /**
     * @brief    config — read a thread-safe snapshot of the current config.
     *
     * @dname    config
     * @return   Copy of the live WebRadioConfig.
     * @pubstate locks mutex_ for the copy.
     *
     * @author   Michele Bigi
     * @date     2026-08-08
     */
    [[nodiscard]] core::WebRadioConfig config() const;

    /**
     * @brief    setConfig — persist and apply a new streaming config.
     *
     * @dname    setConfig
     * @param    config  Validated config from the HTTP layer.
     * @return   Ok on success, or a StoreError.
     * @pubstate persists via store_ first, then updates the live copy under
     *           mutex_ so a failed save never desyncs the running task.
     *
     * @author   Michele Bigi
     * @date     2026-08-08
     */
    [[nodiscard]] std::expected<void, core::StoreError> setConfig(
        const core::WebRadioConfig& config);

private:
    core::ISecureStore& store_;
    SemaphoreHandle_t mutex_;
    core::WebRadioConfig config_;
};

} // namespace webradio
