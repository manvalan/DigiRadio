/**
 * @file    RdsMetadataAccumulator.hpp
 * @brief   Accumulate FM RDS program name and radiotext from raw groups.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */
#pragma once

#include "core/BroadcastLabel.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace core {

/**
 * @brief    RdsMetadataAccumulator — stateful RDS PS/RT decoder (IEC 62106).
 *
 * @dname    RdsMetadataAccumulator
 * @return   n/a (type)
 * @pubstate Holds partial PS and RT buffers until complete groups arrive.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class RdsMetadataAccumulator {
public:
    /**
     * @brief    reset — clear accumulated PS and radiotext.
     *
     * @dname    reset
     * @pubstate clears internal buffers.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    void reset() noexcept;

    /**
     * @brief    applyGroup — ingest one RDS group (blocks A–D).
     *
     * @dname    applyGroup
     * @param    blockA  RDS block A (PI code; confirmed once two
     *                   consecutive groups agree).
     * @param    blockB  RDS block B (group type and segment address).
     * @param    blockC  RDS block C (RT text for group 2; AF codes for
     *                   group 0A; ignored for group 0B).
     * @param    blockD  RDS block D (PS text for group 0; RT text for
     *                   group 2, per ETSI EN 62106 §3.1.5/§3.1.5.3).
     * @param    blockErrors  AN649 FM_RDS_STATUS RESP11: BLEA[7:6],
     *                   BLEB[5:4], BLEC[3:2], BLED[1:0], each 0 = no
     *                   errors, 1 = 1–2 corrected, 2 = 3–5 corrected,
     *                   3 = uncorrectable. Text, PI and AF are only taken
     *                   from blocks at 0 or 1; a group whose block B is
     *                   uncorrectable is dropped. Default 0 = trusted.
     * @pubstate updates PS/RT buffers for group types 0 and 2, and the
     *           AF list for group 0A.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    void applyGroup(std::uint16_t blockA,
                    std::uint16_t blockB,
                    std::uint16_t blockC,
                    std::uint16_t blockD,
                    std::uint8_t blockErrors = 0U) noexcept;

    /**
     * @brief    programName — read the accumulated 8-character PS name.
     *
     * @dname    programName
     * @return   Trimmed PS label once all four segments were each received
     *           twice alike; the last such name stays while the next is
     *           assembled, and an all-blank PS never replaces it.
     * @pubstate reads psPublished_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::optional<BroadcastLabel> programName() const;

    /**
     * @brief    radiotext — read the accumulated 64-character RT string.
     *
     * @dname    radiotext
     * @return   Trimmed radiotext once complete (up to the 0x0D end
     *           marker, or all segments); the previous text stays while a
     *           new one (A/B flag flip) is assembled.
     * @pubstate reads rtPublished_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::optional<BroadcastLabel> radiotext() const;

    /**
     * @brief    alternateFrequenciesKhz — FM AF codes decoded from group 0A.
     *
     * @dname    alternateFrequenciesKhz
     * @return   Unique centre frequencies in kHz (87.6–107.9), excluding
     *           filler and LF/MF markers.
     */
    [[nodiscard]] std::span<const std::uint32_t> alternateFrequenciesKhz() const;

    /**
     * @brief    programIdentification — confirmed RDS PI code (block A).
     *
     * @dname    programIdentification
     * @return   PI code once two consecutive groups carried the same
     *           block A (guards against a single corrupted block), or
     *           nullopt before that.
     * @pubstate reads piConfirmed_.
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    [[nodiscard]] std::optional<std::uint16_t> programIdentification() const;

private:
    static constexpr std::size_t kPsSegments = 4U;
    static constexpr std::size_t kRtSegments = 16U;
    static constexpr std::size_t kMaxAf = 25U;

    void ingestAfCode(std::uint8_t code) noexcept;
    void applyProgramService(std::uint16_t blockB, std::uint16_t blockC,
                             std::uint16_t blockD,
                             std::uint8_t blockErrors) noexcept;
    void feedProgramService(std::size_t segment, char first,
                            char second) noexcept;
    void applyRadiotext(std::uint16_t blockB, std::uint16_t blockC,
                        std::uint16_t blockD,
                        std::uint8_t blockErrors) noexcept;
    void startRadiotext(bool abFlag, bool versionB) noexcept;
    void publishRadiotextIfComplete() noexcept;

    std::array<char, 8> psCandidate_{};
    std::uint8_t psCandidateMask_{0U};
    std::array<char, 8> psBuffer_{};
    std::uint8_t psConfirmedMask_{0U};
    std::optional<std::array<char, 8>> psPublished_;
    std::array<char, 64> rtBuffer_{};
    std::array<bool, kRtSegments> rtSegmentValid_{};
    bool rtAbFlag_{false};
    bool rtVersionB_{false};
    bool rtAbInitialized_{false};
    std::optional<std::string> rtPublished_;
    std::array<std::uint32_t, kMaxAf> afKhz_{};
    std::uint8_t afCount_{0};
    std::optional<std::uint16_t> piCandidate_;
    std::optional<std::uint16_t> piConfirmed_;
};

} // namespace core
