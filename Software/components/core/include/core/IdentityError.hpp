/**
 * @file    IdentityError.hpp
 * @brief   Failure causes when reading board identity from EEPROM.
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

namespace core {

/**
 * @brief    IdentityError — EEPROM / I2C identity read failures.
 *
 * @dname    IdentityError
 * @return   n/a (type)
 * @pubstate n/a
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
enum class IdentityError {
    I2cFailed,  ///< Bus or device transaction failed.
    ReadFailed, ///< Payload length or EEPROM response invalid.
};

} // namespace core
