/**
 * @file    WebRadioService.cpp
 * @brief   WebRadioService implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-08-08
 */

#include "webradio/WebRadioService.hpp"

#include "core/WebRadioJson.hpp"

namespace webradio {

namespace {

[[nodiscard]] core::WebRadioConfig loadInitialConfig(core::ISecureStore& store)
{
    if (store.hasWebRadioConfig()) {
        auto json = store.loadWebRadioConfigJson();
        if (json) {
            auto parsed = core::parseWebRadioConfigJson(*json);
            if (parsed) {
                return *parsed;
            }
        }
    }
    return core::WebRadioConfig::factoryDefault();
}

} // namespace

WebRadioService::WebRadioService(core::ISecureStore& store)
    : store_(store)
    , mutex_(xSemaphoreCreateMutex())
    , config_(loadInitialConfig(store))
{
}

WebRadioService::~WebRadioService()
{
    vSemaphoreDelete(mutex_);
}

core::WebRadioConfig WebRadioService::config() const
{
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const core::WebRadioConfig snapshot = config_;
    xSemaphoreGive(mutex_);
    return snapshot;
}

std::expected<void, core::StoreError> WebRadioService::setConfig(
    const core::WebRadioConfig& config)
{
    const std::string json = core::serializeWebRadioConfigJson(config);
    if (auto saved = store_.saveWebRadioConfigJson(json); !saved) {
        return saved;
    }

    xSemaphoreTake(mutex_, portMAX_DELAY);
    config_ = config;
    ++generation_;
    xSemaphoreGive(mutex_);
    return {};
}

std::uint32_t WebRadioService::generation() const
{
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const std::uint32_t value = generation_;
    xSemaphoreGive(mutex_);
    return value;
}

core::WebRadioStatus WebRadioService::status() const
{
    xSemaphoreTake(mutex_, portMAX_DELAY);
    core::WebRadioStatus snapshot = status_;
    xSemaphoreGive(mutex_);
    return snapshot;
}

void WebRadioService::publishStatus(const core::WebRadioStatus& status)
{
    xSemaphoreTake(mutex_, portMAX_DELAY);
    status_ = status;
    xSemaphoreGive(mutex_);
}

} // namespace webradio
