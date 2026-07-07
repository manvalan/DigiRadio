/**
 * @file    DeviceIdentity.hpp
 * @brief   Per-board identity derived from the factory EUI-48 or fallbacks.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-07
 */
#pragma once

#include "core/Eui48.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace core {

/**
 * @brief    DeviceIdentity — SSID, Bluetooth name, hostname, and serial strings.
 *
 * @dname    DeviceIdentity
 * @return   n/a (type)
 * @pubstate Immutable after construction. unknown() uses documented fallbacks
 *           when the EEPROM read fails.
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
class DeviceIdentity {
public:
    /**
     * @brief    unknown — identity when the EUI-48 read fails.
     *
     * @dname    unknown
     * @return   DeviceIdentity with fallback SSID and serial "unknown".
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] static DeviceIdentity unknown() noexcept;

    /**
     * @brief    fromEui48 — derive network-visible names from factory EUI-48.
     *
     * @dname    fromEui48
     * @param    eui  Factory-programmed identifier from the 24AA025E48.
     * @return   DeviceIdentity with DigiRadio-<suffix> strings.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] static DeviceIdentity fromEui48(Eui48 eui);

    /**
     * @brief    isKnown — whether a factory EUI-48 was read successfully.
     *
     * @dname    isKnown
     * @return   true when fromEui48 was used.
     * @pubstate reads eui_.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] bool isKnown() const noexcept;

    /**
     * @brief    serialNumber — canonical serial or "unknown".
     *
     * @dname    serialNumber
     * @return   Uppercase hex serial when known.
     * @pubstate reads serialNumber_.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] std::string_view serialNumber() const noexcept;

    /**
     * @brief    softApSsid — setup SoftAP SSID for this unit.
     *
     * @dname    softApSsid
     * @return   DigiRadio-<suffix> or the DigiRadio-setup fallback.
     * @pubstate reads softApSsid_.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] std::string_view softApSsid() const noexcept;

    /**
     * @brief    bluetoothName — GAP friendly name for the BT1035 module.
     *
     * @dname    bluetoothName
     * @return   DigiRadio-<suffix> or plain DigiRadio when unknown.
     * @pubstate reads bluetoothName_.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] std::string_view bluetoothName() const noexcept;

    /**
     * @brief    hostname — STA hostname / mDNS label (no .local suffix).
     *
     * @dname    hostname
     * @return   digiradio-<suffix> or digiradio when unknown.
     * @pubstate reads hostname_.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] std::string_view hostname() const noexcept;

    /**
     * @brief    eui48 — optional factory identifier backing this identity.
     *
     * @dname    eui48
     * @return   EUI-48 when known; otherwise empty.
     * @pubstate reads eui_.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] const std::optional<Eui48>& eui48() const noexcept;

private:
    DeviceIdentity(std::optional<Eui48> eui,
                   std::string serialNumber,
                   std::string softApSsid,
                   std::string bluetoothName,
                   std::string hostname);

    std::optional<Eui48> eui_;
    std::string serialNumber_;
    std::string softApSsid_;
    std::string bluetoothName_;
    std::string hostname_;
};

} // namespace core
