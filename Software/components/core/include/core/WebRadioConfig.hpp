/**
 * @file    WebRadioConfig.hpp
 * @brief   Internet radio stream configuration (URL + enable flag).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-08-08
 */
#pragma once

#include <string>

namespace core {

/**
 * @brief    WebRadioConfig — HTTP MP3 stream URL and on/off state.
 *
 * @dname    WebRadioConfig
 * @return   n/a (type)
 * @pubstate Plain value type; url is a plaintext http:// URL (no secrets).
 *
 * @author   Michele Bigi
 * @date     2026-08-08
 */
struct WebRadioConfig {
    bool enabled = false; ///< Whether the streaming task should be playing.
    std::string url;      ///< Plaintext http:// MP3 stream URL.

    /**
     * @brief    factoryDefault — disabled, with a known-good sample URL.
     *
     * @dname    factoryDefault
     * @return   WebRadioConfig with enabled=false and a placeholder URL.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-08-08
     */
    [[nodiscard]] static WebRadioConfig factoryDefault()
    {
        return WebRadioConfig{
            .enabled = false,
            .url = "http://edge.radiomontecarlo.net/RMC.mp3",
        };
    }
};

} // namespace core
