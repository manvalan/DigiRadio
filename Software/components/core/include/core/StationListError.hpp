/**
 * @file    StationListError.hpp
 * @brief   Domain errors for station preset list operations.
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

namespace core {

/**
 * @brief    StationListError — failure causes for preset list CRUD.
 *
 * @dname    StationListError
 * @return   n/a (type)
 * @pubstate n/a
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
enum class StationListError {
    Duplicate,
    Full,
    NotFound,
    SlotInUse,
};

} // namespace core
