/**
 * @file    dab_follower_test.cpp
 * @brief   Host tests for DAB linkage parsers and DabServiceFollower.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-21
 */

#include "core/DabLinkage.hpp"
#include "core/DabServiceFollower.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

constexpr std::uint32_t kSid = 0x5201U;
constexpr std::uint16_t kEidHere = 0x5001U;
constexpr std::uint16_t kEidOther = 0x5002U;
constexpr std::uint16_t kPi = 0x5201U;

[[nodiscard]] bool fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

void put16(std::vector<std::uint8_t>& b, std::uint16_t v)
{
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v >> 8U));
}

void put32(std::vector<std::uint8_t>& b, std::uint32_t v)
{
    put16(b, static_cast<std::uint16_t>(v));
    put16(b, static_cast<std::uint16_t>(v >> 16U));
}

/* AN649 0xBF entry: ID(4) FREQ(4) FREQ_INDEX(1) RNM(1) CONT(1) CTRL(1). */
void putFi(std::vector<std::uint8_t>& b, std::uint32_t id, std::uint32_t hz,
           std::uint8_t index, std::uint8_t rnm)
{
    put32(b, id);
    put32(b, hz);
    b.insert(b.end(), {index, rnm, 0U, 0U});
}

[[nodiscard]] std::vector<std::uint8_t> freqInfoReply(bool lengthIsBytes)
{
    std::vector<std::uint8_t> b;
    put32(b, lengthIsBytes ? 36U : 3U);
    putFi(b, kEidHere, 225648000U, 25U, core::kFiRnmDabEnsemble);
    putFi(b, kEidOther, 227360000U, 26U, core::kFiRnmDabEnsemble);
    putFi(b, kPi, 98400000U, core::kFiNoFreqIndex, core::kFiRnmFmRds);
    b.insert(b.end(), 24U, 0xEEU); // junk past the list (SPI over-read)
    return b;
}

/* AN649 0xB7: SIZE(2) NUM_LINKSETS(1) X(1), then per set LSN(2) flags(1)
 * NUM_LINKS(1) LINKID(4)×n. flags: ACTIVE b7, LINK_TYPE b5:4, HARD b1. */
[[nodiscard]] std::vector<std::uint8_t> linkingReply()
{
    std::vector<std::uint8_t> b;
    put16(b, 0U);
    b.insert(b.end(), {2U, 0U});
    put16(b, 0x123U);
    b.insert(b.end(), {0x80U | 0x10U | 0x02U, 1U}); // active, RDS PI, hard
    put32(b, kPi);
    put16(b, 0x124U);
    b.insert(b.end(), {0x80U | 0x10U, 1U});         // soft RDS PI link
    put32(b, 0x5A01U);
    return b;
}

[[nodiscard]] bool parserTest()
{
    for (const bool bytes : {false, true}) {
        const auto fi = core::parseDabFreqInfo(freqInfoReply(bytes));
        if (!fi || fi->size() != 3U || (*fi)[1].freqIndex != 26U
            || (*fi)[2].frequencyHz != 98400000U) {
            return fail("FI list must parse as entry count or byte length");
        }
    }
    const auto sets = core::parseDabServiceLinking(linkingReply());
    if (!sets || sets->size() != 2U || !(*sets)[0].hard || (*sets)[1].hard
        || (*sets)[0].type != core::DabLinkType::RdsPi
        || (*sets)[0].ids.at(0) != kPi || (*sets)[0].lsn != 0x123U) {
        return fail("linkage sets mis-parsed");
    }
    std::vector<std::uint8_t> oe;
    put16(oe, 4U);
    oe.insert(oe.end(), {1U, 0U});
    put16(oe, kEidOther);
    const auto eids = core::parseDabOeServices(oe);
    if (!eids || eids->size() != 1U || eids->at(0) != kEidOther) {
        return fail("OE EIds mis-parsed");
    }
    std::vector<std::uint8_t> cut = linkingReply();
    cut.resize(9U);
    return !core::parseDabServiceLinking(cut) || fail("truncated reply must fail");
}

[[nodiscard]] core::TunerStatus dabStatus(std::uint8_t fic, bool locked = true)
{
    core::TunerStatus s{};
    s.booted = true;
    s.band = core::TunerBand::Dab;
    s.locked = locked;
    s.dabFreqIndex = 25U;
    s.dabFicQuality = fic;
    s.dabPlayingServiceId = kSid;
    return s;
}

[[nodiscard]] core::DabLinkage linkage()
{
    core::DabLinkage l;
    l.sets = *core::parseDabServiceLinking(linkingReply());
    l.otherEnsembles = {kEidOther};
    l.frequencies = *core::parseDabFreqInfo(freqInfoReply(false));
    return l;
}

/* Other ensemble with the same SId first, then FM of the hard-linked PI;
 * the current ensemble and soft links are never candidates. */
[[nodiscard]] bool candidatesTest()
{
    core::DabServiceFollower f;
    f.observe(dabStatus(80U), 0U);
    if (!f.needsLinkage(0U)) {
        return fail("good signal without linkage must read it");
    }
    f.setLinkage(linkage(), 0U);
    const auto c = f.candidates();
    if (c.size() != 2U || c[0].fm || c[0].dabFreqIndex != 26U || !c[1].fm
        || c[1].fmFrequency->value() != 98400U || c[1].pi != kPi) {
        return fail("expected ensemble 26 then FM 98.4 (hard PI)");
    }
    return !f.needsLinkage(30000U) || fail("fresh linkage needs no re-read");
}

/* Hunt only after three weak snapshots, settle and cooldown. */
[[nodiscard]] bool huntTimingTest()
{
    core::DabServiceFollower f;
    f.observe(dabStatus(80U), 0U);
    f.setLinkage(linkage(), 0U);
    f.observe(dabStatus(10U), 9000U);
    f.observe(dabStatus(10U), 12000U);
    if (f.shouldHunt(13000U)) {
        return fail("two weak snapshots must not hunt");
    }
    f.observe(dabStatus(0U, false), 15000U);
    if (!f.shouldHunt(15000U)) {
        return fail("sustained loss must hunt");
    }
    f.finishHunt(15000U);
    for (std::uint32_t t = 18000U; t <= 24000U; t += 3000U) {
        f.observe(dabStatus(10U), t);
    }
    if (f.shouldHunt(30000U)) {
        return fail("cooldown must hold");
    }
    f.observe(dabStatus(10U), 45000U);
    return f.shouldHunt(46000U) || fail("hunt again after cooldown");
}

/* A new service clears linkage; no linkage means no hunt. */
[[nodiscard]] bool newServiceTest()
{
    core::DabServiceFollower f;
    f.observe(dabStatus(80U), 0U);
    f.setLinkage(linkage(), 0U);
    auto other = dabStatus(80U);
    other.dabPlayingServiceId = 0x5202U;
    f.observe(other, 1000U);
    return f.candidates().empty() || fail("new service must drop linkage");
}

} // namespace

int main()
{
    const bool ok = parserTest() && candidatesTest() && huntTimingTest()
                 && newServiceTest();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
