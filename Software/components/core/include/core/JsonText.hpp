/**
 * @file    JsonText.hpp
 * @brief   Decode the escapes of a JSON string value.
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
 * @date    2026-10-27
 */
#pragma once

#include <string>
#include <string_view>

namespace core {

/**
 * @brief    unescapeJsonString — resolve the escapes of a raw JSON string.
 *
 * @dname    unescapeJsonString
 * @param    raw  Text between the quotes, as sent (slashes may arrive
 *                escaped, as backslash-slash).
 * @return   UTF-8 text with the standard JSON escapes decoded (quote,
 *           backslash, slash, b, f, n, r, t and u-XXXX code points with
 *           surrogate pairs joined); a malformed escape is kept as written.
 * @pubstate none
 *
 * Swift's JSONEncoder (the iOS app) escapes every slash by default, so a
 * URL arrives with backslash-slash pairs: without this the http(s):// checks
 * rejected every web radio URL from the app.
 *
 * @author   Michele Bigi
 * @date     2026-10-27
 */
[[nodiscard]] std::string unescapeJsonString(std::string_view raw);

} // namespace core
