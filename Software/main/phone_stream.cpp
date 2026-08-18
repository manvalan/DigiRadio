/**
 * @file    phone_stream.cpp
 * @brief   net::PhoneStreamSink implementation over the shared I2S sink.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 */

#include "phone_stream.hpp"

#include "esp32_i2s_sink.hpp"

#include <array>
#include <cstdint>

namespace phone_stream {

namespace {

/** Convert one chunk of interleaved 16-bit stereo PCM to the ADAU's
 *  32-bit-slot I2S format and hand it to the shared sink in one write,
 *  same rationale as web_radio_stream.cpp's writeFrame(): one
 *  i2s_channel_write() call per sample would be a needless source of
 *  jitter under sustained streaming. */
bool writePcm16Stereo(const std::int16_t* interleaved,
                      std::size_t frameCount)
{
    constexpr std::size_t kMaxFramesPerCall = 1024U;
    static std::int32_t out[kMaxFramesPerCall * 2U];

    std::size_t offset = 0U;
    while (offset < frameCount) {
        const std::size_t batch =
            (frameCount - offset) < kMaxFramesPerCall
                ? (frameCount - offset)
                : kMaxFramesPerCall;
        for (std::size_t i = 0U; i < batch; ++i) {
            const std::int16_t left = interleaved[(offset + i) * 2U];
            const std::int16_t right = interleaved[(offset + i) * 2U + 1U];
            out[i * 2U] = static_cast<std::int32_t>(left) << 16;
            out[i * 2U + 1U] = static_cast<std::int32_t>(right) << 16;
        }
        if (!esp32_i2s_sink::writeSamples(out, batch * 2U)) {
            return false;
        }
        offset += batch;
    }
    return true;
}

} // namespace

net::PhoneStreamSink& sink() noexcept
{
    static net::PhoneStreamSink instance{
        .tryAcquire = &esp32_i2s_sink::tryAcquire,
        .release = &esp32_i2s_sink::release,
        .writePcm16Stereo = &writePcm16Stereo,
    };
    return instance;
}

} // namespace phone_stream
