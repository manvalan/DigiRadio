/**
 * @file    audio_routing.hpp
 * @brief   Switches the ADAU1701 source to the ESP32 I2S input while a
 *          stream plays, and back afterwards.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */
#pragma once

namespace audio {
class AudioService;
} // namespace audio

namespace audio_routing {

/**
 * @brief    init — remember the audio service used by claim()/restore().
 *
 * @dname    init
 * @param    audio  Service owning the ADAU1701 (static lifetime).
 * @pubstate stores the pointer.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
void init(audio::AudioService& audio);

/**
 * @brief    claimEsp32Input — route the ESP32 I2S pair (MX1 index 1) to
 *           the output, remembering the source that was selected.
 *
 * Until 0.10.0 only the app did this, with a second request after
 * POST /api/streaming: if that request failed, or the stream was started
 * from the web UI, the radio kept playing FM/DAB over a silent stream.
 * Called by whoever holds the shared I2S sink; not persisted.
 *
 * @dname    claimEsp32Input
 * @return   true when the ESP32 input is now selected.
 * @pubstate safeloads MX1; remembers the previous source.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] bool claimEsp32Input();

/**
 * @brief    restorePreviousSource — undo claimEsp32Input().
 *
 * @dname    restorePreviousSource
 * @pubstate safeloads MX1 back to the remembered source, if any.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
void restorePreviousSource();

} // namespace audio_routing
