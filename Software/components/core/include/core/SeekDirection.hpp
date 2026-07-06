/**
 * @file    SeekDirection.hpp
 * @brief   FM seek direction selector (core domain).
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
 * @brief    SeekDirection — FM seek scan direction.
 *
 * @dname    SeekDirection
 * @return   n/a (type)
 * @pubstate n/a
 *
 * Replaces bare bool parameters in public tuner APIs (AGENTS.md §2.1).
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
enum class SeekDirection { Up, Down };

} // namespace core
