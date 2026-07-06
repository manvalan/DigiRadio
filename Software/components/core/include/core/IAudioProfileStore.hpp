/**
 * @file    IAudioProfileStore.hpp
 * @brief   Persistence boundary for user audio configuration.
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

#include "core/AudioProfile.hpp"
#include "core/StoreError.hpp"

#include <expected>

namespace core {

/**
 * @brief    IAudioProfileStore — load/save AudioProfile in NVS (non-secret).
 *
 * @dname    IAudioProfileStore
 * @return   n/a (type)
 * @pubstate Shell implementations own NVS handles; core uses fakes in tests.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class IAudioProfileStore {
public:
    virtual ~IAudioProfileStore() = default;

    /**
     * @brief    hasProfile — check whether a saved profile exists.
     *
     * @dname    hasProfile
     * @return   true when loadProfile would succeed.
     * @pubstate reads backing storage via implementation.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual bool hasProfile() const = 0;

    /**
     * @brief    saveProfile — persist a validated audio profile.
     *
     * @dname    saveProfile
     * @param    profile  User mixer/EQ/master snapshot.
     * @return   Ok on success, or StoreError::IoFailed.
     * @pubstate writes backing storage via implementation.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<void, StoreError> saveProfile(
        const AudioProfile& profile) = 0;

    /**
     * @brief    loadProfile — read the stored audio profile.
     *
     * @dname    loadProfile
     * @return   AudioProfile on success, or StoreError::NotFound / IoFailed.
     * @pubstate reads backing storage via implementation.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<AudioProfile, StoreError> loadProfile()
        const = 0;

    /**
     * @brief    clearProfile — erase the stored audio profile.
     *
     * @dname    clearProfile
     * @return   Ok on success, or StoreError::IoFailed.
     * @pubstate clears backing storage via implementation.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<void, StoreError> clearProfile() = 0;
};

} // namespace core
