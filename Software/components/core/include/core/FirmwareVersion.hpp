/**
 * @file    FirmwareVersion.hpp
 * @brief   Strong type for the firmware release identifier string.
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

#include <string>
#include <string_view>

namespace core {

/**
 * @brief    FirmwareVersion — validated firmware release identifier.
 *
 * @dname    FirmwareVersion
 * @param    version  Non-empty semantic version string (e.g. "0.1.0").
 * @return   n/a (type)
 * @pubstate Owns version_ (immutable after construction). No public data
 *           members.
 *
 * Wraps the release string so API endpoints never pass a bare char*.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class FirmwareVersion {
public:
    /**
     * @brief    FirmwareVersion — construct from a non-empty version string.
     *
     * @dname    FirmwareVersion
     * @param    version  Non-empty semantic version (e.g. "0.1.0").
     * @pubstate writes version_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    explicit FirmwareVersion(std::string_view version);

    /**
     * @brief    value — read the version string.
     *
     * @dname    value
     * @return   The stored version string view.
     * @pubstate reads version_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::string_view value() const noexcept;

private:
    std::string version_;
};

} // namespace core
