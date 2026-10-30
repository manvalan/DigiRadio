/**
 * @file    RdsMetadataAccumulator.cpp
 * @brief   RdsMetadataAccumulator implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/RdsMetadataAccumulator.hpp"

#include "core/EbuLatin.hpp"

#include <algorithm>

namespace core {

namespace {

/** AN649 FM_RDS_STATUS BLEx: 0 none, 1 = 1–2 corrected, 2 = 3–5
 *  corrected, 3 = uncorrectable. A 3–5 bit correction is often a
 *  miscorrection, so only 0 and 1 are trusted for content. */
constexpr std::uint8_t kTrustedErrors = 1U;
constexpr std::uint8_t kUncorrectable = 3U;
/** Group 3A as the 5-bit type+version code (type 3, version A). */
constexpr std::uint8_t kGroup3A = 0x06U;
/** RadioText Plus ODA application id. */
constexpr std::uint16_t kAidRtPlus = 0x4BD7U;
/** RT+ content types (same numbering as DL Plus). */
constexpr std::uint8_t kRtPlusTitle = 1U;
constexpr std::uint8_t kRtPlusArtist = 4U;
constexpr char kRtEndMarker = '\r';

[[nodiscard]] std::uint8_t errorsOf(std::uint8_t blockErrors,
                                    unsigned shift) noexcept
{
    return static_cast<std::uint8_t>((blockErrors >> shift) & 0x03U);
}

[[nodiscard]] bool trusted(std::uint8_t blockErrors, unsigned shift) noexcept
{
    return errorsOf(blockErrors, shift) <= kTrustedErrors;
}

/** A corrupted block decodes to control codes far more often than to a
 *  wrong letter, so a pair holding one is not taken at all. */
[[nodiscard]] bool plausible(char c) noexcept
{
    const auto byte = static_cast<unsigned char>(c);
    return byte >= 0x20U && byte != 0x7FU;
}

[[nodiscard]] char highByte(std::uint16_t block) noexcept
{
    return static_cast<char>((block >> 8U) & 0xFFU);
}

[[nodiscard]] char lowByte(std::uint16_t block) noexcept
{
    return static_cast<char>(block & 0xFFU);
}

} // namespace

void RdsMetadataAccumulator::reset() noexcept
{
    psCandidate_.fill('\0');
    psCandidateMask_ = 0U;
    psBuffer_.fill('\0');
    psConfirmedMask_ = 0U;
    psPublished_.reset();
    rtBuffer_.fill('\0');
    rtSegmentValid_.fill(false);
    rtAbFlag_ = false;
    rtVersionB_ = false;
    rtAbInitialized_ = false;
    rtPublished_.reset();
    rtPublishedCurrent_ = false;
    rtPlusGroup_.reset();
    rtPlusValid_ = false;
    rtPlusRunning_ = false;
    afKhz_.fill(0U);
    afCount_ = 0U;
    piCandidate_.reset();
    piConfirmed_.reset();
}

void RdsMetadataAccumulator::applyGroup(std::uint16_t blockA,
                                        std::uint16_t blockB,
                                        std::uint16_t blockC,
                                        std::uint16_t blockD,
                                        std::uint8_t blockErrors) noexcept
{
    if (errorsOf(blockErrors, 4U) >= kUncorrectable) {
        return; // group type and segment address unknown
    }
    if (blockA != 0U && trusted(blockErrors, 6U)) {
        if (piCandidate_ == blockA) {
            piConfirmed_ = blockA;
        }
        piCandidate_ = blockA;
    }
    const auto groupType = static_cast<std::uint8_t>((blockB >> 12U) & 0x0FU);
    const auto groupCode = static_cast<std::uint8_t>((blockB >> 11U) & 0x1FU);
    if (rtPlusGroup_ && groupCode == *rtPlusGroup_) {
        applyRtPlus(blockB, blockC, blockD, blockErrors);
    } else if (groupCode == kGroup3A) {
        applyOdaAnnouncement(blockB, blockD, blockErrors);
    } else if (groupType == 0U) {
        applyProgramService(blockB, blockC, blockD, blockErrors);
    } else if (groupType == 2U) {
        applyRadiotext(blockB, blockC, blockD, blockErrors);
    }
}

void RdsMetadataAccumulator::applyProgramService(
    std::uint16_t blockB, std::uint16_t blockC, std::uint16_t blockD,
    std::uint8_t blockErrors) noexcept
{
    // Group type 0 (both 0A and 0B): the two Program Service name
    // characters are always in Block D. Block C differs by version --
    // 0A carries alternate-frequency codes, 0B repeats the PI code --
    // but never PS text either way (ETSI EN 62106 §3.1.5).
    const bool segmentTrusted = trusted(blockErrors, 4U);
    if (segmentTrusted && trusted(blockErrors, 0U)) {
        feedProgramService(static_cast<std::size_t>(blockB & 0x03U),
                           highByte(blockD), lowByte(blockD));
    }
    const bool versionA = ((blockB >> 11U) & 0x01U) == 0U;
    if (versionA && trusted(blockErrors, 2U)) {
        ingestAfCode(static_cast<std::uint8_t>((blockC >> 8U) & 0xFFU));
        ingestAfCode(static_cast<std::uint8_t>(blockC & 0xFFU));
    }
}

void RdsMetadataAccumulator::feedProgramService(std::size_t segment,
                                                char first,
                                                char second) noexcept
{
    if (!plausible(first) || !plausible(second)) {
        return;
    }
    const auto bit = static_cast<std::uint8_t>(1U << segment);
    const std::size_t at = segment * 2U;
    const bool known = (psCandidateMask_ & bit) != 0U;
    if (!known || psCandidate_[at] != first || psCandidate_[at + 1U] != second) {
        // Something else in this place than last time: the station moved
        // on to its next message (scrolling PS) or the group was noise.
        // Either way nothing confirmed so far can be trusted to belong
        // with what comes next, so start the name over.
        if (known) {
            psConfirmedMask_ = 0U;
        }
        psCandidate_[at] = first;
        psCandidate_[at + 1U] = second;
        psCandidateMask_ |= bit;
        return;
    }
    psBuffer_[at] = first;
    psBuffer_[at + 1U] = second;
    psConfirmedMask_ |= bit;
    const bool blank = std::all_of(psBuffer_.begin(), psBuffer_.end(),
                                   [](char c) { return c == ' '; });
    if (psConfirmedMask_ == 0x0FU && !blank) {
        psPublished_ = psBuffer_;
    }
}

void RdsMetadataAccumulator::applyRadiotext(std::uint16_t blockB,
                                            std::uint16_t blockC,
                                            std::uint16_t blockD,
                                            std::uint8_t blockErrors) noexcept
{
    const bool versionB = ((blockB >> 11U) & 0x01U) != 0U;
    const bool abFlag = (blockB & 0x10U) != 0U;
    if (!rtAbInitialized_ || abFlag != rtAbFlag_ || versionB != rtVersionB_) {
        startRadiotext(abFlag, versionB);
    }
    const bool cOk = versionB || trusted(blockErrors, 2U);
    if (!trusted(blockErrors, 4U) || !cOk || !trusted(blockErrors, 0U)) {
        return;
    }
    // 2A: four characters per segment in C and D; 2B: two in D only (C
    // repeats the PI), so a 2B text is at most 32 characters.
    const std::array<char, 4> chars = {highByte(blockC), lowByte(blockC),
                                       highByte(blockD), lowByte(blockD)};
    const std::size_t width = versionB ? 2U : 4U;
    const std::size_t from = versionB ? 2U : 0U;
    const std::size_t index = static_cast<std::size_t>(blockB & 0x0FU);
    for (std::size_t i = 0U; i < width; ++i) {
        rtBuffer_[index * width + i] = chars[from + i];
    }
    rtSegmentValid_[index] = true;
    publishRadiotextIfComplete();
}

void RdsMetadataAccumulator::startRadiotext(bool abFlag,
                                            bool versionB) noexcept
{
    rtBuffer_.fill('\0');
    rtSegmentValid_.fill(false);
    rtAbFlag_ = abFlag;
    rtVersionB_ = versionB;
    rtAbInitialized_ = true;
    rtPublishedCurrent_ = false;
    rtPlusValid_ = false; // tags describe the text being replaced
}

void RdsMetadataAccumulator::applyOdaAnnouncement(
    std::uint16_t blockB, std::uint16_t blockD,
    std::uint8_t blockErrors) noexcept
{
    // Group 3A (EN 62106 3.1.5.4): block B[4:0] = group type code of the
    // ODA (type + version), block D = its AID. RT+ is AID 0x4BD7.
    if (trusted(blockErrors, 4U) && trusted(blockErrors, 0U)
        && blockD == kAidRtPlus) {
        rtPlusGroup_ = static_cast<std::uint8_t>(blockB & 0x1FU);
    }
}

void RdsMetadataAccumulator::applyRtPlus(std::uint16_t blockB,
                                         std::uint16_t blockC,
                                         std::uint16_t blockD,
                                         std::uint8_t blockErrors) noexcept
{
    if (!trusted(blockErrors, 4U) || !trusted(blockErrors, 2U)
        || !trusted(blockErrors, 0U)) {
        return;
    }
    // RT+ group (IEC 62106-6 / RT+ spec): B[4] item toggle, B[3] item
    // running, then two tags: content type 6 bits, start 6 bits, length
    // marker 6 (tag 1) / 5 (tag 2) bits = length - 1, packed over B[2:0],
    // C and D.
    rtPlusRunning_ = (blockB & 0x08U) != 0U;
    rtPlusTags_[0] = RtPlusTag{
        .contentType = static_cast<std::uint8_t>(((blockB & 0x07U) << 3U)
                                                 | (blockC >> 13U)),
        .start = static_cast<std::uint8_t>((blockC >> 7U) & 0x3FU),
        .length = static_cast<std::uint8_t>(((blockC >> 1U) & 0x3FU) + 1U)};
    rtPlusTags_[1] = RtPlusTag{
        .contentType = static_cast<std::uint8_t>(((blockC & 0x01U) << 5U)
                                                 | (blockD >> 11U)),
        .start = static_cast<std::uint8_t>((blockD >> 5U) & 0x3FU),
        .length = static_cast<std::uint8_t>((blockD & 0x1FU) + 1U)};
    rtPlusValid_ = true;
}

DlPlusItem RdsMetadataAccumulator::rtPlus() const
{
    if (!rtPlusValid_ || !rtPlusRunning_ || !rtPublishedCurrent_) {
        return {};
    }
    return DlPlusItem{.title = rtPlusTagged(kRtPlusTitle),
                      .artist = rtPlusTagged(kRtPlusArtist)};
}

std::optional<BroadcastLabel> RdsMetadataAccumulator::rtPlusTagged(
    std::uint8_t contentType) const
{
    for (const RtPlusTag& tag : rtPlusTags_) {
        if (tag.contentType != contentType
            || tag.start >= rtPublished_->size()) {
            continue;
        }
        return BroadcastLabel::tryFromChipBytes(ebuLatinToUtf8(
            std::string_view(*rtPublished_).substr(tag.start, tag.length)));
    }
    return std::nullopt;
}

void RdsMetadataAccumulator::publishRadiotextIfComplete() noexcept
{
    const std::size_t width = rtVersionB_ ? 2U : 4U;
    for (std::size_t segment = 0U; segment < kRtSegments; ++segment) {
        if (!rtSegmentValid_[segment]) {
            return; // a gap before any end marker: not complete yet
        }
        const auto begin = rtBuffer_.begin() + segment * width;
        const auto marker = std::find(begin, begin + width, kRtEndMarker);
        if (marker != begin + width) {
            rtPublished_ = std::string(rtBuffer_.begin(), marker);
            rtPublishedCurrent_ = true;
            return;
        }
    }
    rtPublished_ = std::string(rtBuffer_.begin(),
                               rtBuffer_.begin() + kRtSegments * width);
    rtPublishedCurrent_ = true;
}

std::optional<BroadcastLabel> RdsMetadataAccumulator::programName() const
{
    if (!psPublished_) {
        return std::nullopt;
    }
    return BroadcastLabel::tryFromChipBytes(ebuLatinToUtf8(
        std::string_view(psPublished_->data(), psPublished_->size())));
}

std::optional<BroadcastLabel> RdsMetadataAccumulator::radiotext() const
{
    if (!rtPublished_) {
        return std::nullopt;
    }
    return BroadcastLabel::tryFromChipBytes(ebuLatinToUtf8(*rtPublished_));
}

void RdsMetadataAccumulator::ingestAfCode(std::uint8_t code) noexcept
{
    // IEC 62106: codes 1–204 are 87.6–107.9 MHz in 100 kHz steps.
    if (code < 1U || code > 204U) {
        return;
    }
    const std::uint32_t khz = 87500U + (static_cast<std::uint32_t>(code) * 100U);
    for (std::uint8_t i = 0; i < afCount_; ++i) {
        if (afKhz_[i] == khz) {
            return;
        }
    }
    if (afCount_ >= kMaxAf) {
        return;
    }
    afKhz_[afCount_] = khz;
    ++afCount_;
}

std::span<const std::uint32_t> RdsMetadataAccumulator::alternateFrequenciesKhz() const
{
    return std::span<const std::uint32_t>(afKhz_.data(), afCount_);
}

std::optional<std::uint16_t> RdsMetadataAccumulator::programIdentification() const
{
    return piConfirmed_;
}

} // namespace core
