/**
 * @file    RegisterWrite.hpp
 * @brief   One SigmaStudio SIGMA_WRITE_REGISTER_BLOCK transaction.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-07
 */
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace core {

/**
 * @brief    RegisterWrite — 16-bit DSP address plus payload bytes.
 *
 * @dname    RegisterWrite
 * @return   n/a (type)
 * @pubstate Owns data_; replayed by Adau1701Driver via SIGMA_WRITE_REGISTER_BLOCK.
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
class RegisterWrite {
public:
    /**
     * @brief    RegisterWrite — construct one download step.
     *
     * @dname    RegisterWrite
     * @param    address  Target address in the ADAU1701 memory map.
     * @param    data     Payload bytes (may exceed 255; driver chunks I2C).
     * @pubstate moves data into data_.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    RegisterWrite(std::uint16_t address, std::vector<std::uint8_t> data);

    /**
     * @brief    address — read the 16-bit target address.
     *
     * @dname    address
     * @return   SigmaStudio register/data address.
     * @pubstate reads address_.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] std::uint16_t address() const noexcept;

    /**
     * @brief    data — read payload bytes.
     *
     * @dname    data
     * @return   Immutable byte span of the write payload.
     * @pubstate reads data_.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] std::span<const std::uint8_t> data() const noexcept;

private:
    std::uint16_t address_;
    std::vector<std::uint8_t> data_;
};

} // namespace core
