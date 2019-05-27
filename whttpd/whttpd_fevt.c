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
//#include <freertos/FreeRTOS.h>
#include <espressif/esp_common.h>

#include "whttpd_defs.h"
#include "whttpd_fevt.h"
#include "whttpd_fevt_cb.h"

//WHTTPD SPECAL FILE EVENTS (for specified files/pages do the load related evens)

//-------- whttpd_FEvtItems[] array definition

const whttpd_fevt_item_struct whttpd_FEvtItems[WHTTPD_FEVT_ITEMS_CNT] = {
		//Here we define all files/pages that have special load related evens.
		{.RqF = "fota_result.html",		.func_cb_req_hdr_received = cb_fota_result_html_req_hdr_received,
										.func_cb_req_finished = NULL,
										.func_cb_resp_started = NULL,
										.func_cb_resp_finished = NULL}
		//
		// ... you can add your own items here
		//
		//! if you change the number of items here, don't forget to change WHTTPD_FEVT_ITEMS_CNT respectively !
};

//--------

uint8_t ICACHE_FLASH_ATTR whttpd_fevt_get_item_idx(char* RqF){
//Search whttpd_FEvtItems[] for item where whttpd_FEvtItems[].RqF == RqF.
	if(RqF==NULL) return WHTTPD_FEVT_INVALID_ITEM_INDEX;
	uint8_t FEvtItemIdx;
	for(FEvtItemIdx=0;FEvtItemIdx<WHTTPD_FEVT_ITEMS_CNT;FEvtItemIdx++){ //go through all whttpd_FEvtItems[]
		if(strcasecmp(whttpd_FEvtItems[FEvtItemIdx].RqF, RqF)==0) return FEvtItemIdx;
	}
	return WHTTPD_FEVT_INVALID_ITEM_INDEX;
}

void ICACHE_FLASH_ATTR whttpd_fevt_call_cb_req_hdr_received(uint8_t FEvtItemIdx){
	if((FEvtItemIdx==WHTTPD_FEVT_INVALID_ITEM_INDEX)||(FEvtItemIdx>=WHTTPD_FEVT_ITEMS_CNT)||(whttpd_FEvtItems[FEvtItemIdx].func_cb_req_hdr_received==NULL)) return;
	whttpd_FEvtItems[FEvtItemIdx].func_cb_req_hdr_received();
}

void ICACHE_FLASH_ATTR whttpd_fevt_call_cb_req_finished(uint8_t FEvtItemIdx){
	if((FEvtItemIdx==WHTTPD_FEVT_INVALID_ITEM_INDEX)||(FEvtItemIdx>=WHTTPD_FEVT_ITEMS_CNT)||(whttpd_FEvtItems[FEvtItemIdx].func_cb_req_finished==NULL)) return;
	whttpd_FEvtItems[FEvtItemIdx].func_cb_req_finished();
}

void ICACHE_FLASH_ATTR whttpd_fevt_call_cb_resp_started(uint8_t FEvtItemIdx){
	if((FEvtItemIdx==WHTTPD_FEVT_INVALID_ITEM_INDEX)||(FEvtItemIdx>=WHTTPD_FEVT_ITEMS_CNT)||(whttpd_FEvtItems[FEvtItemIdx].func_cb_resp_started==NULL)) return;
	whttpd_FEvtItems[FEvtItemIdx].func_cb_resp_started();
}

void ICACHE_FLASH_ATTR whttpd_fevt_call_cb_resp_finished(uint8_t FEvtItemIdx){
	if((FEvtItemIdx==WHTTPD_FEVT_INVALID_ITEM_INDEX)||(FEvtItemIdx>=WHTTPD_FEVT_ITEMS_CNT)||(whttpd_FEvtItems[FEvtItemIdx].func_cb_resp_finished==NULL)) return;
	whttpd_FEvtItems[FEvtItemIdx].func_cb_resp_finished();
}
