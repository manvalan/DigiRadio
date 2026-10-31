/* 
 * File:	C:/Users/mikbi/Documents/Allegati/Desktop/DigiRadio-Adau/ADAU1452/eeprom_writer/export\DigiRadio_EepromWriter_ADAU145xSchematic_0_PARAM.h
 * Created:	sabato, 31 ottobre 2026 22:43 
 * Description:	ADAU145xSchematic_0 parameter RAM definitions.
 * 
 * This software is distributed in the hope that it will be useful, 
 * but is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR 
 * CONDITIONS OF ANY KIND, without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 * 
 * This software may only be used to program products purchased from 
 * Analog Devices for incorporation by you into audio products that 
 * are intended for resale to audio product end users.This software 
 * may not be distributed whole or in any part to third parties. 
 * 
 * Copyright © 2026 Analog Devices, Inc. All rights reserved.
 */
#ifndef __DIGIRADIO_EEPROMWRITER_ADAU145XSCHEMATIC_0_PARAM_H__
#define __DIGIRADIO_EEPROMWRITER_ADAU145XSCHEMATIC_0_PARAM_H__

/* Module Safeload - Schematic Params */
#define MOD_SAFELOAD_COUNT                  7
#define MOD_SAFELOAD_SCHEMATIC              "ADAU145xSchematic_0"
#define MOD_SAFELOAD_DATA_SAFELOAD0_ADDR    24576
#define MOD_SAFELOAD_DATA_SAFELOAD1_ADDR    24577
#define MOD_SAFELOAD_DATA_SAFELOAD2_ADDR    24578
#define MOD_SAFELOAD_DATA_SAFELOAD3_ADDR    24579
#define MOD_SAFELOAD_DATA_SAFELOAD4_ADDR    24580
#define MOD_SAFELOAD_ADDR_SAFELOAD_ADDR     24581
#define MOD_SAFELOAD_NUM_SAFELOAD_ADDR      24582

/* Module Cell_Ss - DC Input */
#define MOD_CELL_SS_COUNT                  1
#define MOD_CELL_SS_SCHEMATIC              "ADAU145xSchematic_0"
#define MOD_CELL_SS_DCVALUE_ADDR           23
#define MOD_CELL_SS_DCVALUE_FIXPT          0x01000000
#define MOD_CELL_SS_DCVALUE_VALUE          SIGMASTUDIOTYPE_8_24_CONVERT(1)
#define MOD_CELL_SS_DCVALUE_TYPE           SIGMASTUDIOTYPE_8_24
#define MOD_CELL_SS_DCVALUE_DATA_MEMORY    "DM0"
#define MOD_CELL_SS_DCVALUE_DATA_MEMORY    "DM0"


/* Module Cell_Mosi - DC Input */
#define MOD_CELL_MOSI_COUNT                  1
#define MOD_CELL_MOSI_SCHEMATIC              "ADAU145xSchematic_0"
#define MOD_CELL_MOSI_DCVALUE_ADDR           22
#define MOD_CELL_MOSI_DCVALUE_FIXPT          0x01000000
#define MOD_CELL_MOSI_DCVALUE_VALUE          SIGMASTUDIOTYPE_8_24_CONVERT(1)
#define MOD_CELL_MOSI_DCVALUE_TYPE           SIGMASTUDIOTYPE_8_24
#define MOD_CELL_MOSI_DCVALUE_DATA_MEMORY    "DM0"
#define MOD_CELL_MOSI_DCVALUE_DATA_MEMORY    "DM0"


/* Module Cell_Sclk - DC Input */
#define MOD_CELL_SCLK_COUNT                  1
#define MOD_CELL_SCLK_SCHEMATIC              "ADAU145xSchematic_0"
#define MOD_CELL_SCLK_DCVALUE_ADDR           21
#define MOD_CELL_SCLK_DCVALUE_FIXPT          0x01000000
#define MOD_CELL_SCLK_DCVALUE_VALUE          SIGMASTUDIOTYPE_8_24_CONVERT(1)
#define MOD_CELL_SCLK_DCVALUE_TYPE           SIGMASTUDIOTYPE_8_24
#define MOD_CELL_SCLK_DCVALUE_DATA_MEMORY    "DM0"
#define MOD_CELL_SCLK_DCVALUE_DATA_MEMORY    "DM0"



/* Module Cell_Miso - DSP Readback */
#define MOD_CELL_MISO_COUNT                1
#define MOD_CELL_MISO_SCHEMATIC            "ADAU145xSchematic_0"
#define MOD_CELL_MISO_VALUE_ADDR           20
#define MOD_CELL_MISO_VALUE_FIXPT          0x01000000
#define MOD_CELL_MISO_VALUE_VALUE          SIGMASTUDIOTYPE_8_24_CONVERT(1)
#define MOD_CELL_MISO_VALUE_TYPE           SIGMASTUDIOTYPE_8_24
#define MOD_CELL_MISO_VALUE_DATA_MEMORY    "DM0"
#define MOD_CELL_MISO_VALUE_DATA_MEMORY    "DM0"

#endif
