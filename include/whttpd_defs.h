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
#ifndef __WHTTPD_DEFS_H__
#define __WHTTPD_DEFS_H__

#include <espressif/esp_common.h>
#include <lwip/lwip/sockets.h>
#include <xtensa/config/core-isa.h> //to know XCHAL_INSTROM0_PADDR (for flash reading operations)

#include "whttpd_config.h"

//WHTTPD (W-Dimension's HTTP server) - basic/global definitions

#define UPPER_CHAR(Ch)			( (((Ch)>='a')&&((Ch)<='z'))?(Ch)-('a'-'A'):(Ch) )

#define IS_WHITESPACE(Ch)		( (Ch)<33 )
#define IS_QUOTE(Ch)			( ((Ch)=='\"')||((Ch)=='\'') )

#define RAND_0_TO_X(x)			( (uint32_t)rand() / ((~(uint32_t)0)/(x)) ) //get uint32_t random number 0..x (including)

#define NOT_FLAG8(x)			( (uint8_t)~(uint8_t)(x) )
#define NOT_FLAG16(x)			( (uint16_t)~(uint16_t)(x) )

#ifdef DO_DEBUG_WHTTPD
#define DBG_WHTTPD(...)			printf( "whttpd: "__VA_ARGS__ )
#else
#define DBG_WHTTPD
#endif

#ifdef DO_DEBUG_WHTTPD_VERBOSE
#define DBG_WHTTPD_V(...)		printf( "whttpd: "__VA_ARGS__ )
#else
#define DBG_WHTTPD_V
#endif

#define WHTTPD_RCAFLAG_IS_CASE_SENSITIVE	(1<<0) //when trying to match the Prefix or Suffix (in whttpd_RCAItems[]), do case sensitive comparison
#define WHTTPD_RCAFLAG_OUTPUT_UPPERCASE		(1<<1) //when outputting the result into particular .RCA[].OutputBuf, do upper case
#define WHTTPD_RCAFLAG_OUTPUT_TRIM			(1<<2) //remove all leading/trailing whitespaces - chars that are below char 33 in ASCII (if any)
#define WHTTPD_RCAFLAG_OUTPUT_TRIM_QUOTES	(1<<3) //remove leading/trailing quotes " (if any)
#define WHTTPD_RCAFLAG_OUTPUT_ADDMPB2H		(1<<4) //special just for multipart boundary - add two hyphens at the beginning (<FULL-BOUNDARY> is --<boundary>)
#define WHTTPD_RCAFLAG_NO_OUTPUT			(1<<5) //don't produce output to .OutputBuf (don't allocate memory)
#define WHTTPD_RCAFLAG_SIGNAL_DOUBLE_CRLF	(1<<6) //special usage item - when this item is encountered in the header, set WHTTPD_RCARESFLAG_DOUBLE_CRLF return flag and break whttpd_rca_analyze(...)
#define WHTTPD_RCAFLAG_SIGNAL_MP_BOUNDARY	(1<<7) //special usage item - ... WHTTPD_RCARESFLAG_MP_BOUNDARY

#define WHTTPD_RCARESFLAG_DOUBLE_CRLF		(1<<0) //whttpd_rca_analyze(...) just matched whole .Suffix of item that has flag WHTTPD_RCAFLAG_SIGNAL_DOUBLE_CRLF and exited immediately (.RecvBuf is not probably analyzed yet completely)
#define WHTTPD_RCARESFLAG_MP_BOUNDARY		(1<<1) // ... WHTTPD_RCAFLAG_SIGNAL_MP_BOUNDARY

#define WHTTPD_RQFLAG_IN_MPART_HEADER				(1<<0) //we're analyzing POST data and now we're in mutipart data header
#define WHTTPD_RQFLAG_IN_MPART_DATA					(1<<1) //we're analyzing POST data and now we're in mutipart data
#define WHTTPD_RQFLAG_WAS_IN_MPART_DATA				(1<<2) //we're analyzing POST data and we switched back to mutipart data header but before we've already been in mutipart data
#define WHTTPD_RQFLAG_MPART_DATA_CB_ALREADY_CALLED	(1<<3) //we're analyzing POST data and this is set after first call of POST data callback function (so second+ call will not signal WHTTPD_POST_CBFLAG_IS_FIRST_DATA_BLOCK)
#define WHTTPD_RQFLAG_MPART_DATA_CB_NEED_DATA_END	(1<<4) //we're analyzing POST data and the last POST data callback function call didn't signal end of data (if we realize that that was all, we need to signal end of data WHTTPD_POST_CBFLAG_IS_LAST_DATA_BLOCK)
#define WHTTPD_RQFLAG_STOR_PART_MP_BOUND_TO_DECIDE	(1<<5) //we're analyzing POST data - very special case - see whttpd_pass_post_data_from_recvbuf_if_any(...) for everything about .TruncPostData*
#define WHTTPD_RQFLAG_STOR_PART_MP_BOUND_PASS		(1<<6) // ...

typedef struct {
	const char* Prefix;
	const char* Suffix;
	const uint8_t Flags; //combination of WHTTPD_RCAFLAG_*
} whttpd_rca_item_struct;

typedef enum {
	WHTTPD_RCA_BEFORE_PREFIX = 0, WHTTPD_RCA_BEFORE_SUFFIX, WHTTPD_RCA_AFTER_SUFFIX, WHTTPD_RCA_REALLOC_FAILED
} whttpd_rca_stat_enum;

typedef struct {
	whttpd_rca_stat_enum State;
	uint16_t PosI;
	uint16_t PosO;
	char* OutputBuf; //where parsed data are written
	uint16_t OutputBufSize;
} whttpd_rca_stat_struct;

#define WHTTPD_SLOTFLAG_FORCE_CLOSE				(1<<0) //close connection related with this slot, free memory used by buffers, free the slot for further re-usage
#define WHTTPD_SLOTFLAG_DO_SEND					(1<<1) //send .SendBufBytes bytes from .SendBuf
#define WHTTPD_SLOTFLAG_RESPONSE_END			(1<<2) //after successful WHTTPD_SLOTFLAG_DO_SEND
#define WHTTPD_SLOTFLAG_RCV_RQ_ALL				(1<<3) //indicator that we've got entire request header data from the client
#define WHTTPD_SLOTFLAG_FILL_SENDBUF			(1<<4) //if set, we'll fill .SendBuf by requested file data
#define WHTTPD_SLOTFLAG_USE_PREPROC				(1<<5) //if set, we'll use preprocessor to replace/execute tags in requested file when filling .SendBuf
#define WHTTPD_SLOTFLAG_WAIT_WHILE_PP_IN_USE	(1<<6) //if set, the preprocessor was in use when we tried to use it - try to use it in every new iteration of WHTTPD main loop
#define WHTTPD_SLOTFLAG_USING_PREPROC			(1<<7) //only one slot can have this set at one time

typedef enum {
	WHTTPD_RQ_TYPE_UNKNOWN = 0, WHTTPD_RQ_TYPE_GET, WHTTPD_RQ_TYPE_POST
} whttpd_rq_type_enum;

typedef struct {
	char* RqParams;
	int16_t HTTPRespCode;
} whttpd_slot_state_for_preproc_struct;

typedef struct {
	uint8_t* PostDataPtr;
	char* FormInputName;
	char* FileName;
	uint8_t* TruncPostDataPtr;        //for very special case when the received data chunk ends in the middle of multipart boundary - then we pass to POST data callback function only data before possible multipart boundary and decide later if it was a false alarm or not (if yes, pass truncated post data to proper callback function)
	uint8_t TruncPostDataBytes;       // ...
	uint8_t TruncPostDataPostItemIdx; // ...
} whttpd_slot_state_for_post_struct;

typedef struct {
	int AcptSck; //-1 = unused; 0+ = active connection descriptor (as returned by lwip_accept(...))
	struct sockaddr_in AcptAddr;
	uint16_t IdleLoopsCtr;
	uint8_t* RecvBuf;
	int RecvBufBytes; //(as returned by lwip_recv(...))
	uint8_t* SendBuf;
	uint16_t SendBufBytes;
	uint8_t Flags; //combination of WHTTPD_SLOTFLAG_*
	//
	uint16_t RCARecvBufPos;
	whttpd_rca_stat_struct RCA[WHTTPD_RCA_ITEMS_CNT]; //state vectors of RCA - recv. chunk analyzer (client's request analyzer)
	whttpd_rq_type_enum RqType;
	uint8_t RqCRLFSize; //default newline is "\r\n" (but sometimes it can be "\n" - this is decided when request header is received)
	uint32_t RqBodyContentLen;
	uint32_t RqBodyRecvBytes;
	uint8_t RqFlags; //combination of WHTTPD_RQFLAG_*
	uint8_t RqLastPostItemIdx; //valid only when flag WHTTPD_RQFLAG_MPART_DATA_CB_NEED_DATA_END in RqFlasg is set
	//
	uint8_t FEvtItemIdx;
	//
	char* RqParams;
	//
	uint8_t RespFIdx;
	uint32_t RespFSize;
	uint32_t RespFOffs;
	//
	whttpd_slot_state_for_preproc_struct SSFPreproc;
	//
	whttpd_slot_state_for_post_struct SSFPost;
} whttpd_slot_struct;

#define WHTTPD_NO_SLOT_IDX				-1

#define WHTTPD_IS_VALID_SOCKFD(s)		(s >= 0) //-1 = unused; 0+ = active connection descriptor (as returned by lwip_accept(...))

#define WHTTPD_ERR_MSG_200				"OK"
#define WHTTPD_ERR_MSG_400				"Bad Request"
#define WHTTPD_ERR_MSG_404				"Not Found"
#define WHTTPD_ERR_MSG_500				"Internal Server Error"

#define WHTTPD_FAILSAFE_500_CHNK_BODY	"1B\r\n500 - Internal Server Error\r\n0\r\n\r\n" //!make sure that WHTTPD_SEND_BUF_LEN is big enough to accommodate sprintf-ed response header + WHTTPD_FAILSAFE_500_CHNK_BODY

#define WHTTPD_SPIFLASH_OFFS			XCHAL_INSTROM0_PADDR //offset where SPI FLASH is mapped on ESP8266 - for ESP8266 it's 0x40200000

#endif
