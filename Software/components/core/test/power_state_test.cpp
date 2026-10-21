/**
 * @file    power_state_test.cpp
 * @brief   Host tests for PowerState JSON and AudioService soft standby.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-21
 */

#include "audio/AudioService.hpp"
#include "core/IDsp.hpp"
#include "core/PowerStateJson.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

/** Records the last master gain and profile master the DSP received. */
class RecordingDsp final : public core::IDsp {
public:
    float lastMasterLeftDb{0.0F};
    float lastProfileMasterDb{0.0F};
    bool failMaster{false};

    [[nodiscard]] std::expected<void, core::DspError> applyProfile(
        const core::AudioProfile& profile) override
    {
        lastProfileMasterDb = profile.masterLeft.value();
        return {};
    }

    [[nodiscard]] std::expected<void, core::DspError> selectSource(
        core::ActiveSource) override
    {
        return {};
    }

    [[nodiscard]] std::expected<void, core::DspError> applyEq(
        const core::EqProfile&) override
    {
        return {};
    }

    [[nodiscard]] std::expected<void, core::DspError> setMasterVolume(
        core::GainDb left, core::GainDb) override
    {
        if (failMaster) {
            return std::unexpected(core::DspError::SafeloadFailed);
        }
        lastMasterLeftDb = left.value();
        return {};
    }

    [[nodiscard]] std::expected<void, core::DspError> setEqBand(
        core::EqBandIndex, core::GainDb, core::FrequencyHz, float) override
    {
        return {};
    }

    [[nodiscard]] std::expected<void, core::DspError> setBeepEnabled(
        bool) override
    {
        return {};
    }

    [[nodiscard]] std::expected<void, core::DspError> setBassBoostLevel(
        core::EnhanceLevel) override
    {
        return {};
    }

    [[nodiscard]] std::expected<void, core::DspError> setStereoSpreadLevel(
        core::EnhanceLevel) override
    {
        return {};
    }

    [[nodiscard]] std::expected<core::AudioLevels, core::DspError>
    readLevels() override
    {
        return core::AudioLevels{};
    }

    [[nodiscard]] std::expected<void, core::DspError> writeRawParam(
        unsigned, float) override
    {
        return {};
    }
};

[[nodiscard]] core::GainDb gain(float db)
{
    return *core::GainDb::tryFromDb(db);
}

[[nodiscard]] bool fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

[[nodiscard]] bool parsePowerState_acceptsOnAndStandby()
{
    const auto on = core::parsePowerStateJson(R"({"state":"on"})");
    const auto standby = core::parsePowerStateJson(R"({"state": "standby"})");
    if (!on || *on != core::PowerState::On) {
        return fail("state=on parse mismatch");
    }
    if (!standby || *standby != core::PowerState::Standby) {
        return fail("state=standby parse mismatch");
    }
    return true;
}

[[nodiscard]] bool parsePowerState_rejectsBadInput()
{
    if (core::parsePowerStateJson(R"({"state":"off"})")) {
        return fail("unknown state should fail");
    }
    if (core::parsePowerStateJson(R"({})")) {
        return fail("missing state should fail");
    }
    if (core::parsePowerStateJson("not json")) {
        return fail("malformed body should fail");
    }
    return true;
}

[[nodiscard]] bool serializePowerState_roundTrips()
{
    const std::string json =
        core::serializePowerStateJson(core::PowerState::Standby);
    if (json != R"({"state":"standby"})") {
        return fail("standby serialisation mismatch");
    }
    const auto back = core::parsePowerStateJson(json);
    return back && *back == core::PowerState::Standby
        ? true
        : fail("standby round trip mismatch");
}

[[nodiscard]] bool standby_mutesAndOnRestoresProfileMaster()
{
    RecordingDsp dsp;
    audio::AudioService service(dsp, nullptr);
    if (!service.setMasterVolume(gain(-6.0F), gain(-6.0F), false)) {
        return fail("setMasterVolume failed");
    }
    if (!service.setPowerState(core::PowerState::Standby)
        || dsp.lastMasterLeftDb != core::GainDb::kMinDb) {
        return fail("standby did not mute master");
    }
    if (!service.setPowerState(core::PowerState::On)
        || dsp.lastMasterLeftDb != -6.0F) {
        return fail("on did not restore master");
    }
    return service.powerState() == core::PowerState::On
        ? true
        : fail("power state not On after wake");
}

[[nodiscard]] bool standby_keepsVolumeChangesMutedUntilOn()
{
    RecordingDsp dsp;
    audio::AudioService service(dsp, nullptr);
    if (!service.setPowerState(core::PowerState::Standby)) {
        return fail("enter standby failed");
    }
    if (!service.setMasterVolume(gain(-3.0F), gain(-3.0F), false)
        || dsp.lastMasterLeftDb != core::GainDb::kMinDb) {
        return fail("volume change leaked through standby");
    }
    core::AudioProfile profile = service.currentProfile();
    profile.masterLeft = gain(-1.0F);
    if (!service.applyProfile(profile, false)
        || dsp.lastProfileMasterDb != core::GainDb::kMinDb) {
        return fail("profile apply leaked master through standby");
    }
    if (!service.setPowerState(core::PowerState::On)
        || dsp.lastMasterLeftDb != -1.0F) {
        return fail("on did not restore the latest requested master");
    }
    return true;
}

[[nodiscard]] bool setPowerState_failureKeepsPreviousState()
{
    RecordingDsp dsp;
    audio::AudioService service(dsp, nullptr);
    dsp.failMaster = true;
    if (service.setPowerState(core::PowerState::Standby)) {
        return fail("DSP failure should be reported");
    }
    return service.powerState() == core::PowerState::On
        ? true
        : fail("state changed despite DSP failure");
}

} // namespace

int main()
{
    const bool ok = parsePowerState_acceptsOnAndStandby()
        && parsePowerState_rejectsBadInput()
        && serializePowerState_roundTrips()
        && standby_mutesAndOnRestoresProfileMaster()
        && standby_keepsVolumeChangesMutedUntilOn()
        && setPowerState_failureKeepsPreviousState();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
