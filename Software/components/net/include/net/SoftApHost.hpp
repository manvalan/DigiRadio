/**
 * @file    SoftApHost.hpp
 * @brief   RAII wrapper that brings up an ESP32 SoftAP.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */
#pragma once

#include "net/NetError.hpp"
#include "net/SoftApConfig.hpp"

#include <expected>

namespace net {

/**
 * @brief    SoftApHost — owns SoftAP lifecycle on the ESP32 Wi-Fi stack.
 *
 * @dname    SoftApHost
 * @param    config  SoftAP parameters applied on start().
 * @return   n/a (type)
 * @pubstate Owns started_ (whether Wi-Fi AP is running). Stops AP in the
 *           destructor when started.
 *
 * Imperative shell: wraps ESP-IDF Wi-Fi calls; no business logic.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class SoftApHost {
public:
    /**
     * @brief    SoftApHost — construct with configuration to apply on start.
     *
     * @dname    SoftApHost
     * @param    config  SoftAP parameters.
     * @pubstate writes config_, clears started_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    explicit SoftApHost(SoftApConfig config);

    /**
     * @brief    ~SoftApHost — stop the SoftAP if running.
     *
     * @dname    ~SoftApHost
     * @pubstate stops AP when started_ is true.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    ~SoftApHost();

    SoftApHost(const SoftApHost&) = delete;
    SoftApHost& operator=(const SoftApHost&) = delete;

    /**
     * @brief    SoftApHost — move-construct, transferring started state.
     *
     * @dname    SoftApHost
     * @param    other  Source host; left stopped after the move.
     * @pubstate transfers config_ and started_ from other.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    SoftApHost(SoftApHost&& other) noexcept;

    /**
     * @brief    operator= — move-assign, transferring started state.
     *
     * @dname    operator=
     * @param    other  Source host; left stopped after the move.
     * @return   Reference to this instance.
     * @pubstate transfers config_ and started_ from other.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    SoftApHost& operator=(SoftApHost&& other) noexcept;

    /**
     * @brief    start — bring up the configured SoftAP.
     *
     * @dname    start
     * @return   Ok on success, or a NetError describing the failure.
     * @pubstate writes started_ on success; uses config_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, NetError> start();

private:
    SoftApConfig config_;
    bool started_;
};

} // namespace net
