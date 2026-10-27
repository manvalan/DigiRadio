/**
 * @file    DlsDecoder.cpp
 * @brief   DlsDecoder implementation (AN649 §7.14.2, ETSI TS 102 980).
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

#include "core/DlsDecoder.hpp"

#include "core/EbuLatin.hpp"

#include <array>

namespace core {

namespace {

// AN649 Table 21: prefix byte 0 = Toggle[7] RFU[6:5] C[4] Field1[3:0];
// byte 1 = (C=0) Charset[7:4] / (C=1) Link[4].
constexpr std::uint8_t kToggleBit = 0x80U;
constexpr std::uint8_t kCommandBit = 0x10U;
constexpr std::uint8_t kLinkBit = 0x10U;
constexpr std::uint8_t kCmdRemoveLabel = 0x01U;
constexpr std::uint8_t kCmdDlPlus = 0x02U;

// ETSI TS 101 756 charset codes used by DAB labels.
constexpr std::uint8_t kCharsetEbuLatin = 0x00U;
constexpr std::uint8_t kCharsetUtf8 = 0x0FU;

// ETSI TS 102 980 DL Plus content types.
constexpr std::uint8_t kItemTitle = 1U;
constexpr std::uint8_t kItemArtist = 4U;

constexpr char32_t kReplacement = U'�';

[[nodiscard]] std::size_t utf8SequenceLength(std::uint8_t lead) noexcept
{
    if (lead < 0x80U) {
        return 1U;
    }
    if ((lead & 0xE0U) == 0xC0U) {
        return 2U;
    }
    if ((lead & 0xF0U) == 0xE0U) {
        return 3U;
    }
    return (lead & 0xF8U) == 0xF0U ? 4U : 0U;
}

// Decodes one UTF-8 sequence at bytes[i]; returns the code point and
// advances i (one byte on a malformed sequence, yielding U+FFFD).
[[nodiscard]] char32_t decodeUtf8At(std::span<const std::uint8_t> bytes,
                                    std::size_t& i) noexcept
{
    static constexpr std::array<std::uint8_t, 5> kLeadMask = {
        0x00U, 0x7FU, 0x1FU, 0x0FU, 0x07U};
    const std::size_t len = utf8SequenceLength(bytes[i]);
    if (len == 0U || i + len > bytes.size()) {
        ++i;
        return kReplacement;
    }
    char32_t cp = bytes[i] & kLeadMask[len];
    for (std::size_t k = 1U; k < len; ++k) {
        if ((bytes[i + k] & 0xC0U) != 0x80U) {
            ++i;
            return kReplacement;
        }
        cp = (cp << 6U) | (bytes[i + k] & 0x3FU);
    }
    i += len;
    return cp < 0x20U ? U' ' : cp;
}

[[nodiscard]] std::u32string decodeText(std::uint8_t charset,
                                        std::span<const std::uint8_t> bytes)
{
    std::u32string out;
    out.reserve(bytes.size());
    for (std::size_t i = 0U; i < bytes.size();) {
        if (charset == kCharsetUtf8) {
            out.push_back(decodeUtf8At(bytes, i));
            continue;
        }
        const std::uint8_t byte = bytes[i++];
        const bool ascii = byte < 0x80U;
        out.push_back(charset == kCharsetEbuLatin ? decodeEbuLatin(byte)
                      : ascii                     ? char32_t{byte}
                                                  : kReplacement);
    }
    return out;
}

// UTF-8 of text, cut at a code point boundary within the label limit.
[[nodiscard]] std::optional<BroadcastLabel> toLabel(std::u32string_view text)
{
    std::string utf8;
    for (const char32_t cp : text) {
        std::string next;
        appendUtf8(next, cp);
        if (utf8.size() + next.size() > BroadcastLabel::kMaxLength) {
            break;
        }
        utf8 += next;
    }
    return BroadcastLabel::tryFromChipBytes(utf8);
}

} // namespace

void DlsDecoder::reset() noexcept
{
    text_.clear();
    toggle_ = false;
    tags_.clear();
    link_ = false;
    itemRunning_ = false;
}

void DlsDecoder::apply(std::span<const std::uint8_t> payload)
{
    if (payload.size() < 2U) {
        return;
    }
    const std::uint8_t field0 = payload[0];
    const std::uint8_t field1 = payload[1];
    const auto body = payload.subspan(2U);
    if ((field0 & kCommandBit) == 0U) {
        applyMessage((field0 & kToggleBit) != 0U,
                     static_cast<std::uint8_t>(field1 >> 4U), body);
        return;
    }
    const std::uint8_t command = field0 & 0x0FU;
    if (command == kCmdRemoveLabel) {
        reset();
    } else if (command == kCmdDlPlus) {
        applyDlPlus((field1 & kLinkBit) != 0U, body);
    }
}

void DlsDecoder::applyMessage(bool toggle, std::uint8_t charset,
                              std::span<const std::uint8_t> body)
{
    text_ = decodeText(charset, body);
    toggle_ = toggle;
}

void DlsDecoder::applyDlPlus(bool link, std::span<const std::uint8_t> body)
{
    // TS 102 980 §7.2 tags command: CId[7:4] IT[3] IR[2] NT[1:0], then
    // NT+1 tags of 3 bytes: ContentType[6:0], Start[6:0], Length-1[6:0].
    if (body.empty() || (body[0] >> 4U) != 0U) {
        return;
    }
    const std::size_t count = (body[0] & 0x03U) + 1U;
    if (body.size() < 1U + count * 3U) {
        return;
    }
    tags_.clear();
    for (std::size_t t = 0U; t < count; ++t) {
        const auto tag = body.subspan(1U + t * 3U, 3U);
        tags_.push_back(Tag{
            .contentType = static_cast<std::uint8_t>(tag[0] & 0x7FU),
            .start = static_cast<std::uint8_t>(tag[1] & 0x7FU),
            .length = static_cast<std::uint8_t>((tag[2] & 0x7FU) + 1U),
        });
    }
    link_ = link;
    itemRunning_ = (body[0] & 0x04U) != 0U;
}

std::optional<BroadcastLabel> DlsDecoder::label() const
{
    return toLabel(text_);
}

DlPlusItem DlsDecoder::item() const
{
    if (text_.empty() || !itemRunning_ || link_ != toggle_) {
        return {};
    }
    return DlPlusItem{.title = tagged(kItemTitle),
                      .artist = tagged(kItemArtist)};
}

std::optional<BroadcastLabel>
DlsDecoder::tagged(std::uint8_t contentType) const
{
    for (const Tag& tag : tags_) {
        if (tag.contentType != contentType || tag.start >= text_.size()) {
            continue;
        }
        return toLabel(std::u32string_view(text_).substr(tag.start,
                                                         tag.length));
    }
    return std::nullopt;
}

} // namespace core
