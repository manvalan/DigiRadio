/**
 * @file    blob_store_test.cpp
 * @brief   Host tests for the Si4684 image store layout.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */

#include "core/BlobStore.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

constexpr std::uint32_t kCapacity = 0x200000U;

[[nodiscard]] bool expect(bool ok, const char* what)
{
    if (!ok) {
        std::cerr << "FAILED: " << what << '\n';
    }
    return ok;
}

[[nodiscard]] bool crcTest()
{
    constexpr std::string_view text = "123456789";
    const auto bytes = std::as_bytes(std::span(text.data(), text.size()));
    const std::uint32_t chained =
        core::blobCrc32(bytes.subspan(4U), core::blobCrc32(bytes.first(4U)));
    return expect(core::blobCrc32(bytes) == 0xCBF43926U, "CRC-32 check value")
           && expect(chained == 0xCBF43926U, "chained CRC");
}

[[nodiscard]] bool layoutTest()
{
    const auto header = core::layoutBlobStore({5796U, 517000U, 530000U},
                                              {1U, 2U, 3U}, kCapacity);
    if (!expect(header.has_value(), "fits")) {
        return false;
    }
    const auto& e = header->entries;
    return expect(e[0].offset == 4096U, "patch after header")
           && expect(e[1].offset == 12288U, "dab sector aligned")
           && expect(e[2].offset % 4096U == 0U && e[2].offset >= e[1].offset + 517000U,
                     "fm after dab")
           && expect(!core::layoutBlobStore({1U, 2U, kCapacity}, {}, kCapacity),
                     "too big rejected")
           && expect(!core::layoutBlobStore({0U, 2U, 3U}, {}, kCapacity),
                     "empty image rejected");
}

[[nodiscard]] bool headerRoundTripTest()
{
    const auto header = core::layoutBlobStore({5796U, 517000U, 530000U},
                                              {0xAAU, 0xBBU, 0xCCU}, kCapacity);
    auto bytes = core::serializeBlobStoreHeader(*header);
    const auto parsed = core::parseBlobStoreHeader(bytes, kCapacity);
    const bool roundTrip = parsed && *parsed == *header;
    bytes[20] ^= std::byte{1};
    const bool corruptRejected = !core::parseBlobStoreHeader(bytes, kCapacity);
    std::vector<std::byte> erased(core::kBlobHeaderSize, std::byte{0xFF});
    return expect(roundTrip, "header round trip")
           && expect(corruptRejected, "bit flip rejected")
           && expect(!core::parseBlobStoreHeader(erased, kCapacity), "erased flash")
           && expect(!core::parseBlobStoreHeader(core::serializeBlobStoreHeader(*header),
                                                 0x100000U),
                     "entries beyond capacity");
}

} // namespace

int main()
{
    if (!(crcTest() && layoutTest() && headerRoundTripTest())) {
        return EXIT_FAILURE;
    }
    std::cout << "blob_store_test OK\n";
    return EXIT_SUCCESS;
}
