/**
 * @file    phone_stream.hpp
 * @brief   net::PhoneStreamSink implementation over the shared I2S sink.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "net/SetupWebServer.hpp"

namespace phone_stream {

/**
 * @brief    sink — the process-lifetime PhoneStreamSink instance.
 *
 * @dname    sink
 * @return   Function-pointer table bound to esp32_i2s_sink, for
 *           net::HttpRouteContext::phoneStream / PUT /api/stream/phone.
 * @pubstate none
 */
[[nodiscard]] net::PhoneStreamSink& sink() noexcept;

} // namespace phone_stream
