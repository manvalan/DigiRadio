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

#include <cstdint>
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
    int resetGpio;  ///< Module RESET# (pin 8), active-low. Driven (not
                    ///< floating, since 2026-08-23): held LOW together with
                    ///< SYS_CTRL past the datasheet §4.8 Reset Protection
                    ///< timeout (~1.8 s) to force a genuine power-down on
                    ///< every resetAndInitOnce() attempt, then released HIGH
                    ///< before SYS_CTRL's power-up pulse (§4.7).
    int sysCtlGpio; ///< SYS_CTL (pin 34), active-high. Driven LOW/HIGH on
                    ///< every resetAndInitOnce() call, together with
                    ///< resetGpio, to force a real power-cycle each retry.
    int ctsGpio;    ///< Host->module UART_CTS (module pin 15). Diagnostic
                    ///< only (2026-08-22): read-only floating input, never
                    ///< driven — this driver does not implement hardware
                    ///< flow control. See boot()'s comment.
    int rtsGpio;    ///< Module UART_RTS/PIO2 (module pin 16), factory
                    ///< default function is PA_MUTE, not flow control
                    ///< (Feasycom programming guide). Diagnostic only
                    ///< (2026-08-22): read-only floating input.
};

/**
 * @brief    Bt1035Driver — owns UART + reset, runs mandatory AT init.
 *
 * @dname    Bt1035Driver
 * @return   n/a (type)
 * @pubstate Owns UART port after boot(). booted_ true after init sequence
 *           including AT+AUXCFG=3 and AT+I2SCFG=35 (I2S slave from ADAU1701).
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
     * @brief    queryA2dpEncoder — read negotiated A2DP codec (AT+A2DPENC).
     *
     * @dname    queryA2dpEncoder
     * @return   Parsed codec on success, or Bt1035Error. Failing here while
     *           queryA2dpState() reports Streaming means the module has a
     *           link but is not actually encoding audio — a real fault,
     *           not just a quiet source.
     * @pubstate writes UART; parses +A2DPENC from the reply.
     *
     * @author   Michele Bigi
     * @date     2026-08-07
     */
    [[nodiscard]] std::expected<core::Bt1035A2dpCodec, Bt1035Error>
    queryA2dpEncoder();

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
     * @brief    setA2dpCodecConfig — enable optional A2DP codecs (AT+A2DPCFG).
     *
     * @dname    setA2dpCodecConfig
     * @param    bitmask  BIT0=AAC, BIT1=aptX, BIT2=aptX-LL, BIT3=aptX-HD,
     *                    BIT4=aptX-Adaptive, BIT5=LDAC (§5.3.4); 0 forces
     *                    the mandatory SBC-only baseline.
     * @return   Ok on success, or Bt1035Error. Only affects the *next* A2DP
     *          negotiation — an already-connected peer keeps its current
     *          codec until it reconnects (see disconnectA2dp()).
     * @pubstate writes UART.
     *
     * @author   Michele Bigi
     * @date     2026-08-24
     */
    [[nodiscard]] std::expected<void, Bt1035Error> setA2dpCodecConfig(
        std::uint8_t bitmask);

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

    /**
     * @brief    scanNearbyBrEdr — inquiry for nearby A2DP sink devices.
     *
     * @dname    scanNearbyBrEdr
     * @param    scanSeconds  BR/EDR scan duration 1–255 (default 20 in service).
     * @return   Parsed +SCAN entries, or Bt1035Error.
     * @pubstate sends AT+SCAN=1,n and collects until +SCAN=E.
     *
     * @author   Michele Bigi
     * @date     2026-08-05
     */
    [[nodiscard]] std::expected<std::vector<core::Bt1035ScannedDevice>, Bt1035Error>
    scanNearbyBrEdr(std::uint8_t scanSeconds = 20U);

    /**
     * @brief    stopScan — abort an active AT+SCAN inquiry.
     *
     * @dname    stopScan
     * @return   Ok on success, or Bt1035Error.
     * @pubstate sends AT+SCAN=0.
     *
     * @author   Michele Bigi
     * @date     2026-08-05
     */
    [[nodiscard]] std::expected<void, Bt1035Error> stopScan();

    /**
     * @brief    prepareForOutgoingConnect — disable auto-link before A2DPCONN.
     *
     * @dname    prepareForOutgoingConnect
     * @pubstate sends LINKCFG/AUTOCONN off; does not clear PLIST.
     *
     * @author   Michele Bigi
     * @date     2026-08-05
     */
    void prepareForOutgoingConnect();

    /**
     * @brief    waitForA2dpConnected — poll until A2DP link is up.
     *
     * @dname    waitForA2dpConnected
     * @param    timeoutMs  Maximum wait in milliseconds.
     * @return   true when Connected/Streaming/Paused, false on timeout.
     * @pubstate polls AT+A2DPSTAT.
     *
     * @author   Michele Bigi
     * @date     2026-08-05
     */
    [[nodiscard]] bool waitForA2dpConnected(int timeoutMs);

    /**
     * @brief    startA2dpAudio — send AT+A2DPAUDIO=1 (Feasycom §5.3.6).
     *
     * @dname    startA2dpAudio
     * @return   Ok on module OK, or Bt1035Error.
     * @pubstate writes UART.
     *
     * @author   Michele Bigi
     * @date     2026-08-05
     */
    [[nodiscard]] std::expected<void, Bt1035Error> startA2dpAudio();

    /**
     * @brief    waitForA2dpStreaming — poll until +A2DPSTAT=4 (Streaming).
     *
     * @dname    waitForA2dpStreaming
     * @param    timeoutMs  Maximum wait in milliseconds.
     * @return   true when Streaming, false on timeout.
     * @pubstate polls AT+A2DPSTAT; sends A2DPAUDIO=1 when stuck at Connected.
     *
     * @author   Michele Bigi
     * @date     2026-08-05
     */
    [[nodiscard]] bool waitForA2dpStreaming(int timeoutMs);

    /**
     * @brief    connectA2dp — pair/connect to a remote by MAC (AT+A2DPCONN).
     *
     * @dname    connectA2dp
     * @param    mac  12-char ASCII MAC from scan results.
     * @return   Ok on success, or Bt1035Error.
     * @pubstate may take several seconds while the module pairs.
     *
     * @author   Michele Bigi
     * @date     2026-08-05
     */
    [[nodiscard]] std::expected<void, Bt1035Error> connectA2dp(
        std::string_view mac);

private:
    [[nodiscard]] std::expected<void, Bt1035Error> ensureBooted() const;
    [[nodiscard]] std::expected<void, Bt1035Error> runInitSequence();
    [[nodiscard]] std::expected<void, Bt1035Error> resetAndInitOnce();
    [[nodiscard]] std::expected<std::string, Bt1035Error> transmitAndCollect(
        std::string_view commandLine, int timeoutMs = kResponseTimeoutMs);
    [[nodiscard]] std::expected<std::string, Bt1035Error> transmitAndCollectUntil(
        std::string_view commandLine, std::string_view endMarker, int timeoutMs,
        std::uint8_t minScanSeconds = 0U);
    [[nodiscard]] std::expected<void, Bt1035Error> transmitAndExpectOk(
        std::string_view commandLine);
    [[nodiscard]] std::expected<void, Bt1035Error> transmitAndExpectOkLogged(
        std::string_view label, std::string_view commandLine);

    void prepareForInquiryScan();
    [[nodiscard]] bool waitForA2dpIdle(int timeoutMs);
    [[nodiscard]] std::expected<std::vector<core::Bt1035ScannedDevice>, Bt1035Error>
    runInquiryScan(std::uint8_t scanType, std::uint8_t scanSeconds);

    static constexpr int kResponseTimeoutMs = 2000;
    static constexpr int kConnectTimeoutMs = 15000;

    Bt1035Pins pins_;
    bool booted_;
    bool uartInstalled_;
    int uartPort_;
};

} // namespace bt1035
