/**
 * @file    PowerStateJson.hpp
 * @brief   JSON parse/serialise for GET/POST /api/system/power.
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
 * @date    2026-10-21
 */
#pragma once

#include "core/ParseError.hpp"
#include "core/PowerState.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace core {

/**
 * @brief    parsePowerStateJson — parse POST body {"state":"on"|"standby"}.
 *
 * @dname    parsePowerStateJson
 * @param    json  Untrusted request body.
 * @return   PowerState on success; ParseError::MissingField without a
 *           "state" string, InvalidJson for a non-object body or any value
 *           other than "on" / "standby".
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
[[nodiscard]] std::expected<PowerState, ParseError> parsePowerStateJson(
    std::string_view json);

/**
 * @brief    serializePowerStateJson — response body {"state":"on"|"standby"}.
 *
 * @dname    serializePowerStateJson
 * @param    state  Current power state.
 * @return   JSON object string for the HTTP response body.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
[[nodiscard]] std::string serializePowerStateJson(PowerState state);

} // namespace core
