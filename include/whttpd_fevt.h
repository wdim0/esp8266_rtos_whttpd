/*
 * Created by Martin Winkelhofer 02,03/2016
 * W-Dimension / wdim / maarty.w@gmail.com
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
#ifndef __WHTTPD_FEVT_H__
#define __WHTTPD_FEVT_H__

#include <espressif/esp_common.h>

//WHTTPD SPECAL FILE EVENTS (for specified files/pages do the load related evens)

typedef struct {
	const char* RqF; //name of requested file/page
	void (*func_cb_req_hdr_received)(void); //callback function that will be called when request for RqF has just been received (right after request header is parsed)
	void (*func_cb_req_finished)(void); //callback function that will be called when we've received all the data in the request (if request was POST, after all POST data are received and processed)
	void (*func_cb_resp_started)(void); //callback function that will be called before we respond to client
	void (*func_cb_resp_finished)(void); //callback function that will be called when we finished our response to client
} whttpd_fevt_item_struct;

#define WHTTPD_FEVT_INVALID_ITEM_INDEX		255

//----

uint8_t ICACHE_FLASH_ATTR whttpd_fevt_get_item_idx(char* RqF);
void ICACHE_FLASH_ATTR whttpd_fevt_call_cb_req_hdr_received(uint8_t FEvtItemIdx);
void ICACHE_FLASH_ATTR whttpd_fevt_call_cb_req_finished(uint8_t FEvtItemIdx);
void ICACHE_FLASH_ATTR whttpd_fevt_call_cb_resp_started(uint8_t FEvtItemIdx);
void ICACHE_FLASH_ATTR whttpd_fevt_call_cb_resp_finished(uint8_t FEvtItemIdx);

//----

#endif
