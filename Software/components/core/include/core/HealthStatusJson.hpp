/**
 * @file    HealthStatusJson.hpp
 * @brief   JSON serialisation for HealthStatus (pure core, no ESP-IDF).
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

#include "core/HealthStatus.hpp"

#include <string>

namespace core {

/**
 * @brief    serializeHealthStatusJson — render HealthStatus as JSON.
 *
 * @dname    serializeHealthStatusJson
 * @param    status  Health DTO to serialise.
 * @return   JSON object string, e.g. {"status":"ok","fw":"0.1.0"}.
 * @pubstate none
 *
 * Deterministic, allocation-on-stack-then-heap for the returned string.
 * Used by the HTTP shell; tested on the host without hardware.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::string serializeHealthStatusJson(const HealthStatus& status);

} // namespace core
