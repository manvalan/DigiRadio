/**
 * @file    NetState.hpp
 * @brief   Explicit network provisioning state machine.
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
 * @date    2026-07-06
 */
#pragma once

namespace net {

/**
 * @brief    NetState — coarse network provisioning phase.
 *
 * @dname    NetState
 * @return   n/a (type)
 * @pubstate n/a
 *
 * Slice 1 starts in SoftApSetup; Slice 2 adds STA join after provisioning.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
enum class NetState {
    Uninitialized,
    SoftApSetup,
    StaConnecting,
    StaConnected,
};

} // namespace net
