/**
 * @file    EbuLatin.cpp
 * @brief   EBU Latin to UTF-8 conversion.
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

#include "core/EbuLatin.hpp"

#include <array>

namespace core {

namespace {

constexpr char32_t kReplacement = U'�';

constexpr std::array<char32_t, 32> kEbuRow8 = {
    U'á', U'à', U'é', U'è', U'í', U'ì', U'ó', U'ò',
    U'ú', U'ù', U'Ñ', U'Ç', U'Ş', U'ß', U'¡', U'Ĳ',
    U'â', U'ä', U'ê', U'ë', U'î', U'ï', U'ô', U'ö',
    U'û', U'ü', U'ñ', U'ç', U'ş', U'ğ', U'ı', U'ĳ'};
constexpr std::array<char32_t, 32> kEbuRowC = {
    U'Á', U'À', U'É', U'È', U'Í', U'Ì', U'Ó', U'Ò',
    U'Ú', U'Ù', U'Ř', U'Č', U'Š', U'Ž', U'Đ', U'Ŀ',
    U'Â', U'Ä', U'Ê', U'Ë', U'Î', U'Ï', U'Ô', U'Ö',
    U'Û', U'Ü', U'ř', U'č', U'š', U'ž', U'đ', U'ŀ'};

} // namespace

char32_t decodeEbuLatin(std::uint8_t byte) noexcept
{
    if (byte < 0x20U) {
        return U' ';
    }
    if (byte < 0x80U) {
        return byte;
    }
    if (byte < 0xA0U) {
        return kEbuRow8[byte - 0x80U];
    }
    if (byte >= 0xC0U && byte < 0xE0U) {
        return kEbuRowC[byte - 0xC0U];
    }
    return kReplacement;
}

void appendUtf8(std::string& out, char32_t cp)
{
    if (cp < 0x80U) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800U) {
        out.push_back(static_cast<char>(0xC0U | (cp >> 6U)));
        out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
    } else if (cp < 0x10000U) {
        out.push_back(static_cast<char>(0xE0U | (cp >> 12U)));
        out.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
    } else {
        out.push_back(static_cast<char>(0xF0U | (cp >> 18U)));
        out.push_back(static_cast<char>(0x80U | ((cp >> 12U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
    }
}

std::string ebuLatinToUtf8(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        appendUtf8(out, decodeEbuLatin(static_cast<std::uint8_t>(c)));
    }
    return out;
}

} // namespace core
