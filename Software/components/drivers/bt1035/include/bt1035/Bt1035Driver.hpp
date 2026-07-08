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
#include <string>
#include <string_view>
#include <vector>

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
 *           including AT+AUXCFG=3 and AT+I2SCFG=67 (I2S slave from ADAU1701).
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
     * @pubstate sets booted_ after Ping + I2S init both return OK.
     *
     * Sequence: hardware reset, UART @ 115200 with RTS/CTS, then
     * core::bootInitSequence() (see manual chapter bt1035).
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, Bt1035Error> boot();

    /**
     * @brief    isBooted — query whether I2S init succeeded.
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

    /**
     * @brief    enterPairingMode — make module discoverable (AT+PAIR=1).
     *
     * @dname    enterPairingMode
     * @return   Ok on OK response, or Bt1035Error.
     * @pubstate writes UART; module advertises until paired or leavePairingMode().
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, Bt1035Error> enterPairingMode();

    /**
     * @brief    leavePairingMode — stop discoverable advertising (AT+PAIR=0).
     *
     * @dname    leavePairingMode
     * @return   Ok on OK response, or Bt1035Error.
     * @pubstate writes UART.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, Bt1035Error> leavePairingMode();

    /**
     * @brief    queryA2dpState — read current A2DP link state (AT+A2DPSTAT).
     *
     * @dname    queryA2dpState
     * @return   Parsed A2DP state on success, or Bt1035Error.
     * @pubstate writes UART; parses +A2DPSTAT from the reply.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<core::Bt1035A2dpState, Bt1035Error>
    queryA2dpState();

    /**
     * @brief    disconnectA2dp — release the active A2DP session (AT+A2DPDISC).
     *
     * @dname    disconnectA2dp
     * @return   Ok on OK response, or Bt1035Error.
     * @pubstate writes UART.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, Bt1035Error> disconnectA2dp();

    /**
     * @brief    setDeviceName — set the module GAP friendly name (AT+NAME).
     *
     * @dname    setDeviceName
     * @param    name  Bluetooth name (Feasycom FSC-BT1035 AT+NAME command).
     * @return   Ok on success, or Bt1035Error.
     * @pubstate sends AT+NAME after boot; does not alter I2S init sequence.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] std::expected<void, Bt1035Error> setDeviceName(
        std::string_view name);

    /**
     * @brief    queryDeviceName — read GAP friendly name (AT+NAME).
     *
     * @dname    queryDeviceName
     * @return   Module name on success, or Bt1035Error.
     * @pubstate writes UART; parses +NAME= from the reply.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] std::expected<std::string, Bt1035Error> queryDeviceName();

    /**
     * @brief    setAutoReconnect — configure power-on reconnect (AT+AUTOCONN).
     *
     * @dname    setAutoReconnect
     * @param    times  0 off, 1–15 attempts per Feasycom manual.
     * @return   Ok on success, or Bt1035Error.
     * @pubstate writes UART.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] std::expected<void, Bt1035Error> setAutoReconnect(
        std::uint8_t times);

    /**
     * @brief    queryAutoReconnect — read AT+AUTOCONN setting.
     *
     * @dname    queryAutoReconnect
     * @return   Reconnect count 0–15, or Bt1035Error.
     * @pubstate writes UART.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] std::expected<std::uint8_t, Bt1035Error> queryAutoReconnect();

    /**
     * @brief    queryPairedList — enumerate paired remotes (AT+PLIST).
     *
     * @dname    queryPairedList
     * @return   Parsed paired devices, or Bt1035Error.
     * @pubstate writes UART; may take longer than a single-line command.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] std::expected<std::vector<core::Bt1035PairedDevice>, Bt1035Error>
    queryPairedList();

private:
    [[nodiscard]] std::expected<void, Bt1035Error> ensureBooted() const;
    [[nodiscard]] std::expected<void, Bt1035Error> runInitSequence();
    [[nodiscard]] std::expected<std::string, Bt1035Error> transmitAndCollect(
        std::string_view commandLine, int timeoutMs = kResponseTimeoutMs);
    [[nodiscard]] std::expected<void, Bt1035Error> transmitAndExpectOk(
        std::string_view commandLine);

    Bt1035Pins pins_;
    bool booted_;
    bool uartInstalled_;
    int uartPort_;
};

} // namespace bt1035
