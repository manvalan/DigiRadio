/**
 * @file    Adau1452Dsp.hpp
 * @brief   core::IDsp over Adau1452Driver.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-31
 */
#pragma once

#include "adau1452/Adau1452Driver.hpp"

#include "core/IDsp.hpp"

namespace adau1452 {

/**
 * @brief    Adau1452Dsp — maps driver errors onto core::DspError.
 *
 * @dname    Adau1452Dsp
 * @param    driver  Driver with static lifetime.
 * @return   n/a (type)
 * @pubstate holds a reference only.
 *
 * @author   Michele Bigi
 * @date     2026-10-31
 */
class Adau1452Dsp final : public core::IDsp {
public:
    explicit Adau1452Dsp(Adau1452Driver& driver) : driver_(driver) {}

    [[nodiscard]] std::expected<void, core::DspError> applyProfile(
        const core::AudioProfile& profile) override;
    [[nodiscard]] std::expected<void, core::DspError> selectSource(
        core::ActiveSource source) override;
    [[nodiscard]] std::expected<void, core::DspError> applyEq(
        const core::EqProfile& eq) override;
    [[nodiscard]] std::expected<void, core::DspError> setMasterVolume(
        core::GainDb left, core::GainDb right) override;
    [[nodiscard]] std::expected<void, core::DspError> setEqBand(
        core::EqBandIndex band, core::GainDb gain, core::FrequencyHz center,
        float q) override;
    [[nodiscard]] std::expected<void, core::DspError> setBeepEnabled(
        bool enabled) override;
    [[nodiscard]] std::expected<void, core::DspError> setBassBoostLevel(
        core::EnhanceLevel level) override;
    [[nodiscard]] std::expected<void, core::DspError> setStereoSpreadLevel(
        core::EnhanceLevel level) override;
    [[nodiscard]] std::expected<void, core::DspError> writeRawParam(
        unsigned address, float value) override;
    [[nodiscard]] std::expected<core::AudioLevels, core::DspError> readLevels()
        override;

private:
    Adau1452Driver& driver_;
};

} // namespace adau1452
