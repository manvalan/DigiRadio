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
     * @param    blockA  RDS block A (PI code, currently unused).
     * @param    blockB  RDS block B (group type and segment address).
     * @param    blockC  RDS block C (RT text for group 2 only; group 0's
     *                   AF list/repeated PI is not text and is ignored).
     * @param    blockD  RDS block D (PS text for group 0; RT text for
     *                   group 2, per ETSI EN 62106 §3.1.5/§3.1.5.3).
     * @pubstate updates PS/RT buffers for group types 0A and 2A.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    void applyGroup(std::uint16_t blockA,
                    std::uint16_t blockB,
                    std::uint16_t blockC,
                    std::uint16_t blockD) noexcept;

    /**
     * @brief    programName — read the accumulated 8-character PS name.
     *
     * @dname    programName
     * @return   Trimmed PS label when all four segments were received.
     * @pubstate reads psBuffer_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::optional<BroadcastLabel> programName() const;

    /**
     * @brief    radiotext — read the accumulated 64-character RT string.
     *
     * @dname    radiotext
     * @return   Trimmed radiotext when at least one RT segment was received.
     * @pubstate reads rtBuffer_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::optional<BroadcastLabel> radiotext() const;

private:
    static constexpr std::size_t kPsSegments = 4U;
    static constexpr std::size_t kRtSegments = 16U;

    std::array<char, 8> psBuffer_{};
    std::array<bool, kPsSegments> psSegmentValid_{};
    std::array<char, 64> rtBuffer_{};
    std::array<bool, kRtSegments> rtSegmentValid_{};
    bool rtAbFlag_{false};
    bool rtAbInitialized_{false};
};

} // namespace core
