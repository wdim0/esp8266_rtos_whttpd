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
#ifndef __WHTTPD_FEVT_CB_H__
#define __WHTTPD_FEVT_CB_H__

#include <espressif/esp_common.h>

#include "whttpd_config.h"

//WHTTPD SPECAL FILE EVENTS (for specified files/pages do the load related evens) - CALLBACK FUNCTIONS

//----

void ICACHE_FLASH_ATTR cb_fota_result_html_req_hdr_received(void);

//----

#endif
