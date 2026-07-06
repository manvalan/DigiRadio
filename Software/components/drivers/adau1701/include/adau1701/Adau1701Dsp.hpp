/**
 * @file    Adau1701Dsp.hpp
 * @brief   core::IDsp adapter over Adau1701Driver.
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

#include "adau1701/Adau1701Driver.hpp"

#include "core/IDsp.hpp"

namespace adau1701 {

/**
 * @brief    Adau1701Dsp — maps core::IDsp to Adau1701Driver safeload API.
 *
 * @dname    Adau1701Dsp
 * @return   n/a (type)
 * @pubstate Borrows Adau1701Driver for the process lifetime.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class Adau1701Dsp final : public core::IDsp {
public:
    /**
     * @brief    Adau1701Dsp — bind to the board driver instance.
     *
     * @dname    Adau1701Dsp
     * @param    driver  Booted ADAU1701 driver (must outlive this adapter).
     * @pubstate stores driver reference.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    explicit Adau1701Dsp(Adau1701Driver& driver);

    [[nodiscard]] std::expected<void, core::DspError> applyProfile(
        const core::AudioProfile& profile) override;

    [[nodiscard]] std::expected<void, core::DspError> applyMixer(
        const core::MixerState& mixer) override;

    [[nodiscard]] std::expected<void, core::DspError> applyEq(
        const core::EqProfile& eq) override;

    [[nodiscard]] std::expected<void, core::DspError> setInputVolume(
        core::MixSource source, core::GainDb left, core::GainDb right) override;

    [[nodiscard]] std::expected<void, core::DspError> setMasterVolume(
        core::GainDb left, core::GainDb right) override;

    [[nodiscard]] std::expected<void, core::DspError> setEqBand(
        core::EqBandIndex band, core::GainDb gain, core::FrequencyHz center,
        float q) override;

private:
    [[nodiscard]] static core::DspError mapError(Adau1701Error error) noexcept;

    Adau1701Driver& driver_;
};

} // namespace adau1701
