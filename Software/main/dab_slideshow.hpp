/**
 * @file    dab_slideshow.hpp
 * @brief   DAB data pump (DLS, MOT slideshow) and slide flash store.
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

namespace tuner {
class TunerService;
}

namespace dab_slideshow {

/**
 * @brief    start — set up the slide store and the slideshow API.
 *
 * Registers the slide flash area outside the partition table (0xA60000,
 * 1 MB, sixteen 64 KB slots used in turn to spread wear) and plugs
 * GET /api/tuner/slide and the status "slide_id" into the web server.
 * Slides are written only while a client has looked at the slideshow or
 * the DAB status in the last five minutes, so an unattended radio does
 * not wear the flash. Call once at boot, before poll().
 *
 * @dname    start
 * @param    tuner  Tuner service (static storage).
 * @return   false when the flash area cannot be registered (DLS still
 *           works through poll(); there is no slideshow).
 * @pubstate registers the slide flash area and the slide API.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] bool start(tuner::TunerService& tuner);

/**
 * @brief    poll — drain the tuner's DAB data services once.
 *
 * Empties the GET_DIGITAL_SERVICE_DATA queue: DLS/DL Plus stay in the
 * tuner, MOT data groups go to core::MotSlideAssembler. Skipped during a
 * spectrum sweep and while the ESP32 feeds the speaker (web radio, phone
 * audio). Runs every 150 ms from the af_follow task (0.12.1: no task of
 * its own, which saved its 8 KB stack for the web radio).
 *
 * @dname    poll
 * @pubstate reads the tuner under its lock; may write slide flash.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
void poll();

} // namespace dab_slideshow
