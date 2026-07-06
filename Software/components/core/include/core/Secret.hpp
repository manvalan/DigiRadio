/**
 * @file    Secret.hpp
 * @brief   Opaque wrapper for sensitive strings (passwords, keys).
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

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace core {

/**
 * @brief    Secret — holds sensitive bytes, zeroised on destruction.
 *
 * @dname    Secret
 * @return   n/a (type)
 * @pubstate Owns bytes_ (immutable after construction except via move).
 *           No operator<<; never log or serialise this type to plaintext.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class Secret {
public:
    /**
     * @brief    Secret — construct from a plaintext value at the boundary.
     *
     * @dname    Secret
     * @param    value  Sensitive string moved into protected storage.
     * @pubstate writes bytes_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    explicit Secret(std::string value);

    Secret(const Secret&) = delete;
    Secret& operator=(const Secret&) = delete;

    /**
     * @brief    Secret — move-construct, transferring protected storage.
     *
     * @dname    Secret
     * @param    other  Source secret; zeroised after the move.
     * @pubstate transfers bytes_ from other.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    Secret(Secret&& other) noexcept;

    /**
     * @brief    operator= — move-assign, transferring protected storage.
     *
     * @dname    operator=
     * @param    other  Source secret; zeroised after the move.
     * @return   Reference to this instance.
     * @pubstate transfers bytes_ from other.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    Secret& operator=(Secret&& other) noexcept;

    /**
     * @brief    ~Secret — zeroise stored bytes.
     *
     * @dname    ~Secret
     * @pubstate clears bytes_ securely.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    ~Secret();

    /**
     * @brief    length — byte length of the protected value.
     *
     * @dname    length
     * @return   Number of stored bytes.
     * @pubstate reads bytes_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::size_t length() const noexcept;

    /**
     * @brief    usePlaintext — invoke a callable with a borrowed view.
     *
     * @dname    usePlaintext
     * @param    fn  Callable invoked with the secret as string_view.
     * @return   Whatever fn returns.
     * @pubstate reads bytes_; fn must not retain the view past its scope.
     *
     * Shell-only escape hatch for APIs (e.g. esp_wifi_set_config) that
     * require a transient C string. Never log inside fn.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    template<typename Fn>
    [[nodiscard]] auto usePlaintext(Fn&& fn) const
        -> decltype(fn(std::declval<std::string_view>()))
    {
        return std::forward<Fn>(fn)(bytes_);
    }

private:
    void zeroize() noexcept;

    std::string bytes_;
};

} // namespace core
