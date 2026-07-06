/**
 * @file    Bt1035Driver.hpp
 * @brief   FSC-BT1035 Bluetooth transmitter — UART AT control.
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

#include "bt1035/Bt1035Error.hpp"

#include "core/Bt1035At.hpp"

#include <expected>

namespace bt1035 {

/**
 * @brief    Bt1035Pins — board GPIO/UART identifiers for the module.
 *
 * @dname    Bt1035Pins
 * @return   n/a (type)
 * @pubstate Immutable wiring snapshot from board_pins.hpp.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct Bt1035Pins {
    int uartTx;     ///< ESP32 TX -> module RX.
    int uartRx;     ///< ESP32 RX <- module TX.
    int rtsGpio;    ///< RTS (flow control).
    int ctsGpio;    ///< CTS (flow control).
    int resetGpio;  ///< Module RESET (active level per schematic).
    int sysCtlGpio; ///< SYS_CTL (optional module enable).
};

/**
 * @brief    Bt1035Driver — owns UART + reset, runs mandatory AT init.
 *
 * @dname    Bt1035Driver
 * @return   n/a (type)
 * @pubstate Owns UART port after boot(). booted_ true after init sequence
 *           including AT+AUXCFG=1 (Line-In from ADAU1701).
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class Bt1035Driver {
public:
    /**
     * @brief    Bt1035Driver — construct with board pin map.
     *
     * @dname    Bt1035Driver
     * @param    pins  UART, flow control, reset, SYS_CTL wiring.
     * @pubstate stores pins_; not booted until boot().
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    explicit Bt1035Driver(Bt1035Pins pins);

    /**
     * @brief    ~Bt1035Driver — release UART resources.
     *
     * @dname    ~Bt1035Driver
     * @pubstate deletes UART driver when installed.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    ~Bt1035Driver();

    Bt1035Driver(const Bt1035Driver&) = delete;
    Bt1035Driver& operator=(const Bt1035Driver&) = delete;

    /**
     * @brief    boot — reset module and run mandatory AT init sequence.
     *
     * @dname    boot
     * @return   Ok on success, or Bt1035Error.
     * @pubstate sets booted_ after Ping + AT+AUXCFG=1 both return OK.
     *
     * Sequence: hardware reset, UART @ 115200 with RTS/CTS, then
     * core::bootInitSequence() (Chapter~\ref{ch:bt1035}).
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, Bt1035Error> boot();

    /**
     * @brief    isBooted — query whether Line-In init succeeded.
     *
     * @dname    isBooted
     * @return   true after successful boot().
     * @pubstate reads booted_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] bool isBooted() const noexcept;

    /**
     * @brief    sendCommand — transmit one typed AT command and expect OK.
     *
     * @dname    sendCommand
     * @param    command  Enumerated AT command.
     * @return   Ok on OK response, or Bt1035Error.
     * @pubstate writes UART; reads until OK/ERROR/timeout.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, Bt1035Error> sendCommand(
        core::Bt1035AtCommand command);

private:
    [[nodiscard]] std::expected<void, Bt1035Error> ensureBooted() const;
    [[nodiscard]] std::expected<void, Bt1035Error> runInitSequence();
    [[nodiscard]] std::expected<void, Bt1035Error> transmitAndExpectOk(
        std::string_view commandLine);

    Bt1035Pins pins_;
    bool booted_;
    bool uartInstalled_;
    int uartPort_;
};

} // namespace bt1035
