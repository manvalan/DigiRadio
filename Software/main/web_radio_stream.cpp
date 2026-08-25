/**
 * @file    web_radio_stream.cpp
 * @brief   HTTP MP3 stream -> libhelix decode -> ADAU1701 I2S.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 */

#include "web_radio_stream.hpp"

#include "esp32_i2s_sink.hpp"
#include "webradio/WebRadioService.hpp"

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mp3dec.h"

#include <cstdint>
#include <cstring>
#include <string>

namespace web_radio_stream {

namespace {

constexpr char kTag[] = "web_radio";
constexpr int kSampleRateHz = 48000;
constexpr std::size_t kInputBufSize = 4096U; // >= 2x MAINBUF_SIZE (1940)
constexpr int kHttpTimeoutMs = 10000;
constexpr TickType_t kIdlePollDelay = pdMS_TO_TICKS(1000);
constexpr TickType_t kReconnectDelay = pdMS_TO_TICKS(5000);
/** How often to retry when a phone PCM stream currently owns the I2S sink. */
constexpr TickType_t kSinkBusyRetryDelay = pdMS_TO_TICKS(2000);

/** Raw MP3 bytes pending decode, refilled from the HTTP socket. */
struct InputBuffer {
    std::uint8_t data[kInputBufSize];
    std::size_t filled = 0U;
    std::uint8_t* readPtr = data;
};

[[nodiscard]] esp_http_client_handle_t openStream(const std::string& url)
{
    esp_http_client_config_t cfg{};
    cfg.url = url.c_str();
    cfg.timeout_ms = kHttpTimeoutMs;
    // Most public internet radio streams are HTTPS-only today; esp_http_client
    // needs an explicit trust anchor for TLS verification or the handshake
    // fails outright. CONFIG_MBEDTLS_CERTIFICATE_BUNDLE is already enabled
    // (sdkconfig), so attach ESP-IDF's built-in CA bundle -- this is a no-op
    // for plain http:// URLs.
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == nullptr) {
        ESP_LOGE(kTag, "esp_http_client_init failed");
        return nullptr;
    }
    if (esp_http_client_open(client, 0) != ESP_OK) {
        ESP_LOGE(kTag, "esp_http_client_open failed: %s", url.c_str());
        esp_http_client_cleanup(client);
        return nullptr;
    }
    const int contentLength = esp_http_client_fetch_headers(client);
    ESP_LOGI(kTag, "stream opened: %s (content-length=%d, status=%d)",
             url.c_str(), contentLength,
             esp_http_client_get_status_code(client));

    char* contentType = nullptr;
    if (esp_http_client_get_header(client, "Content-Type", &contentType)
            == ESP_OK
        && contentType != nullptr
        && std::strncmp(contentType, "audio/", 6) != 0) {
        ESP_LOGW(kTag,
                 "Content-Type '%s' is not audio/* -- this URL is probably "
                 "a website, not a direct MP3 stream link. Find the actual "
                 "stream endpoint (often ends in .mp3, or is listed as a "
                 "\"listen live\"/shoutcast/icecast URL on the station's "
                 "site) and set that instead.",
                 contentType);
    }
    return client;
}

/** Slide unread bytes to the front, then top up from the HTTP socket. */
void refill(esp_http_client_handle_t client, InputBuffer& in)
{
    const std::size_t unread =
        in.filled - static_cast<std::size_t>(in.readPtr - in.data);
    std::memmove(in.data, in.readPtr, unread);
    const int freeSpace = static_cast<int>(kInputBufSize - unread);
    const int nRead = freeSpace > 0
        ? esp_http_client_read(client, reinterpret_cast<char*>(in.data)
                                            + unread,
                               freeSpace)
        : 0;
    in.filled = unread + (nRead > 0 ? static_cast<std::size_t>(nRead) : 0U);
    in.readPtr = in.data;
}

/** Convert one decoded PCM frame to the ADAU's 32-bit-slot I2S format and
 *  hand the whole frame to the shared sink in a single write. One
 *  i2s_channel_write() call per sample (the previous approach) meant up to
 *  1152 separate driver calls per MP3 frame, each with its own locking/DMA
 *  bookkeeping overhead — a likely source of the reported stutter/crackle,
 *  independent of network jitter. */
void writeFrame(const std::int16_t* pcm, int frameCount, int channels)
{
    // MAX_NGRAN(2) * MAX_NSAMP(576) = 1152 samples/channel, stereo => 2304.
    static std::int32_t out[MAX_NGRAN * MAX_NSAMP * 2];
    const int sampleCount = frameCount > MAX_NGRAN * MAX_NSAMP
                                 ? MAX_NGRAN * MAX_NSAMP
                                 : frameCount;
    for (int i = 0; i < sampleCount; ++i) {
        const std::int16_t left = pcm[i * channels];
        const std::int16_t right = channels > 1 ? pcm[i * channels + 1] : left;
        out[i * 2] = static_cast<std::int32_t>(left) << 16;
        out[i * 2 + 1] = static_cast<std::int32_t>(right) << 16;
    }
    (void)esp32_i2s_sink::writeSamples(out, static_cast<std::size_t>(sampleCount) * 2U);
}

/**
 * One refill+decode+play cycle.
 * @return false when the stream has ended (caller should reconnect).
 */
[[nodiscard]] bool pumpOneFrame(esp_http_client_handle_t client,
                                HMP3Decoder decoder, InputBuffer& in,
                                std::int16_t* pcmOut, bool& loggedFormat)
{
    const std::size_t unread =
        in.filled - static_cast<std::size_t>(in.readPtr - in.data);
    if (unread < MAINBUF_SIZE) {
        refill(client, in);
    }
    const std::size_t available =
        in.filled - static_cast<std::size_t>(in.readPtr - in.data);
    if (available == 0U) {
        return false;
    }

    const int offset =
        MP3FindSyncWord(in.readPtr, static_cast<int>(available));
    if (offset < 0) {
        in.readPtr += available; // no sync word in this chunk, drop it
        return true;
    }
    in.readPtr += offset;

    unsigned char* decodePtr = in.readPtr;
    int bytesLeft = static_cast<int>(available - static_cast<std::size_t>(offset));
    const int err =
        MP3Decode(decoder, &decodePtr, &bytesLeft, pcmOut, 0);
    in.readPtr = decodePtr;
    if (err != ERR_MP3_NONE) {
        if (err == ERR_MP3_MAINDATA_UNDERFLOW) {
            return true; // needs more bytes; try again next cycle
        }
        ESP_LOGW(kTag, "MP3 decode error %d, resyncing", err);
        return true;
    }

    MP3FrameInfo info{};
    MP3GetLastFrameInfo(decoder, &info);
    if (!loggedFormat) {
        loggedFormat = true;
        ESP_LOGI(kTag, "stream format: %d Hz, %d ch, %d bps%s", info.samprate,
                 info.nChans, info.bitsPerSample,
                 info.samprate != kSampleRateHz
                     ? " (WARNING: != 48000 Hz, no resampler -> pitch off)"
                     : "");
    }
    const int frameCount = info.outputSamps / info.nChans;
    writeFrame(pcmOut, frameCount, info.nChans);
    return true;
}

/** Stream until the config is disabled or the connection drops. */
void streamWhileEnabled(webradio::WebRadioService& service,
                        const std::string& url, HMP3Decoder decoder,
                        std::int16_t* pcmOut)
{
    esp_http_client_handle_t client = openStream(url);
    if (client == nullptr) {
        vTaskDelay(kReconnectDelay);
        return;
    }

    InputBuffer in;
    bool loggedFormat = false;
    while (service.config().enabled
           && pumpOneFrame(client, decoder, in, pcmOut, loggedFormat)) {
        // keep pumping until disabled, the stream ends, or it drops
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}

} // namespace

void run(void* arg)
{
    auto* service = static_cast<webradio::WebRadioService*>(arg);

    if (!esp32_i2s_sink::open()) {
        vTaskDelete(nullptr);
        return;
    }

    HMP3Decoder decoder = MP3InitDecoder();
    if (decoder == nullptr) {
        ESP_LOGE(kTag, "MP3InitDecoder failed");
        vTaskDelete(nullptr);
        return;
    }

    static std::int16_t pcmOut[MAX_NCHAN * MAX_NGRAN * MAX_NSAMP];

    while (true) {
        const core::WebRadioConfig cfg = service->config();
        if (!cfg.enabled) {
            vTaskDelay(kIdlePollDelay);
            continue;
        }
        if (!esp32_i2s_sink::tryAcquire()) {
            // A phone PCM stream currently owns the shared I2S sink.
            vTaskDelay(kSinkBusyRetryDelay);
            continue;
        }
        streamWhileEnabled(*service, cfg.url, decoder, pcmOut);
        esp32_i2s_sink::release();
    }
}

} // namespace web_radio_stream
