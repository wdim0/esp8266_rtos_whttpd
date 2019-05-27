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
#include <ssl/ssl_crypto.h>

#include "whttpd_defs.h"
#include "whttpd_post.h"
#include "whttpd_post_cb.h"

//WHTTPD POST DATA HANDLING (processing POST data currently being received from client) - MAIN

//-------- whttpd_PostItems[] array definition

const whttpd_post_item_struct whttpd_PostItems[WHTTPD_POST_ITEMS_CNT] = {
		//Here we define all POST data names (name of form input (using <form><input name="...">)) that will WHTTPD know and it related callback functions.
		{.FormInputName = "fota_flash_data",		.func_cb = cb_fota_flash_data},
		{.FormInputName = "fota_flash_data_md5",	.func_cb = cb_fota_flash_data_md5},
		{.FormInputName = "fota_pwd",				.func_cb = cb_fota_pwd}
		//
		// ... you can add your own items here
		//
		//! if you change the number of items here, don't forget to change WHTTPD_POST_ITEMS_CNT respectively !
};

//--------

uint8_t ICACHE_FLASH_ATTR whttpd_post_get_item_idx(char* FormInputName){
//Search whttpd_PostItems[] for item where whttpd_PostItems[].FormInputName == FormInputName.
	if(FormInputName==NULL) return WHTTPD_POST_INVALID_ITEM_INDEX;
	uint8_t PostItemIdx;
	for(PostItemIdx=0;PostItemIdx<WHTTPD_POST_ITEMS_CNT;PostItemIdx++){ //go through all whttpd_PostItems[]
		if(strcasecmp(whttpd_PostItems[PostItemIdx].FormInputName, FormInputName)==0) return PostItemIdx;
	}
	return WHTTPD_POST_INVALID_ITEM_INDEX;
}

int8_t ICACHE_FLASH_ATTR whttpd_post_call_cb(uint8_t PostItemIdx, uint8_t Flags, uint8_t* InputData, uint16_t Bytes){
	if((PostItemIdx==WHTTPD_POST_INVALID_ITEM_INDEX)||(PostItemIdx>=WHTTPD_POST_ITEMS_CNT)) return -100; //-1..-99 space for legal error codes from func_cb(...)
	return whttpd_PostItems[PostItemIdx].func_cb(Flags, InputData, Bytes);
}
