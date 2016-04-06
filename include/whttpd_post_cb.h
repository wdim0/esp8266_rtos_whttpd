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
#ifndef __WHTTPD_POST_CB_H__
#define __WHTTPD_POST_CB_H__

#include <espressif/esp_common.h>

#include "whttpd_config.h"

//WHTTPD POST DATA HANDLING (processing POST data currently being received from client) - POST DATA CALLBACK FUNCTIONS

#ifdef DO_DEBUG_WHTTPD_FOTA
#define DBG_WHTTPD_FOTA(...)	printf( "whttpd: FOTA: "__VA_ARGS__ )
#else
#define DBG_WHTTPD_FOTA
#endif

#define WHTTPD_POST_FOTA_LOG_MALLOC_STEP	128
#define WHTTPD_POST_FOTA_LOG_MAX_LINE_LEN	300

typedef enum {
	WHTTPD_POST_FOTA_LOG_NORMAL = 0, WHTTPD_POST_FOTA_LOG_ACCENT, WHTTPD_POST_FOTA_LOG_OK, WHTTPD_POST_FOTA_LOG_ERR
} whttpd_post_fota_log_type_enum;

#define WHTTPD_POST_FOTA_LOG_HTML_NEWLINE			"<br>\r\n"
#define WHTTPD_POST_FOTA_LOG_HTML_NORMAL_PREFIX		""
#define WHTTPD_POST_FOTA_LOG_HTML_NORMAL_SUFFIX		""
#define WHTTPD_POST_FOTA_LOG_HTML_ACCENT_PREFIX		"<b>"
#define WHTTPD_POST_FOTA_LOG_HTML_ACCENT_SUFFIX		"</b>"
#define WHTTPD_POST_FOTA_LOG_HTML_OK_PREFIX			"<span class=\"fota_ok\">"
#define WHTTPD_POST_FOTA_LOG_HTML_OK_SUFFIX			"</span>"
#define WHTTPD_POST_FOTA_LOG_HTML_ERR_PREFIX		"<span class=\"fota_err\">"
#define WHTTPD_POST_FOTA_LOG_HTML_ERR_SUFFIX		"</span>"

//----

int8_t ICACHE_FLASH_ATTR cb_fota_flash_data(uint8_t Flags, uint8_t* InputData, uint16_t Bytes);
int8_t ICACHE_FLASH_ATTR cb_fota_flash_data_md5(uint8_t Flags, uint8_t* InputData, uint16_t Bytes);
int8_t ICACHE_FLASH_ATTR cb_fota_pwd(uint8_t Flags, uint8_t* InputData, uint16_t Bytes);
//
void ICACHE_FLASH_ATTR fota_init(void);
char** ICACHE_FLASH_ATTR fota_commit(void);

//----

#endif
