/**
 * @file    BroadcastLabel.hpp
 * @brief   Validated on-air label text (RDS PS/RT, DAB DLS).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace core {

/**
 * @brief    BroadcastLabel — trimmed UTF-8 label from chip byte buffers.
 *
 * @dname    BroadcastLabel
 * @return   n/a (type)
 * @pubstate Owns label_ (immutable after construction).
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class BroadcastLabel {
public:
    /** Maximum DAB DLS length supported in status JSON. */
    static constexpr std::size_t kMaxLength = 128;

    /**
     * @brief    tryFromChipBytes — parse a NUL- or space-padded chip buffer.
     *
     * @dname    tryFromChipBytes
     * @param    raw  Raw bytes from RDS or DAB data service (may contain NUL).
     * @return   BroadcastLabel when non-empty after trim, otherwise nullopt.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] static std::optional<BroadcastLabel>
    tryFromChipBytes(std::string_view raw);

    /**
     * @brief    value — read the validated label text.
     *
     * @dname    value
     * @return   Trimmed label string.
     * @pubstate reads label_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::string_view value() const noexcept;

private:
    explicit BroadcastLabel(std::string label);

    std::string label_;
};

} // namespace core
