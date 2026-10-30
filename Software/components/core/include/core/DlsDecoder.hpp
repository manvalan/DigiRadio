/**
 * @file    DlsDecoder.hpp
 * @brief   Decode DAB DLS / DL Plus payloads forwarded by the Si468x.
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
 * @date    2026-10-26
 */
#pragma once

#include "core/BroadcastLabel.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace core {

/**
 * @brief    DlPlusItem — DL Plus ITEM tags resolved against the label.
 *
 * @dname    DlPlusItem
 * @return   n/a (type)
 * @pubstate Plain DTO; each field set only when the broadcaster tagged it
 *           and the item is running.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
struct DlPlusItem {
    std::optional<BroadcastLabel> title;  ///< ITEM.TITLE (content type 1).
    std::optional<BroadcastLabel> artist; ///< ITEM.ARTIST (content type 4).
};

/**
 * @brief    DlsDecoder — current DAB dynamic label plus its DL Plus tags.
 *
 * @dname    DlsDecoder
 * @param    none
 * @return   n/a (type)
 * @pubstate Public interface: reset(), apply(payload), label(), item().
 *           Holds the last DL message (as code points) and its tags.
 *
 * Each payload is one reassembled GET_DIGITAL_SERVICE_DATA DLS packet:
 * the AN649 §7.14.2.1 Table 21 two-byte prefix, then either a DL
 * message (C=0, Table 22) or a command (C=1): "remove label" (0001) or
 * DL Plus (0010, Table 23, tags per ETSI TS 102 980 §7.2). The prefix
 * never reaches the label text. DL Plus markers index characters of the
 * message, so the text is kept as code points; tags apply only while
 * their Link bit matches the message Toggle bit.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
class DlsDecoder {
public:
    /**
     * @brief    reset — forget the label and its tags (new service/tune).
     *
     * @dname    reset
     * @pubstate clears text_, tags_ and the toggle/link flags.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    void reset() noexcept;

    /**
     * @brief    apply — ingest one reassembled DLS payload (prefix + body).
     *
     * @dname    apply
     * @param    payload  Raw DSRV payload bytes, AN649 Table 21 prefix first.
     * @pubstate writes text_/toggle_ (message), tags_/link_/itemRunning_
     *           (DL Plus) or clears both (remove label). Malformed
     *           payloads are ignored and leave the state unchanged.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    void apply(std::span<const std::uint8_t> payload);

    /**
     * @brief    label — the current dynamic label as UTF-8.
     *
     * @dname    label
     * @return   Trimmed label, or nullopt when none is on air.
     * @pubstate reads text_.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] std::optional<BroadcastLabel> label() const;

    /**
     * @brief    item — DL Plus title/artist for the current label.
     *
     * @dname    item
     * @return   Resolved tags; empty when no matching DL Plus is on air.
     * @pubstate reads text_, toggle_, tags_, link_ and itemRunning_.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] DlPlusItem item() const;

private:
    struct Tag {
        std::uint8_t contentType;
        std::uint8_t start;
        std::uint8_t length;
    };

    void applyMessage(bool toggle, std::uint8_t charset,
                      std::span<const std::uint8_t> body);
    void applyDlPlus(bool link, std::span<const std::uint8_t> body);
    [[nodiscard]] std::optional<BroadcastLabel>
    tagged(std::uint8_t contentType) const;

    std::u32string text_;
    bool toggle_{false};
    std::vector<Tag> tags_;
    bool link_{false};
    bool itemRunning_{false};
};

} // namespace core
