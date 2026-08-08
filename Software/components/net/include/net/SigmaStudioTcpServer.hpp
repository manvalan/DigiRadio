/**
 * @file    SigmaStudioTcpServer.hpp
 * @brief   TCP bridge exposing the ADAU1701 to SigmaStudio's Remote Connection.
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
 * Implements the subset of SigmaStudio's TCPi wire protocol needed for
 * Connect, Link Compile Download, register read/write, and runtime
 * safeload parameter updates, ported from the ADAU1701-TCPi-ESP32
 * reference project (https://github.com/rarranzb/ADAU1701-TCPi-ESP32,
 * MIT) onto DigiRadio's existing I2C plumbing
 * (components/drivers/adau1701/src/SigmaStudioFW.c). A completed Link
 * Compile Download is persisted to the `dsp` flash partition via
 * adau1701::FlashDspProgramSource::storeBlob(), so it becomes the
 * program DigiRadio boots with next time — same mechanism as
 * POST /api/dsp/program.
 *
 * @author  Michele Bigi
 * @date    2026-08-07
 */
#pragma once

#include "net/NetError.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <expected>

namespace net {

/**
 * @brief    SigmaStudioTcpServer — port 8086 bridge to the ADAU1701.
 *
 * @dname    SigmaStudioTcpServer
 * @return   n/a (type)
 * @pubstate Owns a listening socket and a FreeRTOS task for the process
 *           lifetime once started(); stop()/destructor tear both down.
 *           Requires adau1701::Adau1701Driver::boot() to have already
 *           bound the I2C device handle via sigma_studio_set_device().
 *           Only one instance may be started at a time: the accept task
 *           reads the listen fd from a process-lifetime singleton (see
 *           activeListenFd() in the .cpp) rather than a captured `this`,
 *           since instances are constructed as locals and move-relocated
 *           into NetBootstrap — same rationale as SetupWebServer's
 *           routeContextStorage().
 *
 * @author   Michele Bigi
 * @date     2026-08-07
 */
class SigmaStudioTcpServer {
public:
    /**
     * @brief    SigmaStudioTcpServer — construct an unstarted server.
     *
     * @dname    SigmaStudioTcpServer
     * @pubstate clears listenFd_ and task_.
     *
     * @author   Michele Bigi
     * @date     2026-08-07
     */
    SigmaStudioTcpServer();

    /**
     * @brief    ~SigmaStudioTcpServer — stop the server if running.
     *
     * @dname    ~SigmaStudioTcpServer
     * @pubstate deletes task_ and closes listenFd_ when started.
     *
     * @author   Michele Bigi
     * @date     2026-08-07
     */
    ~SigmaStudioTcpServer();

    SigmaStudioTcpServer(const SigmaStudioTcpServer&) = delete;
    SigmaStudioTcpServer& operator=(const SigmaStudioTcpServer&) = delete;

    /**
     * @brief    SigmaStudioTcpServer — move-construct, transferring ownership.
     *
     * @dname    SigmaStudioTcpServer
     * @param    other  Source server; left stopped after the move.
     * @pubstate takes ownership of other.listenFd_ and other.task_.
     *
     * @author   Michele Bigi
     * @date     2026-08-07
     */
    SigmaStudioTcpServer(SigmaStudioTcpServer&& other) noexcept;

    /**
     * @brief    operator= — move-assign, transferring ownership.
     *
     * @dname    operator=
     * @param    other  Source server; left stopped after the move.
     * @return   Reference to this instance.
     * @pubstate takes ownership of other.listenFd_ and other.task_.
     *
     * @author   Michele Bigi
     * @date     2026-08-07
     */
    SigmaStudioTcpServer& operator=(SigmaStudioTcpServer&& other) noexcept;

    /**
     * @brief    start — open the listening socket and spawn the server task.
     *
     * @dname    start
     * @return   Ok on success, or NetError::TcpServerStartFailed.
     * @pubstate writes listenFd_ and task_ on success.
     *
     * @author   Michele Bigi
     * @date     2026-08-07
     */
    [[nodiscard]] std::expected<void, NetError> start();

private:
    void stop() noexcept;

    /** @brief FreeRTOS task entry: accepts and serves one client at a time. */
    static void acceptLoopTask(void* arg);

    int listenFd_;
    TaskHandle_t task_;
};

} // namespace net
