/**
 * @file    IDabDataSink.hpp
 * @brief   Receiver for DAB data delivered with the playing service.
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
 * @date    2026-10-26
 */
#pragma once

#include <cstdint>
#include <span>

namespace core {

/**
 * @brief    IDabDataSink — consumer of X-PAD MOT data groups.
 *
 * @dname    IDabDataSink
 * @param    none
 * @return   n/a (interface)
 * @pubstate Public interface: onMotDataGroup(). Implemented by the
 *           slideshow; ITuner::pollBroadcastData() calls it.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
class IDabDataSink {
public:
    virtual ~IDabDataSink() = default;

    /**
     * @brief    onMotDataGroup — one MSC data group carrying MOT (DSCTy 60).
     *
     * @dname    onMotDataGroup
     * @param    serviceId  DAB service the PAD belongs to.
     * @param    group      Raw data group (EN 300 401 §5.3.3), CRC included.
     * @pubstate implementation-defined.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    virtual void onMotDataGroup(std::uint32_t serviceId,
                                std::span<const std::uint8_t> group) = 0;
};

} // namespace core
