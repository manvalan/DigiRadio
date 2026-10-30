/**
 * @file    EbuLatin.hpp
 * @brief   EBU Latin character set (RDS, DAB labels) to UTF-8.
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
#include <string>
#include <string_view>

namespace core {

/**
 * @brief    decodeEbuLatin — one EBU Latin byte as a Unicode code point.
 *
 * @dname    decodeEbuLatin
 * @param    byte  Character from an RDS PS/RT or a DAB label.
 * @return   Code point: controls (< 0x20) become a space so character
 *           positions (RT+, DL Plus markers) stay aligned; 0x20–0x7F
 *           ASCII; rows 0x80–0x9F and 0xC0–0xDF from EN 62106 Annex E /
 *           ETSI TS 101 756 Annex C (accented Latin letters); other high
 *           bytes U+FFFD.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] char32_t decodeEbuLatin(std::uint8_t byte) noexcept;

/**
 * @brief    appendUtf8 — append a code point as UTF-8.
 *
 * @dname    appendUtf8
 * @param    out  Destination string.
 * @param    cp   Unicode code point.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
void appendUtf8(std::string& out, char32_t cp);

/**
 * @brief    ebuLatinToUtf8 — convert EBU Latin text to UTF-8.
 *
 * @dname    ebuLatinToUtf8
 * @param    text  EBU Latin bytes.
 * @return   UTF-8 string, one code point per input byte.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] std::string ebuLatinToUtf8(std::string_view text);

} // namespace core
