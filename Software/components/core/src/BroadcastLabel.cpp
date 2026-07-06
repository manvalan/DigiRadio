/**
 * @file    BroadcastLabel.cpp
 * @brief   BroadcastLabel implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/BroadcastLabel.hpp"

namespace core {

namespace {

[[nodiscard]] std::string trimChipPadding(std::string_view raw)
{
    std::size_t end = raw.size();
    if (end > BroadcastLabel::kMaxLength) {
        end = BroadcastLabel::kMaxLength;
    }
    while (end > 0U && (raw[end - 1U] == '\0' || raw[end - 1U] == ' ')) {
        --end;
    }
    std::size_t start = 0U;
    while (start < end && raw[start] == ' ') {
        ++start;
    }
    return std::string(raw.substr(start, end - start));
}

} // namespace

std::optional<BroadcastLabel> BroadcastLabel::tryFromChipBytes(
    std::string_view raw)
{
    const std::string trimmed = trimChipPadding(raw);
    if (trimmed.empty()) {
        return std::nullopt;
    }
    return BroadcastLabel(trimmed);
}

BroadcastLabel::BroadcastLabel(std::string label)
    : label_(std::move(label))
{
}

std::string_view BroadcastLabel::value() const noexcept
{
    return label_;
}

} // namespace core
