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
#ifndef __WHTTPD_CONFIG_H__
#define __WHTTPD_CONFIG_H__

//WHTTPD (W-Dimension's HTTP server) - main configuration

//#define DO_DEBUG_WHTTPD					//uncomment this to output main level debug msgs (working of main loop in whttpd_main_task()) on TxD
//#define DO_DEBUG_WHTTPD_VERBOSE			//uncomment this to output verbose level debug msgs (working of main loop in whttpd_main_task()) on TxD
//#define DO_DEBUG_WHTTPD_PREPROC			//uncomment this to output preprocessor debug msgs on TxD
//#define DO_DEBUG_WHTTPD_RECV_DATA			//uncomment this to output received HTTP data on TxD
//#define DO_DEBUG_WHTTPD_SENT_DATA			//uncomment this to output sent HTTP data on TxD
//#define DO_DEBUG_WHTTPD_FOTA				//uncomment this to output log lines of FOTA firmware upgrade process on TxD

#define WHTTPD_STACK_SIZE					1024 //[bytes] - for RTOS task creation
#define WHTTPD_INIT_RETRY_MAX_CNT			10
#define WHTTPD_INIT_RETRY_DELAY				(1000 / portTICK_RATE_MS) //([ms] / portTICK_RATE_MS)
#define WHTTPD_PORT							80
#define WHTTPD_LISTEN_BACKLOG				6 //see comment above lwip_listen(...) in whttpd.c
#define WHTTPD_MAX_CONNECTIONS				8
#define WHTTPD_RECV_BUF_LEN					1024 //[bytes]
#define WHTTPD_SEND_BUF_LEN					1024 //[bytes] !max 4096 (we have "000" placeholder for chunk size => max. "FFF") !make sure that WHTTPD_SEND_BUF_LEN is big enough to accommodate sprintf-ed response header + WHTTPD_FAILSAFE_404_CHNK_BODY
#define WHTTPD_MAINLOOP_DELAY				(10 / portTICK_RATE_MS) //([ms] / portTICK_RATE_MS)
#define WHTTPD_SLOTLOOP_DELAY				(5 / portTICK_RATE_MS) //([ms] / portTICK_RATE_MS)
#define WHTTPD_MAINLOOP_CONN_IDLE_TIMEOUT	400 //[x WHTTPD_MAINLOOP_DELAY]
#define WHTTPD_VER							"WHTTPD/1.0 (RTOS on ESP8266)"
#define WHTTPD_RCA_OUTPUTBUF_MALLOC_STEP	32
#define WHTTPD_DEFAULT_PAGE					"index.html"
//
#define WHTTPD_FOTA_UPGRADE_PWD				"abc" //password for FOTA upgrade - set this to something different
//
#define WHTTPD_RCA_ITEMS_CNT				12 //max 255 RCA items (RCA - received chunk analyzer (client's request analyzer))
//see also whttpd_RCAItems[] in whttpd.c - that's also a form of "config"
//
#define WHTTPD_PP_ITEMS_CNT					11 //max 254 tag items in preprocessor
//see also whttpd_PPItems[] in whttpd_preproc.c - that's also a form of "config"
//
#define WHTTPD_POST_ITEMS_CNT				3 //max 255
//see also whttpd_PostItems[] in whttpd_post.c - that's also a form of "config"

#define WHTTPD_FEVT_ITEMS_CNT				1 //max 255
//see also whttpd_FEvtItems[] in whttpd_fevt.c - that's also a form of "config"

#endif
