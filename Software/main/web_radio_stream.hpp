/**
 * @file    web_radio_stream.hpp
 * @brief   HTTP MP3 stream -> libhelix decode -> ADAU1701 I2S.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

namespace webradio {
class WebRadioService;
} // namespace webradio

namespace web_radio_stream {

/**
 * Task entry point: polls webradio::WebRadioService for the enabled flag
 * and URL, connects when enabled, decodes MP3 to PCM with libhelix-mp3,
 * and writes it to the ADAU1701 over I2S slave TX (board_pins
 * BCLK/LRCLK/I2sDataOut). Idles when disabled, reconnects on stream drop,
 * and re-reads the config on every frame so a UI/API toggle takes effect
 * without a reboot. Loops forever — run it in its own FreeRTOS task with
 * arg pointing at a live WebRadioService.
 *
 * @param arg  webradio::WebRadioService* — must outlive this task.
 */
void run(void* arg);

} // namespace web_radio_stream
