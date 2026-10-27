/**
 * @file    JsonText.cpp
 * @brief   JSON string unescaping.
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

#include "core/JsonText.hpp"

#include "core/EbuLatin.hpp"

#include <optional>

namespace core {

namespace {

[[nodiscard]] std::optional<char32_t> hex4(std::string_view s, std::size_t at)
{
    if (at + 4U > s.size()) {
        return std::nullopt;
    }
    char32_t value = 0U;
    for (std::size_t i = at; i < at + 4U; ++i) {
        const char c = s[i];
        const int digit = (c >= '0' && c <= '9')   ? c - '0'
                          : (c >= 'a' && c <= 'f') ? c - 'a' + 10
                          : (c >= 'A' && c <= 'F') ? c - 'A' + 10
                                                   : -1;
        if (digit < 0) {
            return std::nullopt;
        }
        value = (value << 4U) | static_cast<char32_t>(digit);
    }
    return value;
}

[[nodiscard]] std::optional<char> simpleEscape(char c) noexcept
{
    switch (c) {
    case '"':
    case '\\':
    case '/':
        return c;
    case 'b':
        return '\b';
    case 'f':
        return '\f';
    case 'n':
        return '\n';
    case 'r':
        return '\r';
    case 't':
        return '\t';
    default:
        return std::nullopt;
    }
}

// \uXXXX at raw[i] (i on the backslash), joining a \uD8xx\uDCxx pair.
// Returns the characters consumed, 0 when malformed.
[[nodiscard]] std::size_t decodeUnicode(std::string_view raw, std::size_t i,
                                        std::string& out)
{
    const auto high = hex4(raw, i + 2U);
    if (!high) {
        return 0U;
    }
    if (*high >= 0xD800U && *high < 0xDC00U
        && raw.substr(i + 6U, 2U) == "\\u") {
        const auto low = hex4(raw, i + 8U);
        if (low && *low >= 0xDC00U && *low < 0xE000U) {
            appendUtf8(out, 0x10000U + ((*high - 0xD800U) << 10U)
                                + (*low - 0xDC00U));
            return 12U;
        }
    }
    appendUtf8(out, *high);
    return 6U;
}

} // namespace

std::string unescapeJsonString(std::string_view raw)
{
    std::string out;
    out.reserve(raw.size());
    for (std::size_t i = 0U; i < raw.size();) {
        if (raw[i] != '\\' || i + 1U >= raw.size()) {
            out.push_back(raw[i++]);
            continue;
        }
        if (const auto simple = simpleEscape(raw[i + 1U]); simple) {
            out.push_back(*simple);
            i += 2U;
            continue;
        }
        const std::size_t used =
            raw[i + 1U] == 'u' ? decodeUnicode(raw, i, out) : 0U;
        if (used == 0U) {
            out.push_back(raw[i++]);
        }
        i += used;
    }
    return out;
}

} // namespace core
