/**
 * @file    embedded_blob_reader_test.cpp
 * @brief   Host tests for EmbeddedBlobReader streaming reads.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/EmbeddedBlobReader.hpp"

#include <array>
#include <cstdlib>
#include <iostream>

namespace {

bool expectEqual(std::size_t actual, std::size_t expected, const char* label)
{
    if (actual == expected) {
        return true;
    }
    std::cerr << label << ": expected " << expected << ", got " << actual
              << '\n';
    return false;
}

} // namespace

/**
 * @brief    main — run EmbeddedBlobReader host tests.
 *
 * @dname    main
 * @return   0 on success, 1 on failure.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
int main()
{
    const std::array<std::byte, 8> source = {
        std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04},
        std::byte{0x05}, std::byte{0x06}, std::byte{0x07}, std::byte{0x08},
    };
    core::EmbeddedBlobReader reader(source.data(), source.size());

    if (!expectEqual(reader.size(), source.size(), "size")) {
        return EXIT_FAILURE;
    }

    std::array<std::byte, 3> chunk = {};
    if (!expectEqual(reader.read(0U, chunk), 3U, "first read length")) {
        return EXIT_FAILURE;
    }
    if (chunk[0] != std::byte{0x01} || chunk[2] != std::byte{0x03}) {
        std::cerr << "first read content mismatch\n";
        return EXIT_FAILURE;
    }

    if (!expectEqual(reader.read(6U, chunk), 2U, "tail read length")) {
        return EXIT_FAILURE;
    }
    if (chunk[0] != std::byte{0x07} || chunk[1] != std::byte{0x08}) {
        std::cerr << "tail read content mismatch\n";
        return EXIT_FAILURE;
    }

    if (!expectEqual(reader.read(source.size(), chunk), 0U, "past end")) {
        return EXIT_FAILURE;
    }

    std::cout << "embedded_blob_reader_test: ok\n";
    return EXIT_SUCCESS;
}
