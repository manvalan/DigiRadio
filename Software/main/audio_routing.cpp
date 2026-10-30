/**
 * @file    audio_routing.cpp
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

#include "audio_routing.hpp"

#include "audio/AudioService.hpp"
#include "core/ActiveSource.hpp"

#include "esp_log.h"

#include <optional>

namespace audio_routing {

namespace {

constexpr char kTag[] = "audio_routing";
/** MX1 pair 1 is the ESP32 I2S input; the enum calls it Bluetooth for
 *  historical reasons (see core/ActiveSource.hpp). */
constexpr core::ActiveSource kEsp32Input = core::ActiveSource::Bluetooth;

audio::AudioService* gAudio = nullptr;
std::optional<core::ActiveSource> gPrevious;

} // namespace

void init(audio::AudioService& audio)
{
    gAudio = &audio;
}

bool claimEsp32Input()
{
    if (gAudio == nullptr) {
        return false;
    }
    const core::ActiveSource current = gAudio->currentProfile().activeSource;
    if (current == kEsp32Input) {
        return true; // the app already switched (or an earlier claim did)
    }
    if (auto selected = gAudio->selectSource(kEsp32Input, false); !selected) {
        ESP_LOGW(kTag, "could not select the ESP32 input");
        return false;
    }
    gPrevious = current;
    ESP_LOGI(kTag, "ESP32 stream input selected");
    return true;
}

void restorePreviousSource()
{
    if (gAudio == nullptr || !gPrevious) {
        return;
    }
    // Only undo our own switch: if the user picked another source while
    // the stream played, leave their choice alone.
    if (gAudio->currentProfile().activeSource == kEsp32Input) {
        if (auto selected = gAudio->selectSource(*gPrevious, false); !selected) {
            ESP_LOGW(kTag, "could not restore the previous source");
        }
    }
    gPrevious.reset();
}

} // namespace audio_routing
