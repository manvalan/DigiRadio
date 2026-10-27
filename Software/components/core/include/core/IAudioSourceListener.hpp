/**
 * @file    IAudioSourceListener.hpp
 * @brief   Callback fired while the input switch is muted.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-27
 */
#pragma once

#include "core/ActiveSource.hpp"

namespace core {

/**
 * @brief    IAudioSourceListener — notified after MX1 changes input.
 *
 * Called by AudioService while the master is still faded down, so a
 * listener can change the EQ (for example recall the input's default
 * preset) without the change being heard as a jump.
 *
 * @dname    IAudioSourceListener
 * @return   n/a (interface)
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-27
 */
class IAudioSourceListener {
public:
    virtual ~IAudioSourceListener() = default;

    /**
     * @brief    onSourceSelected — the new input is live (still muted).
     *
     * @dname    onSourceSelected
     * @param    source  Input that MX1 now passes through.
     * @pubstate implementation-defined.
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    virtual void onSourceSelected(ActiveSource source) = 0;
};

} // namespace core
