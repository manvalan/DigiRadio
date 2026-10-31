/**
 * @file    Adau1452Eeprom.hpp
 * @brief   Writes the self boot image into the 25AA1024 behind the ADAU1452.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * The EEPROM hangs only on the DSP's master port. A host cannot drive those
 * pins through registers: per data sheet (Multipurpose Pins) an MPx output
 * carries what the DSP program sends it, updated once per sample. So a small
 * writer program runs in the DSP and passes four cells to MP0-MP3; this class
 * clocks the 25AA1024 by writing those cells.
 *
 * @author  Michele Bigi
 * @date    2026-10-31
 */
#pragma once

#include "adau1452/Adau1452Driver.hpp"

#include <cstddef>

namespace adau1452 {

/**
 * @brief    EepromSync — what syncSelfBootEeprom() found or did.
 *
 * @dname    EepromSync
 * @return   n/a (type)
 * @pubstate n/a
 *
 * @author   Michele Bigi
 * @date     2026-10-31
 */
enum class EepromSync {
    UpToDate,      ///< The image last written is the one in this app.
    Written,       ///< Written and read back; reboot the DSP to run it.
    NoImage,       ///< dsp/adau1452_selfboot.bin was not in this build.
    NoWriter,      ///< The writer program or its cells were not in this build.
    Failed,        ///< I/O failure or read-back mismatch.
};

/**
 * @brief    syncSelfBootEeprom — write the image once, verify, remember it.
 *
 * @dname    syncSelfBootEeprom
 * @param    driver  Reset is pulsed and the writer program replaces whatever
 *                   ran, so audio stops; call boot() after a Written.
 * @return   Outcome; never leaves a half-written image marked as good.
 * @pubstate reads and writes the CRC of the last good image in NVS.
 *
 * @author   Michele Bigi
 * @date     2026-10-31
 */
[[nodiscard]] EepromSync syncSelfBootEeprom(Adau1452Driver& driver);

/**
 * @brief    dumpSelfBootEeprom — print the EEPROM to the console as hex.
 *
 * @dname    dumpSelfBootEeprom
 * @param    driver  As for syncSelfBootEeprom(); call boot() afterwards.
 * @param    bytes   How much to read from address 0.
 * @return   true when all of it was read.
 * @pubstate none. Lets the image SigmaStudio+ wrote over USBi on the first
 *           board become dsp/adau1452_selfboot.bin (tools/eeprom_log_to_bin.py).
 *
 * @author   Michele Bigi
 * @date     2026-10-31
 */
[[nodiscard]] bool dumpSelfBootEeprom(Adau1452Driver& driver, std::size_t bytes);

} // namespace adau1452
