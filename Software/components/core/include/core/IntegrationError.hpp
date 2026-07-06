/**
 * @file    IntegrationError.hpp
 * @brief   Typed errors for application integration flows.
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
 * @brief    IntegrationError — failure causes for preset recall orchestration.
 *
 * @dname    IntegrationError
 * @return   n/a (type)
 * @pubstate n/a
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
enum class IntegrationError {
    StoreFailed,    ///< NVS load/save failed during startup or recall.
    PresetNotFound, ///< Preset index out of range.
    TuneFailed,     ///< Tuner could not recall the preset target.
    AudioFailed,    ///< ADAU1701 profile could not be applied.
};

} // namespace core
