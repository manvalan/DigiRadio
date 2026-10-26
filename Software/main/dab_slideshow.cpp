/**
 * @file    dab_slideshow.cpp
 * @brief   DAB data pump task, flash slide store and slide HTTP API.
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
 * @date    2026-10-26
 */

#include "dab_slideshow.hpp"

#include "esp32_i2s_sink.hpp"

#include "core/IDabDataSink.hpp"
#include "core/ISlideWriter.hpp"
#include "core/MotSlideAssembler.hpp"
#include "net/SetupWebServer.hpp"
#include "tuner/TunerService.hpp"

#include "esp_flash.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_timer.h"

#include <atomic>
#include <bitset>
#include <mutex>
#include <optional>

namespace dab_slideshow {

namespace {

constexpr char kTag[] = "slideshow";

// Flash outside the partition table: after the Si4684 image store
// (0x860000 + 2 MB, si4684::Si4684ImageStore) on the 16 MB module.
constexpr std::uint32_t kFlashOffset = 0xA60000U;
constexpr std::uint32_t kSlotSize = 0x10000U; // 64 KB > the 50 KB SLS limit
constexpr std::uint32_t kSlotCount = 16U;
constexpr std::uint32_t kSectorSize = 0x1000U;
constexpr char kLabel[] = "slides";

constexpr std::size_t kPacketsPerPoll = 32U;
constexpr std::int64_t kInterestWindowUs = 5LL * 60LL * 1000000LL;

/**
 * @brief    FlashSlideStore — ISlideWriter over sixteen 64 KB flash slots.
 *
 * @dname    FlashSlideStore
 * @return   n/a (type)
 * @pubstate Owns the external "slides" partition handle and which slot is
 *           shown / being written. Sectors are erased lazily the first
 *           time a write touches them, so a slide erases only its size.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
class FlashSlideStore : public core::ISlideWriter {
public:
    [[nodiscard]] bool open()
    {
        return esp_partition_register_external(
                   esp_flash_default_chip, kFlashOffset, kSlotSize * kSlotCount,
                   kLabel, ESP_PARTITION_TYPE_DATA,
                   ESP_PARTITION_SUBTYPE_DATA_UNDEFINED, &partition_)
               == ESP_OK;
    }

    [[nodiscard]] std::uint32_t maxSlideSize() const noexcept override
    {
        return kSlotSize;
    }

    [[nodiscard]] bool beginSlide(std::uint32_t size) override
    {
        const std::lock_guard lock(mutex_);
        // Round robin over every slot but the one on show, so neither a
        // carousel nor slides that never complete wear one slot out.
        writingSlot_ = (writingSlot_ + 1U) % kSlotCount;
        if (writingSlot_ == shownSlot_) {
            writingSlot_ = (writingSlot_ + 1U) % kSlotCount;
        }
        erased_.reset();
        return partition_ != nullptr && size <= kSlotSize;
    }

    [[nodiscard]] bool writeSlide(std::uint32_t offset,
                                  std::span<const std::uint8_t> bytes) override
    {
        if (partition_ == nullptr || offset + bytes.size() > kSlotSize
            || !eraseSectors(offset, bytes.size())) {
            return false;
        }
        return esp_partition_write(partition_, slotBase(writingSlot_) + offset,
                                   bytes.data(), bytes.size())
               == ESP_OK;
    }

    void commitSlide(const core::SlideInfo& info) override
    {
        const std::lock_guard lock(mutex_);
        shownSlot_ = writingSlot_;
        shown_ = info;
        ESP_LOGI(kTag, "slide %u: %u bytes in slot %u",
                 static_cast<unsigned>(info.id),
                 static_cast<unsigned>(info.size),
                 static_cast<unsigned>(shownSlot_));
    }

    void clearSlides() override
    {
        const std::lock_guard lock(mutex_);
        shown_.reset();
    }

    [[nodiscard]] std::optional<core::SlideInfo> current()
    {
        const std::lock_guard lock(mutex_);
        return shown_;
    }

    [[nodiscard]] std::size_t read(std::uint32_t offset,
                                   std::span<std::uint8_t> out)
    {
        const std::lock_guard lock(mutex_);
        if (!shown_ || offset >= shown_->size || partition_ == nullptr) {
            return 0U;
        }
        const std::size_t count =
            std::min<std::size_t>(out.size(), shown_->size - offset);
        return esp_partition_read(partition_, slotBase(shownSlot_) + offset,
                                  out.data(), count)
                       == ESP_OK
                   ? count
                   : 0U;
    }

private:
    [[nodiscard]] static std::uint32_t slotBase(std::uint32_t slot) noexcept
    {
        return slot * kSlotSize;
    }

    [[nodiscard]] bool eraseSectors(std::uint32_t offset, std::size_t length)
    {
        const std::uint32_t first = offset / kSectorSize;
        const std::uint32_t last =
            static_cast<std::uint32_t>(offset + length - 1U) / kSectorSize;
        for (std::uint32_t s = first; s <= last; ++s) {
            if (erased_[s]) {
                continue;
            }
            if (esp_partition_erase_range(partition_,
                                          slotBase(writingSlot_) + s * kSectorSize,
                                          kSectorSize)
                != ESP_OK) {
                return false;
            }
            erased_.set(s);
        }
        return true;
    }

    const esp_partition_t* partition_ = nullptr;
    std::mutex mutex_;
    std::uint32_t shownSlot_ = 0U;
    std::uint32_t writingSlot_ = 1U;
    std::bitset<kSlotSize / kSectorSize> erased_;
    std::optional<core::SlideInfo> shown_;
};

/**
 * @brief    InterestGatedSink — feeds MOT groups to the assembler only
 *           while a client watches.
 *
 * @dname    InterestGatedSink
 * @return   n/a (type)
 * @pubstate Owns the assembler; reads gLastInterestUs.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
class InterestGatedSink : public core::IDabDataSink {
public:
    explicit InterestGatedSink(core::ISlideWriter& writer)
        : assembler_(writer)
    {
    }

    void onMotDataGroup(std::uint32_t serviceId,
                        std::span<const std::uint8_t> group) override;

private:
    core::MotSlideAssembler assembler_;
};

FlashSlideStore gStore;
std::atomic<std::int64_t> gLastInterestUs{-kInterestWindowUs};
tuner::TunerService* gTuner = nullptr;

void InterestGatedSink::onMotDataGroup(std::uint32_t serviceId,
                                       std::span<const std::uint8_t> group)
{
    if (esp_timer_get_time() - gLastInterestUs.load() < kInterestWindowUs) {
        assembler_.apply(serviceId, group);
    }
}

std::optional<core::SlideInfo> currentSlide()
{
    return gStore.current();
}

std::size_t readSlide(std::uint32_t offset, std::span<std::uint8_t> out)
{
    return gStore.read(offset, out);
}

void touch()
{
    gLastInterestUs.store(esp_timer_get_time());
}

constexpr net::SlideApi kSlideApi{
    .current = currentSlide,
    .read = readSlide,
    .touch = touch,
};

} // namespace

bool start(tuner::TunerService& tuner)
{
    gTuner = &tuner;
    if (!gStore.open()) {
        ESP_LOGE(kTag, "cannot register slide flash at 0x%lx: no slideshow",
                 static_cast<unsigned long>(kFlashOffset));
        return false;
    }
    net::setSlideApi(&kSlideApi);
    return true;
}

void poll()
{
    static InterestGatedSink sink(gStore);
    // While the ESP32 feeds the speaker (web radio, phone) the DAB
    // programme is not heard, and the 8 KB DSRV frames would compete
    // with the stream's TLS buffers for the few large heap blocks.
    if (gTuner == nullptr || gTuner->sweepActive() || esp32_i2s_sink::inUse()) {
        return;
    }
    if (auto ok = gTuner->pollDabData(sink, kPacketsPerPoll); !ok) {
        ESP_LOGD(kTag, "DSRV poll failed (%d)", static_cast<int>(ok.error()));
    }
}

} // namespace dab_slideshow
