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

#include "board_pins.hpp"
#include "webradio/WebRadioService.hpp"

#include "driver/i2s_std.h"
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

/** Raw MP3 bytes pending decode, refilled from the HTTP socket. */
struct InputBuffer {
    std::uint8_t data[kInputBufSize];
    std::size_t filled = 0U;
    std::uint8_t* readPtr = data;
};

[[nodiscard]] i2s_chan_handle_t openTxChannel()
{
    i2s_chan_config_t chanCfg =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_SLAVE);
    i2s_chan_handle_t txHandle = nullptr;
    if (i2s_new_channel(&chanCfg, &txHandle, nullptr) != ESP_OK) {
        ESP_LOGE(kTag, "i2s_new_channel failed");
        return nullptr;
    }

    i2s_std_config_t stdCfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(kSampleRateHz),
        // 32-bit slots match ADAU1701 SerialOutRegister1 (64 BCLKs/frame).
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = static_cast<gpio_num_t>(board::pins::I2sBclk),
            .ws = static_cast<gpio_num_t>(board::pins::I2sLrclk),
            .dout = static_cast<gpio_num_t>(board::pins::I2sDataOut),
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    if (i2s_channel_init_std_mode(txHandle, &stdCfg) != ESP_OK
        || i2s_channel_enable(txHandle) != ESP_OK) {
        ESP_LOGE(kTag, "I2S TX channel init/enable failed");
        i2s_del_channel(txHandle);
        return nullptr;
    }
    ESP_LOGI(kTag, "I2S slave TX started (BCLK=%d WS=%d DOUT=%d)",
             board::pins::I2sBclk, board::pins::I2sLrclk,
             board::pins::I2sDataOut);
    return txHandle;
}

[[nodiscard]] esp_http_client_handle_t openStream(const std::string& url)
{
    esp_http_client_config_t cfg{};
    cfg.url = url.c_str();
    cfg.timeout_ms = kHttpTimeoutMs;
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

/** Convert one decoded PCM frame to the ADAU's 32-bit-slot I2S format. */
void writeFrame(i2s_chan_handle_t tx, const std::int16_t* pcm,
                int frameCount, int channels)
{
    std::int32_t out[2];
    for (int i = 0; i < frameCount; ++i) {
        const std::int16_t left = pcm[i * channels];
        const std::int16_t right = channels > 1 ? pcm[i * channels + 1] : left;
        out[0] = static_cast<std::int32_t>(left) << 16;
        out[1] = static_cast<std::int32_t>(right) << 16;
        std::size_t written = 0U;
        (void)i2s_channel_write(tx, out, sizeof(out), &written,
                                portMAX_DELAY);
    }
}

/**
 * One refill+decode+play cycle.
 * @return false when the stream has ended (caller should reconnect).
 */
[[nodiscard]] bool pumpOneFrame(esp_http_client_handle_t client,
                                HMP3Decoder decoder, i2s_chan_handle_t tx,
                                InputBuffer& in, std::int16_t* pcmOut,
                                bool& loggedFormat)
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
    writeFrame(tx, pcmOut, frameCount, info.nChans);
    return true;
}

/** Stream until the config is disabled or the connection drops. */
void streamWhileEnabled(webradio::WebRadioService& service,
                        const std::string& url, HMP3Decoder decoder,
                        i2s_chan_handle_t tx, std::int16_t* pcmOut)
{
    esp_http_client_handle_t client = openStream(url);
    if (client == nullptr) {
        vTaskDelay(kReconnectDelay);
        return;
    }

    InputBuffer in;
    bool loggedFormat = false;
    while (service.config().enabled
           && pumpOneFrame(client, decoder, tx, in, pcmOut, loggedFormat)) {
        // keep pumping until disabled, the stream ends, or it drops
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}

} // namespace

void run(void* arg)
{
    auto* service = static_cast<webradio::WebRadioService*>(arg);

    i2s_chan_handle_t tx = openTxChannel();
    if (tx == nullptr) {
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
        streamWhileEnabled(*service, cfg.url, decoder, tx, pcmOut);
    }
}

} // namespace web_radio_stream
