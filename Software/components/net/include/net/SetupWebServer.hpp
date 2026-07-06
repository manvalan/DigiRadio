/**
 * @file    SetupWebServer.hpp
 * @brief   HTTP server for setup UI, health, and Wi-Fi provisioning API.
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
 * @date    2026-07-06
 */
#pragma once

#include "core/ISecureStore.hpp"
#include "net/NetError.hpp"
#include "net/NetState.hpp"

#include <expected>

struct httpd_req;

namespace tuner {
class TunerService;
} // namespace tuner

struct httpd_handle;

namespace net {

/**
 * @brief    HttpRouteContext — dependencies injected into HTTP handlers.
 *
 * @dname    HttpRouteContext
 * @return   n/a (type)
 * @pubstate Borrows store and tuner for the lifetime of SetupWebServer.
 *           Passed as esp_http_server user_ctx (no file-scope globals).
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct HttpRouteContext {
    core::ISecureStore* store;     ///< Secure store for Wi-Fi provisioning.
    tuner::TunerService* tuner;    ///< Tuner service for tuner REST routes.
};

/**
 * @brief    SetupWebServer — setup UI, health, and Wi-Fi provisioning API.
 *
 * @dname    SetupWebServer
 * @return   n/a (type)
 * @pubstate Owns server_ and borrows store_ while running. Routes delegate
 *           JSON work to the pure core.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class SetupWebServer {
public:
    /**
     * @brief    SetupWebServer — construct an unstarted server.
     *
     * @dname    SetupWebServer
     * @pubstate clears server_, store_, and netState_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    SetupWebServer();

    /**
     * @brief    ~SetupWebServer — stop the HTTP server if running.
     *
     * @dname    ~SetupWebServer
     * @pubstate stops server_ when non-null.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    ~SetupWebServer();

    SetupWebServer(const SetupWebServer&) = delete;
    SetupWebServer& operator=(const SetupWebServer&) = delete;

    /**
     * @brief    SetupWebServer — move-construct, transferring the handle.
     *
     * @dname    SetupWebServer
     * @param    other  Source server; left stopped after the move.
     * @pubstate takes ownership of other.server_ and copies store pointer.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    SetupWebServer(SetupWebServer&& other) noexcept;

    /**
     * @brief    operator= — move-assign, transferring the handle.
     *
     * @dname    operator=
     * @param    other  Source server; left stopped after the move.
     * @return   Reference to this instance.
     * @pubstate takes ownership of other.server_ and copies store pointer.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    SetupWebServer& operator=(SetupWebServer&& other) noexcept;

    /**
     * @brief    start — register routes and listen on port 80.
     *
     * @dname    start
     * @param    store     Secure store for POST /api/wifi persistence.
     * @param    netState  Active network phase exposed to handlers.
     * @param    tuner     Tuner service for the tuner REST routes.
     * @return   Ok on success, or NetError::HttpServerStartFailed.
     * @pubstate writes server_, store_, netState_, and tuner_ on success.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, NetError> start(core::ISecureStore& store,
                                                      NetState netState,
                                                      tuner::TunerService& tuner);

private:
    httpd_handle* server_;
    core::ISecureStore* store_;
    NetState netState_;
    tuner::TunerService* tuner_;
    HttpRouteContext routeContext_;
};

} // namespace net
