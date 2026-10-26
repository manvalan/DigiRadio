/**
 * @file    web_radio_stream.cpp
 * @brief   Web radio player task: ring buffer -> libhelix MP3 decode ->
 *          resample to 48 kHz -> ADAU1701 I2S.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * 0.10.0 rewrite. Until then one task read 4 KB from the socket, decoded
 * a frame and wrote it to I2S without converting the sample rate: the
 * ADAU1701 clocks 48 kHz, nearly every MP3 station sends 44.1 kHz, so the
 * audio played 9 % fast and ran the socket dry every few frames.
 */

#include "web_radio_stream.hpp"

#include "audio_routing.hpp"
#include "esp32_i2s_sink.hpp"
#include "web_radio_session.hpp"
#include "webradio/WebRadioService.hpp"

#include "core/PcmResampler.hpp"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mp3dec.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <span>

namespace web_radio_stream {

namespace {

constexpr char kTag[] = "web_radio";
/** About 2.5 s at 128 kbit/s. Internal RAM (no PSRAM on this module), so
 *  it exists only while streaming is on; smaller sizes are tried when the
 *  heap is fragmented. */
constexpr std::array<std::size_t, 3> kRingSizes{40U * 1024U, 24U * 1024U,
                                                16U * 1024U};
/** Contiguous heap a TLS connection needs once the ring is taken. */
constexpr std::size_t kTlsReserveBytes = 18U * 1024U;
constexpr unsigned kStartFillPercent = 50U;
/** Low-bitrate streams fill slowly: start anyway after this long. */
constexpr std::int64_t kPrefillTimeoutUs = 6000000;
constexpr std::size_t kMinStartBytes = 4096U;
constexpr std::size_t kInputBufSize = 4096U; // >= 2x MAINBUF_SIZE (1940)
constexpr std::size_t kResampleBlockFrames = 384U;
constexpr std::size_t kResampleOutFrames = 2400U; // 384 frames at 8 kHz
constexpr std::size_t kI2sChunkFrames = 480U;
constexpr TickType_t kIdlePollDelay = pdMS_TO_TICKS(500);
constexpr TickType_t kSinkBusyRetryDelay = pdMS_TO_TICKS(1000);
constexpr TickType_t kInitRetryDelay = pdMS_TO_TICKS(5000);
constexpr TickType_t kRingReadTimeout = pdMS_TO_TICKS(100);
constexpr std::int64_t kPublishPeriodUs = 500000;
constexpr int kTrimEveryFrames = 32;

/** What one decode attempt achieved. */
enum class Decoded { Played, NeedData, Skipped, OutputFailed };

/** Raw MP3 bytes pending decode, refilled from the ring buffer. */
struct InputBuffer {
    std::array<std::uint8_t, kInputBufSize> data{};
    std::size_t filled = 0U;
    std::size_t read = 0U;

    [[nodiscard]] std::size_t unread() const { return filled - read; }
};

/** Player-side counters merged into the published status. */
struct PlayerStats {
    core::WebRadioState state{core::WebRadioState::Buffering};
    core::WebRadioFault fault{core::WebRadioFault::None};
    std::uint32_t sampleRateHz{0U};
    std::uint8_t channels{0U};
    std::uint16_t bitrateKbps{0U};
    std::uint32_t underruns{0U};
    std::int64_t lastPublishUs{0};
};

struct Player {
    webradio::WebRadioService* service{nullptr};
    HMP3Decoder decoder{nullptr};
    Session session;
    InputBuffer in;
    PlayerStats stats;
    core::PcmResampler resampler;
    int framesSinceTrim{0};
};

[[nodiscard]] unsigned fillPercent(const Session& s)
{
    const std::size_t used = xStreamBufferBytesAvailable(s.ring);
    return static_cast<unsigned>(used * 100U / s.ringBytes);
}

void publish(Player& p, bool force = false)
{
    const std::int64_t now = esp_timer_get_time();
    if (!force && now - p.stats.lastPublishUs < kPublishPeriodUs) {
        return;
    }
    p.stats.lastPublishUs = now;
    core::WebRadioStatus status;
    Session& s = p.session;
    xSemaphoreTake(s.lock, portMAX_DELAY);
    const bool netOk = s.netState == core::WebRadioState::Playing;
    status.state = netOk ? p.stats.state : s.netState;
    status.fault = netOk ? p.stats.fault : s.fault;
    status.httpStatus = s.httpStatus;
    status.codec = s.codec;
    status.reconnects = s.reconnects;
    status.title = s.title;
    xSemaphoreGive(s.lock);
    status.sampleRateHz = p.stats.sampleRateHz;
    status.channels = p.stats.channels;
    status.bitrateKbps = p.stats.bitrateKbps;
    status.bufferPercent = static_cast<std::uint8_t>(fillPercent(s));
    status.underruns = p.stats.underruns;
    p.service->publishStatus(status);
}

[[nodiscard]] bool sessionCurrent(const Player& p, std::uint32_t generation)
{
    return p.service->generation() == generation
           && p.service->config().enabled;
}

/** Waits until the ring is half full (or has enough after a timeout). */
[[nodiscard]] bool waitForFill(Player& p, std::uint32_t generation)
{
    p.stats.state = core::WebRadioState::Buffering;
    const std::int64_t start = esp_timer_get_time();
    while (sessionCurrent(p, generation)) {
        const std::size_t used = xStreamBufferBytesAvailable(p.session.ring);
        const bool timedOut = esp_timer_get_time() - start > kPrefillTimeoutUs;
        if (fillPercent(p.session) >= kStartFillPercent
            || (timedOut && used >= kMinStartBytes)) {
            p.stats.state = core::WebRadioState::Playing;
            return true;
        }
        publish(p);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return false;
}

/** Slides unread bytes to the front, then tops up from the ring.
 *  @return bytes received from the ring. */
std::size_t refill(Player& p)
{
    InputBuffer& in = p.in;
    const std::size_t unread = in.unread();
    std::memmove(in.data.data(), in.data.data() + in.read, unread);
    in.filled = unread;
    in.read = 0U;
    const std::size_t got =
        xStreamBufferReceive(p.session.ring, in.data.data() + unread,
                             in.data.size() - unread, kRingReadTimeout);
    in.filled += got;
    return got;
}

/** Converts 16-bit stereo to the ADAU's 32-bit slots and writes it. */
[[nodiscard]] bool writeI2s(std::span<const std::int16_t> stereo)
{
    static std::array<std::int32_t, kI2sChunkFrames * 2U> out;
    const std::size_t frames = stereo.size() / 2U;
    for (std::size_t done = 0U; done < frames; done += kI2sChunkFrames) {
        const std::size_t batch = std::min(kI2sChunkFrames, frames - done);
        for (std::size_t i = 0U; i < batch * 2U; ++i) {
            out[i] = static_cast<std::int32_t>(stereo[done * 2U + i]) << 16;
        }
        if (!esp32_i2s_sink::writeSamples(out.data(), batch * 2U)) {
            return false;
        }
    }
    return true;
}

void steerDrift(Player& p)
{
    if (++p.framesSinceTrim < kTrimEveryFrames) {
        return;
    }
    p.framesSinceTrim = 0;
    p.resampler.setTrimPpm(core::driftTrimPpm(fillPercent(p.session)));
}

/** Resamples one decoded frame (interleaved stereo) and plays it. */
[[nodiscard]] bool playFrame(Player& p, std::span<const std::int16_t> stereo)
{
    static std::array<std::int16_t, kResampleOutFrames * 2U> out;
    steerDrift(p);
    for (std::size_t at = 0U; at < stereo.size(); at += kResampleBlockFrames * 2U) {
        const std::size_t length =
            std::min(kResampleBlockFrames * 2U, stereo.size() - at);
        const std::size_t frames =
            p.resampler.process(stereo.subspan(at, length), out);
        if (!writeI2s(std::span<const std::int16_t>(out.data(), frames * 2U))) {
            return false;
        }
    }
    return true;
}

void noteFormat(Player& p, const MP3FrameInfo& info)
{
    p.stats.bitrateKbps = static_cast<std::uint16_t>(info.bitrate / 1000);
    p.stats.channels = static_cast<std::uint8_t>(info.nChans);
    if (p.resampler.inputRateHz() == static_cast<std::uint32_t>(info.samprate)) {
        return;
    }
    if (!p.resampler.setInputRate(static_cast<std::uint32_t>(info.samprate))) {
        ESP_LOGW(kTag, "unsupported MP3 sample rate %d Hz", info.samprate);
        return;
    }
    p.stats.sampleRateHz = static_cast<std::uint32_t>(info.samprate);
    ESP_LOGI(kTag, "stream format: %d Hz, %d ch, %d kbit/s -> 48000 Hz",
             info.samprate, info.nChans, info.bitrate / 1000);
}

/** Decodes one frame from the input buffer and plays it. */
[[nodiscard]] Decoded decodeAndPlay(Player& p)
{
    static std::array<std::int16_t, MAX_NCHAN * MAX_NGRAN * MAX_NSAMP> pcm;
    static std::array<std::int16_t, 2U * MAX_NGRAN * MAX_NSAMP> stereo;
    InputBuffer& in = p.in;
    const int offset = MP3FindSyncWord(in.data.data() + in.read,
                                       static_cast<int>(in.unread()));
    if (offset < 0) {
        in.read = in.filled; // no sync word in what we have: drop it
        return Decoded::Skipped;
    }
    in.read += static_cast<std::size_t>(offset);
    unsigned char* ptr = in.data.data() + in.read;
    int left = static_cast<int>(in.unread());
    const int err = MP3Decode(p.decoder, &ptr, &left, pcm.data(), 0);
    in.read = static_cast<std::size_t>(ptr - in.data.data());
    if (err == ERR_MP3_INDATA_UNDERFLOW) {
        return Decoded::NeedData;
    }
    if (err != ERR_MP3_NONE) {
        if (err != ERR_MP3_MAINDATA_UNDERFLOW
            && left == static_cast<int>(in.unread())) {
            ++in.read; // corrupt header: skip past this sync word
        }
        return Decoded::Skipped;
    }
    MP3FrameInfo info{};
    MP3GetLastFrameInfo(p.decoder, &info);
    noteFormat(p, info);
    if (p.resampler.inputRateHz() == 0U || info.nChans < 1) {
        return Decoded::Skipped;
    }
    const auto frames = static_cast<std::size_t>(info.outputSamps / info.nChans);
    for (std::size_t i = 0U; i < frames; ++i) {
        stereo[i * 2U] = pcm[i * info.nChans];
        stereo[i * 2U + 1U] = pcm[i * info.nChans + (info.nChans - 1)];
    }
    const auto out = std::span<const std::int16_t>(stereo.data(), frames * 2U);
    return playFrame(p, out) ? Decoded::Played : Decoded::OutputFailed;
}

[[nodiscard]] bool rebuffer(Player& p, std::uint32_t generation)
{
    ++p.stats.underruns;
    ESP_LOGW(kTag, "buffer empty (underrun %lu), rebuffering",
             static_cast<unsigned long>(p.stats.underruns));
    return waitForFill(p, generation);
}

void reportOutputFailure(Player& p)
{
    p.stats.fault = core::WebRadioFault::AudioOutput;
    p.stats.state = core::WebRadioState::Error;
    ESP_LOGE(kTag, "I2S write timed out: no BCLK/LRCLK from the ADAU1701?");
    publish(p, true);
    vTaskDelay(pdMS_TO_TICKS(1000));
}

/** One step of the play loop; false ends the session. */
[[nodiscard]] bool step(Player& p, std::uint32_t generation)
{
    const std::size_t got = p.in.unread() < MAINBUF_SIZE ? refill(p) : 1U;
    const Decoded result =
        p.in.unread() == 0U ? Decoded::NeedData : decodeAndPlay(p);
    if (result == Decoded::NeedData && got == 0U) {
        return rebuffer(p, generation);
    }
    if (result == Decoded::OutputFailed) {
        reportOutputFailure(p);
    } else if (result == Decoded::Played) {
        p.stats.fault = core::WebRadioFault::None;
        p.stats.state = core::WebRadioState::Playing;
    }
    publish(p);
    return sessionCurrent(p, generation);
}

void stopFetch(Player& p)
{
    p.session.stop.store(true);
    // The network task notices within one socket read (8 s timeout at
    // worst, usually immediately on a live stream).
    if (xSemaphoreTake(p.session.done, pdMS_TO_TICKS(15000)) != pdTRUE) {
        ESP_LOGE(kTag, "network task did not stop; waiting");
        xSemaphoreTake(p.session.done, portMAX_DELAY);
    }
}

[[nodiscard]] bool prepareSession(Player& p, const std::string& url)
{
    Session& s = p.session;
    xStreamBufferReset(s.ring);
    s.url = url;
    s.stop.store(false);
    s.netState = core::WebRadioState::Connecting;
    s.fault = core::WebRadioFault::None;
    s.httpStatus = 0;
    s.codec.clear();
    s.reconnects = 0U;
    s.title.reset();
    p.in = InputBuffer{};
    p.stats = PlayerStats{};
    p.framesSinceTrim = 0;
    if (p.decoder != nullptr) {
        MP3FreeDecoder(p.decoder);
    }
    p.decoder = MP3InitDecoder();
    return p.decoder != nullptr && startFetch(s);
}

void playSession(Player& p, const std::string& url, std::uint32_t generation)
{
    if (!prepareSession(p, url)) {
        p.stats.state = core::WebRadioState::Error;
        p.stats.fault = core::WebRadioFault::OutOfMemory;
        publish(p, true);
        vTaskDelay(pdMS_TO_TICKS(5000));
        return;
    }
    ESP_LOGI(kTag, "playing %s", url.c_str());
    if (waitForFill(p, generation)) {
        while (step(p, generation)) {
        }
    }
    stopFetch(p);
}

[[nodiscard]] bool createRing(Session& s)
{
    // HTTPS still needs one contiguous ~17 KB block (mbedTLS input record
    // buffer) after the ring: skip sizes that would leave less.
    const std::size_t largest =
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    for (const std::size_t bytes : kRingSizes) {
        if (bytes != kRingSizes.back() && bytes + kTlsReserveBytes > largest) {
            continue;
        }
        s.ring = xStreamBufferCreate(bytes, 1U);
        if (s.ring != nullptr) {
            s.ringBytes = bytes;
            ESP_LOGI(kTag, "ring buffer %u bytes", static_cast<unsigned>(bytes));
            return true;
        }
    }
    ESP_LOGE(kTag, "no heap for the ring buffer (largest block %u)",
             static_cast<unsigned>(
                 heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    return false;
}

void deleteRing(Session& s)
{
    vStreamBufferDelete(s.ring);
    s.ring = nullptr;
    s.ringBytes = 0U;
}

/** I2S channel and session semaphores; retried, never fatal: the task
 *  must stay alive because system_diag keeps its handle. */
[[nodiscard]] bool initPlayer(Player& p)
{
    if (p.session.done == nullptr) {
        p.session.done = xSemaphoreCreateBinary();
    }
    if (p.session.lock == nullptr) {
        p.session.lock = xSemaphoreCreateMutex();
    }
    return esp32_i2s_sink::open() && p.session.done != nullptr
           && p.session.lock != nullptr;
}

void publishFault(Player& p, core::WebRadioFault fault)
{
    core::WebRadioStatus status;
    status.state = core::WebRadioState::Error;
    status.fault = fault;
    p.service->publishStatus(status);
}

void publishIdle(Player& p)
{
    if (p.service->status().state == core::WebRadioState::Idle) {
        return;
    }
    p.service->publishStatus(core::WebRadioStatus{});
}

/** Holds the sink and the ESP32 input for as long as streaming stays on. */
void streamWhileEnabled(Player& p)
{
    if (!esp32_i2s_sink::tryAcquire()) {
        vTaskDelay(kSinkBusyRetryDelay); // the phone stream has the sink
        return;
    }
    if (!createRing(p.session)) {
        esp32_i2s_sink::release();
        publishFault(p, core::WebRadioFault::OutOfMemory);
        vTaskDelay(kInitRetryDelay);
        return;
    }
    if (!audio_routing::claimEsp32Input()) {
        ESP_LOGW(kTag, "ADAU source switch failed; stream may be inaudible");
    }
    while (p.service->config().enabled) {
        const std::uint32_t generation = p.service->generation();
        playSession(p, p.service->config().url, generation);
    }
    audio_routing::restorePreviousSource();
    esp32_i2s_sink::release();
    deleteRing(p.session);
    if (p.decoder != nullptr) {
        MP3FreeDecoder(p.decoder); // give the heap back while idle
        p.decoder = nullptr;
    }
    publishIdle(p);
}

} // namespace

void run(void* arg)
{
    static Player player;
    player.service = static_cast<webradio::WebRadioService*>(arg);
    while (!initPlayer(player)) {
        ESP_LOGE(kTag, "web radio unavailable (I2S or memory), retrying");
        publishFault(player, core::WebRadioFault::AudioOutput);
        vTaskDelay(kInitRetryDelay);
    }
    while (true) {
        if (player.service->config().enabled) {
            streamWhileEnabled(player);
        } else {
            publishIdle(player);
            vTaskDelay(kIdlePollDelay);
        }
    }
}

} // namespace web_radio_stream
