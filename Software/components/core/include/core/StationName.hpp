/**
 * @file    StationName.hpp
 * @brief   Strong type for a user-visible station preset label.
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

#include "core/ParseError.hpp"

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace core {

/**
 * @brief    StationName — validated preset display name (1–32 UTF-8 bytes).
 *
 * @dname    StationName
 * @return   n/a (type)
 * @pubstate Owns name_ (immutable after construction).
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class StationName {
public:
    /** Maximum label length stored in NVS. */
    static constexpr std::size_t kMaxLength = 32;

    /**
     * @brief    tryFrom — validate a station name at the HTTP boundary.
     *
     * @dname    tryFrom
     * @param    raw  Untrusted label from JSON input.
     * @return   StationName on success, or ParseError::MissingField.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] static std::expected<StationName, ParseError>
    tryFrom(std::string_view raw);

    /**
     * @brief    value — read the stored label.
     *
     * @dname    value
     * @return   Validated preset name.
     * @pubstate reads name_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::string_view value() const noexcept;

private:
    explicit StationName(std::string name);

    std::string name_;
};

} // namespace core
