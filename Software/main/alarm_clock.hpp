/**
 * @file    alarm_clock.hpp
 * @brief   Alarm table (turn on / turn off at set times) and sleep timer.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */
#pragma once

namespace audio {
class AudioService;
}
namespace bluetooth {
class BluetoothService;
}
namespace integration {
class IntegrationService;
}
namespace webradio {
class WebRadioService;
}

namespace alarm_clock {

/** Services the alarm task drives; all must outlive the process. */
struct Services {
    audio::AudioService& audio;             ///< Power state and master gain.
    integration::IntegrationService& integration; ///< Preset recall.
    webradio::WebRadioService& webRadio;    ///< Web radio on/off.
    bluetooth::BluetoothService& bluetooth; ///< Speaker reconnect on wake.
};

/**
 * @brief    start — sync the clock and run the alarm table.
 *
 * Starts SNTP (pool.ntp.org, Europe/Rome time zone), loads the table from
 * NVS (namespace "alarms"), plugs GET/PUT /api/alarms and
 * GET/POST /api/system/sleep into the web server and spawns the "alarm"
 * task, which checks the table once a second. A TurnOn row leaves standby,
 * plays its preset or web radio URL and fades the master gain in over
 * 30 s; a TurnOff row, the sleep timer and a TurnOn row's duration put the
 * radio in standby and stop the web radio.
 *
 * Call once, after the network is up.
 *
 * @dname    start
 * @param    services  Services to drive (static storage).
 * @return   false if the task could not be created.
 * @pubstate creates the "alarm" task; registers the alarm API.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] bool start(const Services& services);

} // namespace alarm_clock
