/**
 * @file    ParseError.hpp
 * @brief   Typed errors for untrusted JSON parsing at the boundary.
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

namespace core {

/**
 * @brief    ParseError — failure causes when parsing network input.
 *
 * @dname    ParseError
 * @return   n/a (type)
 * @pubstate n/a
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
enum class ParseError {
    InvalidJson,
    MissingField,
    InvalidSsid,
    InvalidPassword,
};

} // namespace core
