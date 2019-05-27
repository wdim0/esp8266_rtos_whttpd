/*
 * Created by Martin Winkelhofer 02,03/2016
 * W-Dimension / wdim / wdim0 / winkelhofer.m@gmail.com / https://github.com/wdim0
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
#ifndef __WHTTPD_POST_H__
#define __WHTTPD_POST_H__

#include <espressif/esp_common.h>

//WHTTPD POST DATA HANDLING (processing POST data currently being received from client)

#define WHTTPD_POST_CBFLAG_IS_FIRST_DATA_BLOCK		(1<<0) //signal to POST data callback function that this is the first block of POST data (may be more later)
#define WHTTPD_POST_CBFLAG_IS_LAST_DATA_BLOCK		(1<<1) //signal to POST data callback function that this is the last block of POST data

typedef struct {
	const char* FormInputName; //name of form input (using <form><input name="...">)
	int8_t (*func_cb)(uint8_t Flags, uint8_t* InputData, uint16_t Bytes);
	/* POST data callback function should behave like this:
	 * input:
	 * - Flags - combination of WHTTPD_POST_CBFLAG_* (see definitions for details)
	 * - InputData - pointer to data buffer
	 * - Bytes - how many bytes are in the buffer
	 * output:
	 * - return 0 for success, less than 0 for error.
	 * This call is also valid: func_cb(WHTTPD_POST_CBFLAG_IS_LAST_DATA_BLOCK, NULL, 0).
	 */
	//
} whttpd_post_item_struct;

#define WHTTPD_POST_INVALID_ITEM_INDEX		255

//----

uint8_t ICACHE_FLASH_ATTR whttpd_post_get_item_idx(char* FormInputName);
int8_t ICACHE_FLASH_ATTR whttpd_post_call_cb(uint8_t PostItemIdx, uint8_t Flags, uint8_t* InputData, uint16_t Bytes);

//----

#endif
