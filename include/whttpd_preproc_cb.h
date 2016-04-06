/*
 * Created by Martin Winkelhofer 02,03/2016
 * W-Dimension / wdim / wdim0 / maarty.w@gmail.com
 *          __   __  __          __
 *  _    __/ /  / /_/ /____  ___/ /
 * | |/|/ / _ \/ __/ __/ _ \/ _  /
 * |__,__/_//_/\__/\__/ .__/\_,_/
 *                   /_/
 * This file is part of WHTTPD - W-Dimension's HTTP server (for ESP8266).
 *
 * WHTTPD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * WHTTPD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with WHTTPD. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __WHTTPD_PREPROC_CB_H__
#define __WHTTPD_PREPROC_CB_H__

#include <espressif/esp_common.h>

//WHTTPD PREPROCESSOR (processing tags in file currently being sent to client) - TAG CALLBACK FUNCTIONS

#define WHTTPD_PP_FOTA_PWD_PREFIX_LEN		8

//----

int8_t ICACHE_FLASH_ATTR cb_get_version(uint8_t IsFirstCall, char* OutputData, uint16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetDone);
int8_t ICACHE_FLASH_ATTR cb_get_err_code(uint8_t IsFirstCall, char* OutputData, uint16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetDone);
int8_t ICACHE_FLASH_ATTR cb_get_err_code_msg(uint8_t IsFirstCall, char* OutputData, uint16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetDone);
//
int8_t ICACHE_FLASH_ATTR cb_flash_get_info(uint8_t IsFirstCall, char* OutputData, uint16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetDone);
int8_t ICACHE_FLASH_ATTR cb_flash_read_all(uint8_t IsFirstCall, char* OutputData, uint16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetDone);
int8_t ICACHE_FLASH_ATTR cb_fota_slot_in_use(uint8_t IsFirstCall, char* OutputData, uint16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetDone);
int8_t ICACHE_FLASH_ATTR cb_fota_slot_free(uint8_t IsFirstCall, char* OutputData, uint16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetDone);
int8_t ICACHE_FLASH_ATTR cb_fota_slot_in_use_info(uint8_t IsFirstCall, char* OutputData, uint16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetDone);
char* ICACHE_FLASH_ATTR whttpd_preproc_fota_get_pwd_prefix(void);
int8_t ICACHE_FLASH_ATTR cb_fota_gen_and_get_pwd_prefix(uint8_t IsFirstCall, char* OutputData, uint16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetDone);
int8_t ICACHE_FLASH_ATTR cb_fota_commit(uint8_t IsFirstCall, char* OutputData, uint16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetDone);
//
int8_t ICACHE_FLASH_ATTR cb_gpio_set_gpio2(uint8_t IsFirstCall, char* OutputData, uint16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetDone);

//----

#endif
