/**
 * @file    Si4684Types.hpp
 * @brief   Domain types for Si4684 tuner operations (DAB/FM status, services).
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

#include "core/FrequencyKHz.hpp"
#include "core/SeekDirection.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace si4684 {

/**
 * @brief    Si4684DigitalServiceType — START_DIGITAL_SERVICE mode byte.
 *
 * @dname    Si4684DigitalServiceType
 * @return   n/a (type)
 * @pubstate n/a
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
enum class Si4684DigitalServiceType : std::uint8_t {
    Audio = 0x00,
    Packet = 0x01,
};

/**
 * @brief    SeekBandWrap — FM seek band-wrap behaviour (FM_SEEK_START).
 *
 * @dname    SeekBandWrap
 * @return   n/a (type)
 * @pubstate n/a
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
enum class SeekBandWrap { Wrap, NoWrap };

/**
 * @brief    Si4684PartInfo — chip identity from GET_PART_INFO (AN649).
 *
 * @dname    Si4684PartInfo
 * @return   n/a (type)
 * @pubstate Plain DTO from GET_PART_INFO response bytes.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct Si4684PartInfo {
    std::uint16_t chipId;         ///< Part identifier from the chip.
    std::uint8_t firmwareMajor;   ///< Loaded firmware major version.
    std::uint8_t firmwareMinor;   ///< Loaded firmware minor version.
    std::uint8_t firmwareBuild;   ///< Loaded firmware build number.
};

/**
 * @brief    Si4684SysState — running application after boot.
 *
 * @dname    Si4684SysState
 * @return   n/a (type)
 * @pubstate Plain DTO from GET_SYS_STATE.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct Si4684SysState {
    std::uint8_t imageType; ///< Loaded image type byte from the chip.
};

/**
 * @brief    Si4684FmRsq — FM received-signal quality (FM_RSQ_STATUS).
 *
 * @dname    Si4684FmRsq
 * @return   n/a (type)
 * @pubstate Plain DTO from FM_RSQ_STATUS response bytes.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct Si4684FmRsq {
    core::FrequencyKHz frequency; ///< Tuned centre frequency in kHz.
    std::int8_t rssiDbuV;         ///< RSSI in dBµV.
    std::int8_t snrDb;            ///< SNR in dB.
    bool valid;                   ///< RSQ valid flag from the chip.
    bool stereo;                  ///< Stereo pilot detected.
};

/**
 * @brief    Si4684FmRdsStatus — raw RDS group snapshot (FM_RDS_STATUS).
 *
 * @dname    Si4684FmRdsStatus
 * @return   n/a (type)
 * @pubstate Plain DTO from FM_RDS_STATUS response bytes.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct Si4684FmRdsStatus {
    std::uint16_t blockA; ///< RDS block A.
    std::uint16_t blockB; ///< RDS block B.
    std::uint16_t blockC; ///< RDS block C.
    std::uint16_t blockD; ///< RDS block D.
    bool received;        ///< Group received flag.
    std::uint8_t fifoUsed; ///< Remaining groups in the RDS FIFO.
};

/**
 * @brief    Si4684DabServiceData — one GET_DIGITAL_SERVICE_DATA block.
 *
 * @dname    Si4684DabServiceData
 * @return   n/a (type)
 * @pubstate Plain DTO from GET_DIGITAL_SERVICE_DATA response bytes.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct Si4684DabServiceData {
    std::uint32_t serviceId;    ///< Associated DAB service identifier.
    std::uint32_t componentId;  ///< Associated component identifier.
    std::uint8_t dataSrc;       ///< Payload source (2 = DLS PAD).
    std::uint16_t byteCount;    ///< Payload byte count.
    std::uint16_t segmentIndex; ///< Zero-based segment index.
    std::uint16_t segmentCount; ///< Total segments for this label object.
    std::vector<std::uint8_t> payload; ///< Raw payload bytes.
};

/**
 * @brief    Si4684DabDigRadStatus — ensemble lock metrics (DAB_DIGRAD_STATUS).
 *
 * @dname    Si4684DabDigRadStatus
 * @return   n/a (type)
 * @pubstate Plain DTO from DAB_DIGRAD_STATUS response bytes.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct Si4684DabDigRadStatus {
    std::uint8_t ficQuality; ///< FIC quality 0–100.
    std::uint8_t cnrDb;      ///< CNR in dB.
    bool acquired;           ///< Ensemble acquired flag.
    bool valid;              ///< DIGRAD valid flag.
};

/**
 * @brief    Si4684DabEventStatus — DAB event flags (DAB_GET_EVENT_STATUS).
 *
 * @dname    Si4684DabEventStatus
 * @return   n/a (type)
 * @pubstate Plain DTO from DAB_GET_EVENT_STATUS response bytes.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct Si4684DabEventStatus {
    bool serviceListReady; ///< Service list available for fetch.
    bool reconfig;         ///< Ensemble reconfiguration event.
};

/**
 * @brief    Si4684DabService — one row from GET_DIGITAL_SERVICE_LIST.
 *
 * @dname    Si4684DabService
 * @return   n/a (type)
 * @pubstate Plain DTO parsed from a service-list chunk.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct Si4684DabService {
    std::uint32_t serviceId;    ///< DAB service identifier.
    std::uint32_t componentId;  ///< Audio component identifier.
    std::array<char, 17> label; ///< Programme label, NUL-terminated.
    std::uint8_t serviceType;   ///< Service type byte from the list.
};

/** Band III DAB channel plan (kHz), PE5PVB / ETSI EN 300 401 Table 14. */
inline constexpr std::array<std::uint32_t, 38> kDefaultDabFrequencyKhz = {
    174928, 176640, 178352, 180064, 181936, 183648, 185360, 187072, 188928,
    190640, 192352, 194064, 195936, 197648, 199360, 201072, 202928, 204640,
    206352, 208064, 209936, 211648, 213360, 215072, 216928, 218640, 220352,
    222064, 223936, 225648, 227360, 229072, 230784, 232496, 234208, 235776,
    237488, 239200,
};

} // namespace si4684
