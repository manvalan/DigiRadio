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

#include "freertos/FreeRTOS.h"

namespace tuner {
class TunerService;
}

namespace dab_slideshow {

/**
 * @brief    start — drain DAB data services and serve the slideshow.
 *
 * Spawns the "dab_data" task, which every 150 ms (outside spectrum
 * sweeps) empties the tuner's GET_DIGITAL_SERVICE_DATA queue: DLS/DL Plus
 * stay in the tuner, MOT data groups go to core::MotSlideAssembler. Slides
 * are written to a flash area outside the partition table (0xA60000,
 * 1 MB, sixteen 64 KB slots used in turn to spread wear) and only while a
 * client has looked at the slideshow or the DAB status in the last five
 * minutes, so an unattended radio does not wear the flash. Plugs
 * GET /api/tuner/slide and the status "slide_id" into the web server.
 *
 * @dname    start
 * @param    tuner  Tuner service (static storage).
 * @return   The task handle, or nullptr if it could not be created.
 * @pubstate registers the slide flash area and the slide API.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] TaskHandle_t start(tuner::TunerService& tuner);

} // namespace dab_slideshow
