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
//#include <freertos/FreeRTOS.h>
#include <espressif/esp_common.h>
#include <ssl/ssl_crypto.h> //to have access to MD5

#include "whttpd_defs.h"
#include "whttpd_fevt.h"
#include "whttpd_fevt_cb.h"
#include "whttpd_post_cb.h"

//WHTTPD SPECAL FILE EVENTS (for specified files/pages do the load related evens) - CALLBACK FUNCTIONS
//! when you change code here, don't forget to edit whttpd_FEvtItems[] array definition in whttpd_fevt.c respectively

void ICACHE_FLASH_ATTR cb_fota_result_html_req_hdr_received(void){
	fota_init(); //this is defined in whttpd_post_cb.c (all FOTA stuff at one place)
}
