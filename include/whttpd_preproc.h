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
#ifndef __WHTTPD_PREPROC_H__
#define __WHTTPD_PREPROC_H__

#include <espressif/esp_common.h>

#include "whttpd_config.h"

//WHTTPD PREPROCESSOR (processing tags in file currently being sent to client)

#ifdef DO_DEBUG_WHTTPD_PREPROC
#define DBG_WHTTPD_PP(...)		printf( "whttpd: preproc: "__VA_ARGS__ )
#else
#define DBG_WHTTPD_PP
#endif

#define WHTTPD_PPFLAG_IN_USE					(1<<0) //from start until the entire input file is passed to output, this is set (we have intentionally only one global preprocessor to assure minimum memory usage)
#define WHTTPD_PPFLAG_FAILSAFE_MODE				(1<<1) //if this is set, preprocessor is just fetching data as they are, without preprocessing
#define WHTTPD_PPFLAG_HAVE_LINE					(1<<2) //
#define WHTTPD_PPFLAG_END_OF_FILE				(1<<3) //
#define WHTTPD_PPFLAG_GETTING_BEF_TAG_DATA		(1<<4) //(BEF_TAG = before tag or no tag)
#define WHTTPD_PPFLAG_GETTING_TAG_DATA			(1<<5) //
#define WHTTPD_PPFLAG_HAVE_DATA_BEF_TAG			(1<<6) //(BEF_TAG = before tag or no tag)
#define WHTTPD_PPFLAG_HAVE_TAG					(1<<7) //
#define WHTTPD_PPFLAG_END_OF_LINE				(1<<8) //
#define WHTTPD_PPFLAG_TAG_FUNC_FIRST_CALL		(1<<9) //

typedef struct {
	char* LineBuf;				//where input file is read, line by line (terminated by '\n' (including))
	uint16_t LineBufSize;		//uint16_t => one line (terminated by "\n" or "\r\n" in the preprocessed file can have max 65535 chars (including "\r\n" (null termination at max 65535))). See WHTTPD_PP_LINE_BUF_MAX_SIZE
	//
	uint16_t Flags;				//combination of WHTTPD_PPFLAG_*
	//
	uint8_t RespFIdx;
	uint32_t RespFSize;
	//
	whttpd_slot_state_for_preproc_struct* SSFPreproc;
	//
	int16_t LineReadStartPos;	//what is the offset from where to start to read .LineBuf (including)
	int16_t TagStartPos;		//if (TagStartPos==LineReadEndPos) there's no tag / else the tag is between TagStartPos..LineReadEndPos (TagStartPos is pointing to first char of tag ('[') and LineReadEndPos is pointing to last char of tag (']'))
	int16_t LineReadEndPos;		//what is the offset up to which to read .LineBuf (including)
	uint8_t PPItemIdx;			//if we have tag, this specifies the tag (whttpd_PPItems[PPItemIdx]). We can have max 254 tag items (index 255 reserved for WHTTPD_PP_INVALID_ITEM_INDEX)
	uint16_t TagFuncCallsCtr;
} whttpd_pp_struct;

#define WHTTPD_PP_INVALID_ITEM_INDEX		255
#define WHTTPD_PP_LINE_BUF_MAX_SIZE			65535

#define WHTTPD_PP_SET_RETCODE_OK			0
#define WHTTPD_PP_SET_RETCODE_PP_IN_USE		-1

typedef struct {
	const char* Tag;
	int8_t (*func_cb)(uint8_t IsFirstCall, char* OutputData, uint16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetDone);
	/* Tag callback function should behave like this:
	 * input:
	 * - IsFirstCall - is non-zero value if this is the first call of this callback function (in the scope of one tag) because the callback function can generate as many data as it wants, but we need to chunk the output, so the next call would be with IsFirstCall==0
	 * - MaxBytes - indicates up to how many bytes we can store into OutputData buffer
	 * output:
	 * - OutputData - output data buffer (this buffer exists when func_cb is called)
	 * - RetBytes - here we indicate how many bytes were really stored (our response can be smaller than MaxBytes)
	 * - RetDone - here we indicate if we've already finished our data output (set to non-zero value) or if we need further calling (when we send chunk to client and we have again space in OutputData buffer)
	 * - return 0 for success, less than 0 for error (will cause aborting next call of this cb (as RetDone would be set) => aborts tag preprocessing)
	 */
	//
} whttpd_pp_item_struct;

#define WHTTPD_PP_TAG_CB_CALLS_CNT_LIMIT	10000 //(max 65535) safety fuse - if user's .func_cb would never set *RetDone, it would be called again and again. This is the limit of max count of calls per one tag processing

//----

void ICACHE_FLASH_ATTR whttpd_preproc_manage_cb_output(char* Dest, char* Src, uint16_t* AlreadyCopiedBytes, uint16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetDone);
uint8_t ICACHE_FLASH_ATTR whttpd_preproc_get_req_param_value_ptr(char* SearchFor, char** PtrValue, uint16_t *Len);
//
void ICACHE_FLASH_ATTR whttpd_preproc_first_init();
void ICACHE_FLASH_ATTR whttpd_preproc_init();
int8_t ICACHE_FLASH_ATTR whttpd_preproc_set(uint8_t RespFIdx, uint32_t RespFSize, whttpd_slot_state_for_preproc_struct* SSFPreproc);
volatile whttpd_pp_struct* ICACHE_FLASH_ATTR whttpd_preproc_get_PP_ptr(void);
//void ICACHE_FLASH_ATTR whttpd_preproc_find_next_tag(); //don't publish here
void ICACHE_FLASH_ATTR whttpd_preproc_get_data(uint32_t* RespFOffs, uint8_t* RetBuf, int16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetHaveAll);

//----

#endif
