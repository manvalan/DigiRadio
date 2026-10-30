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
#include "core/WebRadioStatus.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <cstdint>
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

    /**
     * @brief    generation — counter bumped by every setConfig().
     *
     * The player compares it with the value it started from, so a new
     * URL (or a re-sent one) restarts the stream at once instead of only
     * after the old connection drops.
     *
     * @dname    generation
     * @return   Current generation.
     * @pubstate locks mutex_ for the read.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] std::uint32_t generation() const;

    /**
     * @brief    status — latest snapshot published by the player task.
     *
     * @dname    status
     * @return   Copy of the live WebRadioStatus.
     * @pubstate locks mutex_ for the copy.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] core::WebRadioStatus status() const;

    /**
     * @brief    publishStatus — replace the live status (player task only).
     *
     * @dname    publishStatus
     * @param    status  New snapshot.
     * @pubstate locks mutex_ for the copy.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    void publishStatus(const core::WebRadioStatus& status);

private:
    core::ISecureStore& store_;
    SemaphoreHandle_t mutex_;
    core::WebRadioConfig config_;
    core::WebRadioStatus status_;
    std::uint32_t generation_{0U};
};

} // namespace webradio
