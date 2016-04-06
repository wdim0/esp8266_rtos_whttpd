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
//#include <freertos/task.h>
#include <espressif/esp_common.h>
#include <lwip/lwip/sockets.h>
#include <wfof.h>

#include "whttpd_defs.h"
#include "whttpd_preproc.h"
#include "whttpd_post.h"
#include "whttpd_fevt.h"

//WHTTPD (W-Dimension's HTTP server) - MAIN

volatile whttpd_slot_struct whttpd_Slots[WHTTPD_MAX_CONNECTIONS];
uint8_t UpgradeRebootWhenNoActiveSlot = 0;

whttpd_rca_item_struct whttpd_RCAItems[WHTTPD_RCA_ITEMS_CNT] = {
		//Here we define how to parse headers received from the client.
		//You can add your own items -> you'll find it parsed in the .RCA[].OutputBuf (see whttpd_slot_struct)
		{.Prefix="GET ", .Suffix=" HTTP/", .Flags=WHTTPD_RCAFLAG_OUTPUT_TRIM},                                //don't delete - we define here how to parse Request-URI in GET request -> .RCA[0]
		{.Prefix="POST ", .Suffix=" HTTP/", .Flags=WHTTPD_RCAFLAG_OUTPUT_TRIM},                               //don't delete - ... POST request -> .RCA[1]
		{.Prefix="", .Suffix="\r\n\r\n", .Flags=WHTTPD_RCAFLAG_SIGNAL_DOUBLE_CRLF|WHTTPD_RCAFLAG_NO_OUTPUT},  //don't delete - "\r\n\r\n" detector -> .RCA[2] (CRLF detector)
		{.Prefix="", .Suffix="\n\n", .Flags=WHTTPD_RCAFLAG_SIGNAL_DOUBLE_CRLF|WHTTPD_RCAFLAG_NO_OUTPUT},      //don't delete - "\n\n" detector -> .RCA[3] (CRLF detector)
		{.Prefix="HTTP/", .Suffix="\n", .Flags=WHTTPD_RCAFLAG_OUTPUT_TRIM},                                   //don't delete - HTTP version -> .RCA[4]
		{.Prefix="Content-Type:", .Suffix="\n", .Flags=WHTTPD_RCAFLAG_OUTPUT_TRIM},                           //don't delete - content type -> .RCA[5]
		{.Prefix="Content-Length:", .Suffix="\n", .Flags=WHTTPD_RCAFLAG_OUTPUT_TRIM},                         //don't delete - content length (string representation) -> .RCA[6]
		{.Prefix="Content-Type: multipart/form-data; boundary=", .Suffix="\n", .Flags=WHTTPD_RCAFLAG_OUTPUT_TRIM|WHTTPD_RCAFLAG_OUTPUT_TRIM_QUOTES|WHTTPD_RCAFLAG_OUTPUT_ADDMPB2H}, //don't delete - value of full boundary (that's what we're really searching for) when receiving multipart/form-data POST -> .RCA[7]
		{.Prefix="Content-Disposition: form-data; name=\"", .Suffix="\"", .Flags=WHTTPD_RCAFLAG_OUTPUT_TRIM}, //don't delete - name of form input (using <form><input name="...">) -> .RCA[8]
		{.Prefix="; filename=\"", .Suffix="\"", .Flags=WHTTPD_RCAFLAG_OUTPUT_TRIM},                           //don't delete - name of uploaded file (using <form><input type="file" ...>) -> .RCA[9]
		{.Prefix="", .Suffix=NULL, .Flags=WHTTPD_RCAFLAG_SIGNAL_MP_BOUNDARY|WHTTPD_RCAFLAG_NO_OUTPUT},        //don't delete - multipart boundary detector (.Suffix is set later) -> .RCA[10]
		{.Prefix="User-Agent:", .Suffix="\n", .Flags=WHTTPD_RCAFLAG_OUTPUT_TRIM}
		// ...
		//! if you change the number of items here, don't forget to change WHTTPD_RCA_ITEMS_CNT respectively !
};

//these are fixed
#define WHTTPD_RCAITEM_GET					0
#define WHTTPD_RCAITEM_POST					1
#define WHTTPD_RCAITEM_CRLF2_DETECT			2
#define WHTTPD_RCAITEM_CRLF1_DETECT			3
#define WHTTPD_RCAITEM_HTTP_VER				4
#define WHTTPD_RCAITEM_CONT_TYPE			5
#define WHTTPD_RCAITEM_CONT_LEN				6
#define WHTTPD_RCAITEM_MP_BOUNDARY			7
#define WHTTPD_RCAITEM_FORM_INPUT_NAME		8
#define WHTTPD_RCAITEM_UPLD_FILE_NAME		9
#define WHTTPD_RCAITEM_MP_BOUNDARY_DETECT	10

//---- advanced string functions

void ICACHE_FLASH_ATTR whttpd_str_trim(char** RetStr){
//Trims Str (modifies directly Str). Str can be max 65535 bytes long (null termination at offset 65535).
	uint16_t Pos = 0;
	uint16_t NewStartIdx = 0;
	uint16_t NewLen = 0;
	uint8_t GotNonWSCh = 0;
	while((*RetStr)[Pos]!=0){
		if(!GotNonWSCh){
			if(IS_WHITESPACE((*RetStr)[Pos])) NewStartIdx = Pos+1;
			else {
				GotNonWSCh = 1;
				NewLen = 1;
			}
		}
		else{
			if(!IS_WHITESPACE((*RetStr)[Pos])) NewLen = (Pos-NewStartIdx)+1;
		}
		Pos++;
	}
	if(NewLen==0) (*RetStr)[0]=0; //all whitespaces / empty string
	if((NewStartIdx>0)||(NewLen<Pos)){ //if the original string has leading/trailing whitespaces
		memmove((*RetStr), &((*RetStr)[NewStartIdx]), NewLen);
		(*RetStr)[NewLen]=0; //new null termination
	}
}

void ICACHE_FLASH_ATTR whttpd_str_trim_quotes(char** RetStr){
//Trims Str (modifies directly Str). Str can be max 65535 bytes long (null termination at offset 65535).
	uint16_t Pos = 0;
	uint16_t NewStartIdx = 0;
	uint16_t NewLen = 0;
	uint8_t GotNonQCh = 0;
	while((*RetStr)[Pos]!=0){
		if(!GotNonQCh){
			if(IS_QUOTE((*RetStr)[Pos])) NewStartIdx = Pos+1;
			else {
				GotNonQCh = 1;
				NewLen = 1;
			}
		}
		else{
			if(!IS_QUOTE((*RetStr)[Pos])) NewLen = (Pos-NewStartIdx)+1;
		}
		Pos++;
	}
	if(NewLen==0) (*RetStr)[0]=0; //all quotes / empty string
	if((NewStartIdx>0)||(NewLen<Pos)){ //if the original string has leading/trailing quotes
		memmove((*RetStr), &((*RetStr)[NewStartIdx]), NewLen);
		(*RetStr)[NewLen]=0; //new null termination
	}
}

void ICACHE_FLASH_ATTR whttpd_str_split(char* Src, char** RetBefDelim, char** RetAftDelim, char Delim, uint8_t FillOnlyBefDelim){
/* Splits Src to RetBefDelim, RetAftDelim using delimiter Delim. Str can be max 65535 bytes long
 * (null termination at offset 65535).
 * Fills RetBefDelim, RetAftDelim - allocates memory - don't forget to free when you don't need it anymore.
 */
	if((*RetBefDelim)!=NULL) free(*RetBefDelim);
	if((!FillOnlyBefDelim)&&((*RetAftDelim)!=NULL)) free(*RetAftDelim);
	//
	char *PosQM = strchr(Src, Delim);
	uint16_t Len;
	if(PosQM==NULL){ //there's no Delim in string Src => create copy of Src into RetBefDelim (allocate memory) & set RetAftDelim = NULL
		Len = strlen(Src);
		(*RetBefDelim) = malloc(Len+1);
		if((*RetBefDelim)==NULL){ //malloc failed
			//(*RetBefDelim) = NULL;
			if(!FillOnlyBefDelim) (*RetAftDelim) = NULL;
			return;
		}
		strcpy((*RetBefDelim), Src);
		if(!FillOnlyBefDelim) (*RetAftDelim) = NULL;
	}
	else{ //there's Delim in string Src => copy what's before Delim into RetBefDelim (allocate memory) & copy what's after Delim into RetAftDelim (allocate memory)
		Len = PosQM-Src;
		(*RetBefDelim) = malloc(Len+1);
		if((*RetBefDelim)==NULL){ //malloc failed
			//(*RetBefDelim) = NULL;
			if(!FillOnlyBefDelim) (*RetAftDelim) = NULL;
			return;
		}
		strncpy((*RetBefDelim), Src, Len);
		(*RetBefDelim)[Len] = 0; //add null termination
		//
		if(!FillOnlyBefDelim){
			Len = ((Src+strlen(Src)-PosQM)-1);
			(*RetAftDelim) = malloc(Len+1);
			if((*RetAftDelim)==NULL){ //malloc failed
				if((*RetBefDelim)!=NULL) free(*RetBefDelim);
				(*RetBefDelim) = NULL;
				//(*RetAftDelim) = NULL;
				return;
			}
			strncpy((*RetAftDelim), &Src[(PosQM-Src)+1], Len);
			(*RetAftDelim)[Len] = 0; //add null termination
		}
	}
}

void ICACHE_FLASH_ATTR whttpd_str_uppercase(char** RetStr, char* Str){
/* Makes an uppercased copy of Str into RetStr. Str can be max 65535 bytes long (null termination at offset 65535).
 * Fills RetStr - allocates memory - don't forget to free when you don't need it anymore.
 */
if((*RetStr)!=NULL) free(*RetStr);
	//
	uint16_t Len = strlen(Str);
	uint16_t Idx;
	if(Len>0){
		(*RetStr) = malloc(Len+1);
		if((*RetStr)==NULL){ //malloc failed
			//(*RetStr) = NULL;
			return;
		}
		for(Idx=0;Idx<Len;Idx++) (*RetStr)[Idx] = UPPER_CHAR(Str[Idx]);
		(*RetStr)[Len] = 0; //add null termination
	}
	else (*RetStr) = NULL;
}

//---- POST data passing functions

void ICACHE_FLASH_ATTR whttpd_pass_post_data_from_recvbuf_if_any(int8_t SIdx){
/* Behaves according to state of various members in whttpd_Slots[SIdx].
 * Variables that influence/are changed by this function:
 * - .SSFPost.*
 * - .RqFlags
 * - .RCARecvBufPos
 * - .RqCRLFSize
 * - whttpd_RCAItems[WHTTPD_RCAITEM_MP_BOUNDARY_DETECT].Suffix
 * - RCA[WHTTPD_RCAITEM_MP_BOUNDARY_DETECT].*
 * This function is always called even for last block of multipart data, because according to HTTP specif. there's multipart boundary string at the end
 * => we don't need to solve additional calling of whttpd_post_call_cb(...) with flag WHTTPD_POST_CBFLAG_IS_LAST_DATA_BLOCK set.
 */
	if(whttpd_Slots[SIdx].SSFPost.PostDataPtr==NULL) return; //no pointer to data to be passed
	if(whttpd_Slots[SIdx].SSFPost.FormInputName==NULL) return; //currently read multipart section has no header about form input name - see RCA[WHTTPD_RCAITEM_FORM_INPUT_NAME]
	//
	uint8_t Mode = 0;
	uint8_t PostCBFlags = 0;
	if((whttpd_Slots[SIdx].RqFlags & WHTTPD_RQFLAG_IN_MPART_HEADER)&&(!(whttpd_Slots[SIdx].RqFlags & WHTTPD_RQFLAG_IN_MPART_DATA))&&(whttpd_Slots[SIdx].RqFlags & WHTTPD_RQFLAG_WAS_IN_MPART_DATA)){
		//if we're here, we've just encountered new multipart boundary of POST request and we was in multipart data before => we need to pass that previous multipart data
		//now .RecvBuf[.RCARecvBufPos] points to next char (no range checking) after last char of .Suffix of boundary detector (RCA[WHTTPD_RCAITEM_MP_BOUNDARY_DETECT])
		Mode = 1;
		PostCBFlags |= WHTTPD_POST_CBFLAG_IS_LAST_DATA_BLOCK;
	}
	if(whttpd_Slots[SIdx].RqFlags & WHTTPD_RQFLAG_IN_MPART_DATA){
		//if we're here, we've just analyzed all the data in the received chunk of data of POST request and the data didn't end yet (not yet terminated by mutipart boundary) => we need to pass the multipart data
		//now .RCARecvBufPos is pointing one char beyond .RecvBuf
		Mode = 2;
	}
	if(!Mode) return;
	//
	uint8_t PostItemIdx = whttpd_post_get_item_idx(whttpd_Slots[SIdx].SSFPost.FormInputName);
	if(PostItemIdx==WHTTPD_POST_INVALID_ITEM_INDEX) return; //but we don't have any POST data callback function related to this data => don't do anything and exit
	//
	int32_t PostDataBytes = 0;
	if(Mode==1){
		PostDataBytes = (&whttpd_Slots[SIdx].RecvBuf[whttpd_Slots[SIdx].RCARecvBufPos])-whttpd_Slots[SIdx].RqCRLFSize-strlen(whttpd_RCAItems[WHTTPD_RCAITEM_MP_BOUNDARY_DETECT].Suffix)-whttpd_Slots[SIdx].SSFPost.PostDataPtr; //-.RqCRLFSize to discount one CRLF before multipart boundary (yes, that's "human readable" HTTP)
	}
	if(Mode==2){
		PostDataBytes = (&whttpd_Slots[SIdx].RecvBuf[whttpd_Slots[SIdx].RCARecvBufPos])-whttpd_Slots[SIdx].SSFPost.PostDataPtr;
		//
		if((whttpd_Slots[SIdx].RCA[WHTTPD_RCAITEM_MP_BOUNDARY_DETECT].State==WHTTPD_RCA_BEFORE_SUFFIX)&&(whttpd_Slots[SIdx].RCA[WHTTPD_RCAITEM_MP_BOUNDARY_DETECT].PosI>0)){ //very special case - it seems that we're in the middle of multipart boundary - multipart boundary detector is in the middle of something
			//lower the PostDataBytes assuming that it's multipart boundary string
			uint8_t DecreaseLen = whttpd_Slots[SIdx].RCA[WHTTPD_RCAITEM_MP_BOUNDARY_DETECT].PosI+whttpd_Slots[SIdx].RqCRLFSize; //+.RqCRLFSize to discount one CRLF before multipart boundary (yes, that's "human readable" HTTP)
			DBG_WHTTPD("slot[%d]: .RecvBuf probably ends with incomplete multipart boundary string => truncating (%d bytes) and storing truncated data in .TruncatedPostDataPtr\n", SIdx, DecreaseLen);
			//
			PostDataBytes -= DecreaseLen;
			//
			//remember the data we've truncated. We'll decide later if it was a false alarm or not (if yes, pass truncated data to proper POST data callback function)
			whttpd_Slots[SIdx].SSFPost.TruncPostDataPtr = malloc(DecreaseLen+1); //+1 just for null termination to be able to be verbose about that
			if(whttpd_Slots[SIdx].SSFPost.TruncPostDataPtr!=NULL){ //malloc ok
				memcpy(whttpd_Slots[SIdx].SSFPost.TruncPostDataPtr, &(whttpd_Slots[SIdx].SSFPost.PostDataPtr[PostDataBytes]), DecreaseLen);
				whttpd_Slots[SIdx].SSFPost.TruncPostDataBytes = DecreaseLen;
				whttpd_Slots[SIdx].SSFPost.TruncPostDataPostItemIdx = PostItemIdx;
				whttpd_Slots[SIdx].RqFlags |= WHTTPD_RQFLAG_STOR_PART_MP_BOUND_TO_DECIDE;
				//
				whttpd_Slots[SIdx].SSFPost.TruncPostDataPtr[whttpd_Slots[SIdx].SSFPost.TruncPostDataBytes] = 0; //add null termination (just to be able to be verbose)
				DBG_WHTTPD("-> truncated data: '%s'\n", whttpd_Slots[SIdx].SSFPost.TruncPostDataPtr);
			}
			else{ //this is really bad, we don't have enough memory to remember TruncatedPostData
				DBG_WHTTPD("-> can't allocate memory for .TruncatedPostDataPtr. Data lost may occur when it's not incomplete multipart boundary string\n");
			}
		}
	}
	if(PostDataBytes>0){
		if(!(whttpd_Slots[SIdx].RqFlags & WHTTPD_RQFLAG_MPART_DATA_CB_ALREADY_CALLED)) PostCBFlags |= WHTTPD_POST_CBFLAG_IS_FIRST_DATA_BLOCK;
		//
		whttpd_post_call_cb(PostItemIdx, PostCBFlags, whttpd_Slots[SIdx].SSFPost.PostDataPtr, PostDataBytes);
		//
		whttpd_Slots[SIdx].RqFlags |= WHTTPD_RQFLAG_MPART_DATA_CB_ALREADY_CALLED;
		if(!(PostCBFlags & WHTTPD_POST_CBFLAG_IS_LAST_DATA_BLOCK)){
			whttpd_Slots[SIdx].RqFlags |= WHTTPD_RQFLAG_MPART_DATA_CB_NEED_DATA_END;
			whttpd_Slots[SIdx].RqLastPostItemIdx = PostItemIdx;
		}
		else{
			whttpd_Slots[SIdx].RqFlags &= NOT_FLAG8(WHTTPD_RQFLAG_MPART_DATA_CB_NEED_DATA_END);
			whttpd_Slots[SIdx].RqLastPostItemIdx = WHTTPD_POST_INVALID_ITEM_INDEX;
		}
	}
}

void ICACHE_FLASH_ATTR whttpd_finish_pass_post_data_if_needed(int8_t SIdx){
	if(!(whttpd_Slots[SIdx].RqFlags & WHTTPD_RQFLAG_MPART_DATA_CB_NEED_DATA_END)) return;
	if(whttpd_Slots[SIdx].RqLastPostItemIdx==WHTTPD_POST_INVALID_ITEM_INDEX) return; //but we don't have any POST data callback function related to this data => don't do anything and exit
	//
	whttpd_post_call_cb(whttpd_Slots[SIdx].RqLastPostItemIdx, WHTTPD_POST_CBFLAG_IS_LAST_DATA_BLOCK, NULL, 0);
	//
	whttpd_Slots[SIdx].RqFlags &= NOT_FLAG8(WHTTPD_RQFLAG_MPART_DATA_CB_NEED_DATA_END);
	whttpd_Slots[SIdx].RqLastPostItemIdx = WHTTPD_POST_INVALID_ITEM_INDEX;
}

void ICACHE_FLASH_ATTR whttpd_discard_truncpostdata_if_any_and_clear_flags(int8_t SIdx){
	if(whttpd_Slots[SIdx].SSFPost.TruncPostDataPtr!=NULL) free(whttpd_Slots[SIdx].SSFPost.TruncPostDataPtr);
	whttpd_Slots[SIdx].SSFPost.TruncPostDataPtr = NULL;
	whttpd_Slots[SIdx].SSFPost.TruncPostDataBytes = 0;
	whttpd_Slots[SIdx].SSFPost.TruncPostDataPostItemIdx = WHTTPD_POST_INVALID_ITEM_INDEX;
	whttpd_Slots[SIdx].RqFlags &= NOT_FLAG8(WHTTPD_RQFLAG_STOR_PART_MP_BOUND_TO_DECIDE|WHTTPD_RQFLAG_STOR_PART_MP_BOUND_PASS);
}

void ICACHE_FLASH_ATTR whttpd_pass_post_data_from_truncpostdata_if_requested(int8_t SIdx){
	if(whttpd_Slots[SIdx].RqFlags & WHTTPD_RQFLAG_STOR_PART_MP_BOUND_PASS){
		uint8_t PostCBFlags = 0;
		if(!(whttpd_Slots[SIdx].RqFlags & WHTTPD_RQFLAG_MPART_DATA_CB_ALREADY_CALLED)) PostCBFlags |= WHTTPD_POST_CBFLAG_IS_FIRST_DATA_BLOCK;
		//
		whttpd_post_call_cb(whttpd_Slots[SIdx].SSFPost.TruncPostDataPostItemIdx, PostCBFlags, whttpd_Slots[SIdx].SSFPost.TruncPostDataPtr, whttpd_Slots[SIdx].SSFPost.TruncPostDataBytes);
		//
		whttpd_Slots[SIdx].RqFlags |= WHTTPD_RQFLAG_MPART_DATA_CB_ALREADY_CALLED|WHTTPD_RQFLAG_MPART_DATA_CB_NEED_DATA_END;
		whttpd_Slots[SIdx].RqLastPostItemIdx = whttpd_Slots[SIdx].SSFPost.TruncPostDataPostItemIdx;
		//
		whttpd_discard_truncpostdata_if_any_and_clear_flags(SIdx);
	}
}

//---- RCA - received chunk analyzer (client's request analyzer)

void ICACHE_FLASH_ATTR whttpd_rca_init_item(int8_t SIdx, uint8_t RCAItemIdx){
//Init specific .RCA[] item.
	memset((uint8_t*)&whttpd_Slots[SIdx].RCA[RCAItemIdx], 0, sizeof(whttpd_rca_stat_struct)); //fills everything by 0 (state set to WHTTPD_RCA_BEFORE_PREFIX)
}

void ICACHE_FLASH_ATTR whttpd_rca_free_and_reinit_item(int8_t SIdx, uint8_t RCAItemIdx){
//Free specific .RCA[] item (free allocated memory for .OutputBuf buffer) and reinit.
	if(whttpd_Slots[SIdx].RCA[RCAItemIdx].OutputBuf!=NULL) free(whttpd_Slots[SIdx].RCA[RCAItemIdx].OutputBuf);
	whttpd_rca_init_item(SIdx, RCAItemIdx);
}

void ICACHE_FLASH_ATTR whttpd_rca_init(int8_t SIdx){
//Init entire RCA[].
	uint8_t RCAItemIdx;
	for(RCAItemIdx=0;RCAItemIdx<WHTTPD_RCA_ITEMS_CNT;RCAItemIdx++){ //go through all whttpd_Slots[SIdx].RCA[]
		whttpd_rca_init_item(SIdx, RCAItemIdx);
	}
}

void ICACHE_FLASH_ATTR whttpd_rca_free_and_reinit(int8_t SIdx){
//Free entire .RCA[] (free allocated memory for .OutputBuf buffers) and reinit.
	uint8_t RCAItemIdx;
	for(RCAItemIdx=0;RCAItemIdx<WHTTPD_RCA_ITEMS_CNT;RCAItemIdx++){ //go through all whttpd_Slots[SIdx].RCA[]
		whttpd_rca_free_and_reinit_item(SIdx, RCAItemIdx);
	}
}

uint8_t ICACHE_FLASH_ATTR whttpd_rca_analyze(int8_t SIdx, uint16_t *RecvBufPos){
/* RCA - received chunk analyzer (client's request analyzer).
 * Analyzes whttpd_Slots[SIdx].RecvBuf char by char, starting on position *RecvBufPos.
 * This function can exit prematurely in some special cases (when the control logic needs update) and then it's called again
 * until entire whttpd_Slots[SIdx].RecvBuf is analyzed.
 * Returns combination of WHTTPD_RCARESFLAG_*
 */
	char Ch,Ch1,Ch2;
	uint8_t RCAItemIdx;
	while(*RecvBufPos<whttpd_Slots[SIdx].RecvBufBytes){ //go through all bytes of received chunk of data
		//
		Ch = whttpd_Slots[SIdx].RecvBuf[*RecvBufPos];
		//
		whttpd_rca_stat_struct* RCA;
		for(RCAItemIdx=0;RCAItemIdx<WHTTPD_RCA_ITEMS_CNT;RCAItemIdx++){ //go through all whttpd_Slots[SIdx].RCA[] / whttpd_RCAItems[]
			if((whttpd_RCAItems[RCAItemIdx].Suffix==NULL)||(whttpd_RCAItems[RCAItemIdx].Prefix==NULL)) continue; //skip inactive items
			Ch1 = (whttpd_RCAItems[RCAItemIdx].Flags & WHTTPD_RCAFLAG_IS_CASE_SENSITIVE)?Ch:UPPER_CHAR(Ch);
			//
			RCA = (whttpd_rca_stat_struct*)&(whttpd_Slots[SIdx].RCA[RCAItemIdx]);
			switch(RCA->State){
				case WHTTPD_RCA_BEFORE_PREFIX:
					Ch2 = (whttpd_RCAItems[RCAItemIdx].Flags & WHTTPD_RCAFLAG_IS_CASE_SENSITIVE)?whttpd_RCAItems[RCAItemIdx].Prefix[RCA->PosI]:UPPER_CHAR(whttpd_RCAItems[RCAItemIdx].Prefix[RCA->PosI]);
					if(Ch1==Ch2) RCA->PosI++;
					else RCA->PosI = 0;
					uint8_t DoBreak = 1;
					if(whttpd_RCAItems[RCAItemIdx].Prefix[RCA->PosI]==0){ //we've matched whole prefix (now we're at null termination) => switch to WHTTPD_RCA_BEFORE_SUFFIX state
						if(RCA->PosI==0) DoBreak=0; //null termination is at position 0 => prefix is empty string
						RCA->State = WHTTPD_RCA_BEFORE_SUFFIX;
						RCA->PosI = 0;
					}
					if(DoBreak) break; //if prefix is empty string => pass right to next branch of the switch (now RCA->State==WHTTPD_RCA_BEFORE_SUFFIX)
				case WHTTPD_RCA_BEFORE_SUFFIX:
					//add Ch to RCA->OutputBuf
					if(!(whttpd_RCAItems[RCAItemIdx].Flags & WHTTPD_RCAFLAG_NO_OUTPUT)){ //generate output to .OutputBuf
						if(RCA->PosO >= RCA->OutputBufSize){ //no more space in OutputBuf => make more space
							//
							char* NewPtr = realloc(RCA->OutputBuf, RCA->OutputBufSize+WHTTPD_RCA_OUTPUTBUF_MALLOC_STEP);
							if(NewPtr==NULL){
								free(RCA->OutputBuf);
								RCA->OutputBuf = NULL;
								RCA->OutputBufSize = 0;
								RCA->State = WHTTPD_RCA_REALLOC_FAILED;
								break;
							}
							else{
								RCA->OutputBuf = NewPtr;
								RCA->OutputBufSize += WHTTPD_RCA_OUTPUTBUF_MALLOC_STEP;
							}
						}
						RCA->OutputBuf[RCA->PosO++] = (whttpd_RCAItems[RCAItemIdx].Flags & WHTTPD_RCAFLAG_OUTPUT_UPPERCASE)?UPPER_CHAR(Ch):Ch;
					}
					//
					Ch2 = (whttpd_RCAItems[RCAItemIdx].Flags & WHTTPD_RCAFLAG_IS_CASE_SENSITIVE)?whttpd_RCAItems[RCAItemIdx].Suffix[RCA->PosI]:UPPER_CHAR(whttpd_RCAItems[RCAItemIdx].Suffix[RCA->PosI]);
					if(Ch1==Ch2) RCA->PosI++;
					else{
						RCA->PosI = 0;
						if((RCAItemIdx==WHTTPD_RCAITEM_MP_BOUNDARY_DETECT)&&(whttpd_Slots[SIdx].RqFlags & WHTTPD_RQFLAG_STOR_PART_MP_BOUND_TO_DECIDE)){ //no, it was not a part of boundary - ask for passing .TruncatedPostDataPtr data to proper POST data callback function
							DBG_WHTTPD("slot[%d]: .RecvBuf did not end with incomplete multipart boundary string => asking to pass .TruncatedPostDataPtr\n", SIdx);
							whttpd_Slots[SIdx].RqFlags &= NOT_FLAG8(WHTTPD_RQFLAG_STOR_PART_MP_BOUND_TO_DECIDE);
							whttpd_Slots[SIdx].RqFlags |= WHTTPD_RQFLAG_STOR_PART_MP_BOUND_PASS;
						}
					}
					if(whttpd_RCAItems[RCAItemIdx].Suffix[RCA->PosI]==0){ //we've matched whole suffix (now we're at null termination) => switch to WHTTPD_RCA_AFTER_SUFFIX state
						RCA->State = WHTTPD_RCA_AFTER_SUFFIX;
						RCA->PosI = 0;
						//
						if((RCAItemIdx==WHTTPD_RCAITEM_MP_BOUNDARY_DETECT)&&(whttpd_Slots[SIdx].RqFlags & WHTTPD_RQFLAG_STOR_PART_MP_BOUND_TO_DECIDE)){ //yes, it was part of boundary - discard data saved in .TruncatedPostDataPtr
							DBG_WHTTPD("slot[%d]: .RecvBuf really did end with incomplete multipart boundary string => discarding .TruncatedPostDataPtr\n", SIdx);
							whttpd_discard_truncpostdata_if_any_and_clear_flags(SIdx);
						}
						//
						if(!(whttpd_RCAItems[RCAItemIdx].Flags & WHTTPD_RCAFLAG_NO_OUTPUT)){ //output is generated to .OutputBuf
							//strip suffix from RCA->OutputBuf, add null termination
							RCA->OutputBuf[RCA->PosO-strlen(whttpd_RCAItems[RCAItemIdx].Suffix)] = 0;
							if(whttpd_RCAItems[RCAItemIdx].Flags & WHTTPD_RCAFLAG_OUTPUT_TRIM) whttpd_str_trim(&(RCA->OutputBuf));
							if(whttpd_RCAItems[RCAItemIdx].Flags & WHTTPD_RCAFLAG_OUTPUT_TRIM_QUOTES) whttpd_str_trim_quotes(&(RCA->OutputBuf));
							if(whttpd_RCAItems[RCAItemIdx].Flags & WHTTPD_RCAFLAG_OUTPUT_ADDMPB2H){ //special just for multipart boundary - add two hyphens at the beginning (<FULL-BOUNDARY> is --<boundary>)
								uint8_t MPBLen = strlen(RCA->OutputBuf);
								if(MPBLen+2 >= RCA->OutputBufSize){ //not enough space in OutputBuf for adding two hyphens => make more space
									//
									char* NewPtr = realloc(RCA->OutputBuf, RCA->OutputBufSize+WHTTPD_RCA_OUTPUTBUF_MALLOC_STEP);
									if(NewPtr==NULL){
										free(RCA->OutputBuf);
										RCA->OutputBuf = NULL;
										RCA->OutputBufSize = 0;
										RCA->State = WHTTPD_RCA_REALLOC_FAILED;
										break;
									}
									else{
										RCA->OutputBuf = NewPtr;
										RCA->OutputBufSize += WHTTPD_RCA_OUTPUTBUF_MALLOC_STEP;
									}
								}
								memmove(&RCA->OutputBuf[2], &RCA->OutputBuf[0], MPBLen+1); //shift by 2 chars to right (including null termination)
								RCA->OutputBuf[0]='-';
								RCA->OutputBuf[1]='-';
							}
							//from now on you can use RCA->OutputBuf as standard null terminated string
						}
						if(whttpd_RCAItems[RCAItemIdx].Flags & WHTTPD_RCAFLAG_SIGNAL_DOUBLE_CRLF){
							(*RecvBufPos)++;
							return WHTTPD_RCARESFLAG_DOUBLE_CRLF; //we've just matched whole suffix of special usage item, exit and return WHTTPD_RCARESFLAG_DOUBLE_CRLF
						}
						if(whttpd_RCAItems[RCAItemIdx].Flags & WHTTPD_RCAFLAG_SIGNAL_MP_BOUNDARY){
							(*RecvBufPos)++;
							return WHTTPD_RCARESFLAG_MP_BOUNDARY; // ... WHTTPD_RCARESFLAG_MP_BOUNDARY
						}
					}
					break;
				case WHTTPD_RCA_AFTER_SUFFIX:
					break;
				case WHTTPD_RCA_REALLOC_FAILED:
					break;
			}
		}
		//
		(*RecvBufPos)++;
	}
	return 0;
}

char* ICACHE_FLASH_ATTR whttpd_rca_get_item_outputbuf_ptr(int8_t SIdx, uint8_t RCAItemIdx){
	if(whttpd_Slots[SIdx].RCA[RCAItemIdx].State == WHTTPD_RCA_AFTER_SUFFIX) return whttpd_Slots[SIdx].RCA[RCAItemIdx].OutputBuf;
	else return NULL;
}

//---- slot management

void ICACHE_FLASH_ATTR whttpd_slot_init(void){
//Call only once on WHTTPD startup.
	int8_t SIdx;
	for(SIdx=0;SIdx<WHTTPD_MAX_CONNECTIONS;SIdx++){
		memset((uint8_t*)&whttpd_Slots[SIdx], 0, sizeof(whttpd_slot_struct)); //fills everything by 0 (all flags are cleared)
		whttpd_Slots[SIdx].AcptSck = -1; //mark as unused slot
	}
}

int8_t /*ICACHE_FLASH_ATTR*/ whttpd_slot_get_unused_idx(void){ //called very often
	int8_t SIdx;
	for(SIdx=0;SIdx<WHTTPD_MAX_CONNECTIONS;SIdx++) if(!WHTTPD_IS_VALID_SOCKFD(whttpd_Slots[SIdx].AcptSck)) return SIdx;
	return WHTTPD_NO_SLOT_IDX;
}

int8_t ICACHE_FLASH_ATTR whttpd_slot_init_after_accept(int8_t SIdx){
	if(!WHTTPD_IS_VALID_SOCKFD(whttpd_Slots[SIdx].AcptSck)) return -1;
	DBG_WHTTPD("slot[%d]: <- conn. %d accepted from %d.%d.%d.%d\n", SIdx, whttpd_Slots[SIdx].AcptSck, ((uint8_t*)&whttpd_Slots[SIdx].AcptAddr.sin_addr)[0], ((uint8_t*)&whttpd_Slots[SIdx].AcptAddr.sin_addr)[1], ((uint8_t*)&whttpd_Slots[SIdx].AcptAddr.sin_addr)[2], ((uint8_t*)&whttpd_Slots[SIdx].AcptAddr.sin_addr)[3]);
	//
	whttpd_Slots[SIdx].IdleLoopsCtr=0;
#ifdef DO_DEBUG_WHTTPD_RECV_DATA
	whttpd_Slots[SIdx].RecvBuf = malloc(WHTTPD_RECV_BUF_LEN+1); //for null termination
#else
	whttpd_Slots[SIdx].RecvBuf = malloc(WHTTPD_RECV_BUF_LEN);
#endif
	whttpd_Slots[SIdx].RecvBufBytes = 0;
#ifdef DO_DEBUG_WHTTPD_SENT_DATA
	whttpd_Slots[SIdx].SendBuf = malloc(WHTTPD_SEND_BUF_LEN+1); //for null termination
#else
	whttpd_Slots[SIdx].SendBuf = malloc(WHTTPD_SEND_BUF_LEN);
#endif

	whttpd_Slots[SIdx].SendBufBytes = 0;
	whttpd_Slots[SIdx].Flags = 0; //clear all flags
	//
	if((whttpd_Slots[SIdx].RecvBuf==NULL)||(whttpd_Slots[SIdx].SendBuf==NULL)){
		DBG_WHTTPD("slot[%d]: can't allocate memory for receive/send buffers, forcing conn. %d close\n", SIdx, whttpd_Slots[SIdx].AcptSck);
		whttpd_Slots[SIdx].Flags |= WHTTPD_SLOTFLAG_FORCE_CLOSE; //mark slot/connection for closing
		return -1;
	}
	//
	whttpd_Slots[SIdx].RCARecvBufPos=0;
	//.RCA[] is inited separately using whttpd_rca_init(...)
	whttpd_Slots[SIdx].RqType = WHTTPD_RQ_TYPE_UNKNOWN;
	whttpd_Slots[SIdx].RqCRLFSize = 2; //default newline is "\r\n"
	whttpd_Slots[SIdx].RqBodyContentLen = 0;
	whttpd_Slots[SIdx].RqBodyRecvBytes = 0;
	whttpd_Slots[SIdx].RqFlags = 0; //clear all flags
	whttpd_Slots[SIdx].RqLastPostItemIdx = WHTTPD_POST_INVALID_ITEM_INDEX;
	//
	whttpd_Slots[SIdx].FEvtItemIdx = WHTTPD_FEVT_INVALID_ITEM_INDEX;
	//
	whttpd_Slots[SIdx].RqParams = NULL;
	//
	whttpd_Slots[SIdx].RespFIdx = WFOF_INVALID_INDEX;
	whttpd_Slots[SIdx].RespFSize = 0;
	whttpd_Slots[SIdx].RespFOffs = 0;
	//
	//.SSFPreproc - members are inited later in the code when we know the values (.SSFPreproc is used by preproc)
	//
	whttpd_Slots[SIdx].SSFPost.PostDataPtr = NULL;
	whttpd_Slots[SIdx].SSFPost.FormInputName = NULL;
	whttpd_Slots[SIdx].SSFPost.FileName = NULL;
	whttpd_discard_truncpostdata_if_any_and_clear_flags(SIdx);
	//
	return 0;
}

void ICACHE_FLASH_ATTR whttpd_slot_cleanup_after_send_all(int8_t SIdx){
	if(!WHTTPD_IS_VALID_SOCKFD(whttpd_Slots[SIdx].AcptSck)) return;
	DBG_WHTTPD("slot[%d]: all data sent, cleaning-up for next request\n", SIdx);
	//
	whttpd_Slots[SIdx].RecvBufBytes = 0; //only reset, don't free() buffers here, we need to be ready for next request using the same connection (we free buffers in whttpd_slot_free_after_close(...))
	whttpd_Slots[SIdx].SendBufBytes = 0; // ...
	whttpd_Slots[SIdx].Flags = 0; //clear all flags
	//
	whttpd_Slots[SIdx].RCARecvBufPos=0;
	//.RCA[] is inited separately using whttpd_rca_init(...)
	whttpd_Slots[SIdx].RqType = WHTTPD_RQ_TYPE_UNKNOWN;
	whttpd_Slots[SIdx].RqCRLFSize = 2; //default newline is "\r\n"
	whttpd_Slots[SIdx].RqBodyContentLen = 0;
	whttpd_Slots[SIdx].RqBodyRecvBytes = 0;
	whttpd_Slots[SIdx].RqFlags = 0; //clear all flags
	whttpd_Slots[SIdx].RqLastPostItemIdx = WHTTPD_POST_INVALID_ITEM_INDEX;
	//
	whttpd_Slots[SIdx].FEvtItemIdx = WHTTPD_FEVT_INVALID_ITEM_INDEX;
	//
	if(whttpd_Slots[SIdx].RqParams!=NULL){ //free whttpd_Slots[SIdx].RqParams if allocated when we analyzed the request
		free(whttpd_Slots[SIdx].RqParams);
		whttpd_Slots[SIdx].RqParams = NULL;
	}
	//
	//no need to clear whttpd_Slots[SIdx].RespF*
	//
	//.SSFPreproc - members are inited later in the code when we know the values (.SSFPreproc is used by preproc)
	//
	whttpd_Slots[SIdx].SSFPost.PostDataPtr = NULL;
	whttpd_Slots[SIdx].SSFPost.FormInputName = NULL;
	whttpd_Slots[SIdx].SSFPost.FileName = NULL;
	whttpd_discard_truncpostdata_if_any_and_clear_flags(SIdx);
}

void ICACHE_FLASH_ATTR whttpd_slot_free_after_close(int8_t SIdx){
	if(!WHTTPD_IS_VALID_SOCKFD(whttpd_Slots[SIdx].AcptSck)) return;
	DBG_WHTTPD("slot[%d]: conn. %d closed, clean-up\n", SIdx, whttpd_Slots[SIdx].AcptSck);
	//
	whttpd_Slots[SIdx].AcptSck = -1; //mark as unused slot
	free(whttpd_Slots[SIdx].RecvBuf);
	free(whttpd_Slots[SIdx].SendBuf);
	whttpd_Slots[SIdx].Flags = 0; //clear all flags
	//
	//no need to clear all the whttpd_Slots[SIdx].* vars, it will be cleared/set in whttpd_slot_init_after_accept(...), just free allocated memory (if any)
	if(whttpd_Slots[SIdx].RqParams!=NULL){ //free whttpd_Slots[SIdx].RqParams if allocated when we analyzed the request
		free(whttpd_Slots[SIdx].RqParams);
		whttpd_Slots[SIdx].RqParams = NULL;
	}
	whttpd_discard_truncpostdata_if_any_and_clear_flags(SIdx);
}

//---- main task of WHTTPD / socket handling

void /*ICACHE_FLASH_ATTR*/ whttpd_main_task(void* pvParameters){
//WHTTPD server main task for RTOS.
	DBG_WHTTPD("task started\n");
	//
	int Sck = -1;
	uint8_t TryCtr;
	struct sockaddr_in SAddr;
	int8_t SIdx = WHTTPD_NO_SLOT_IDX;
	uint8_t FIdx;
	uint32_t FSize;
	char* HdrS = NULL;
	//
	while(1){ //we use this only to have handy possibility to do "break" and skip to the clean-up section at the end
		//
		//create socket, fill Sck
		TryCtr = 0;
		while((Sck = lwip_socket(AF_INET, SOCK_STREAM, 0)) < 0){ //return value is the socket descriptor, or -1 in case of error
			DBG_WHTTPD("lwip_socket() failed\n");
			if(TryCtr++>=WHTTPD_INIT_RETRY_MAX_CNT) break;
			vTaskDelay(WHTTPD_INIT_RETRY_DELAY);
		}
		if(TryCtr>=WHTTPD_INIT_RETRY_MAX_CNT) break;
		//
		//bind Sck socket to port WHTTPD_PORT
		memset(&SAddr, 0, sizeof(SAddr));
		SAddr.sin_family = AF_INET;
		SAddr.sin_addr.s_addr = INADDR_ANY;
		SAddr.sin_len = sizeof(SAddr);
		SAddr.sin_port = htons(WHTTPD_PORT);
		TryCtr = 0;
		while(lwip_bind(Sck, (struct sockaddr*)&SAddr, sizeof(SAddr)) < 0){ //0 on success and -1 on failure
			DBG_WHTTPD("lwip_bind() failed\n");
			if(TryCtr++>=WHTTPD_INIT_RETRY_MAX_CNT) break;
			vTaskDelay(WHTTPD_INIT_RETRY_DELAY);
		}
		if(TryCtr>=WHTTPD_INIT_RETRY_MAX_CNT) break;
		/*
		 * Start listening to all incoming connections for Sck.
		 * Note: the listen backlog (second parameter in listen(...)) is a queue which is used by the operating system to store connections
		 * that have been accepted by the TCP stack but not yet by your program. Conceptually, when a client connects it's placed in this queue
		 * until your accept() code removes it and hands it to your program. As such, the listen backlog is a tuning parameter that can be used
		 * to help your server handle peaks in concurrent connection attempts. Note that this is concerned with peaks in concurrent connection
		 * attempts and in no way related to the maximum number of concurrent connections that your server can maintain.
		 */
		TryCtr = 0;
		while(lwip_listen(Sck, WHTTPD_LISTEN_BACKLOG) < 0){ //0 on success and -1 on failure
			DBG_WHTTPD("lwip_listen() failed\n");
			if(TryCtr++>=WHTTPD_INIT_RETRY_MAX_CNT) break;
			vTaskDelay(WHTTPD_INIT_RETRY_DELAY);
		}
		if(TryCtr>=WHTTPD_INIT_RETRY_MAX_CNT) break;
		//
		//Let's make accept(...) non-blocking, but only non-blocking reads are implemented in lwip :/
		//However lwIP does have receive timeouts, which also affect accepts ;)
		int Opt = 1; //[ms]
		lwip_setsockopt(Sck, SOL_SOCKET, SO_RCVTIMEO, &Opt, sizeof(Opt));
		//
		//init vars, slots, enter main loop
		if(wfof_get_file_info("__WHTTPD_header.txt", &FIdx, &FSize) < 0){ //0 on success and -1 on failure
			DBG_WHTTPD("file '__WHTTPD_header.txt' not found\n");
			break;
		}
		HdrS = malloc(FSize+1);
		if(HdrS==NULL){
			DBG_WHTTPD("can't allocate memory for '__WHTTPD_header.txt'\n");
			break;
		}
		if(wfof_get_file_data(FIdx, HdrS, 0, FSize) < FSize){
			DBG_WHTTPD("can't read entire '__WHTTPD_header.txt' (wfof_get_file_data(...) returned less bytes than expected)\n");
			break;
		}
		HdrS[FSize]=0; //add null termination
		//
		whttpd_slot_init();
		whttpd_preproc_first_init();
		printf("whttpd: server %s ready and listening on port %d\n", WHTTPD_VER, WHTTPD_PORT);
		while(1){ //main loop of the WHTTPD
			//
			//check if we have any unused slot for possible new connection and if yes, accept waiting connection (if any) into that unused slot
			SIdx = whttpd_slot_get_unused_idx();
			if(SIdx!=WHTTPD_NO_SLOT_IDX){ //ok, we have an unused slot
				socklen_t AcptAddrLen = sizeof(struct sockaddr);
				whttpd_Slots[SIdx].AcptSck = lwip_accept(Sck, (struct sockaddr*)&whttpd_Slots[SIdx].AcptAddr, &AcptAddrLen); //non-blocking, return value is the newly created accepted connection descriptor, or -1 in case of no waiting connection (timeout) / error
				if(WHTTPD_IS_VALID_SOCKFD(whttpd_Slots[SIdx].AcptSck)){ //we have just accepted new connection, init slot & RCA
					whttpd_slot_init_after_accept(SIdx);
					whttpd_rca_init(SIdx); //init received chunk analyzer (client's request analyzer)
				}
			}
			//
			//manage slots / manage all opened connections (read data if any)
			uint8_t AllSlotsUnused = 1;
			for(SIdx=0;SIdx<WHTTPD_MAX_CONNECTIONS;SIdx++){ //go through all slots
				if(!WHTTPD_IS_VALID_SOCKFD(whttpd_Slots[SIdx].AcptSck)) continue; //skip unused slots
				AllSlotsUnused = 0;
				//
				//read data received by the connection (if any)
				whttpd_Slots[SIdx].RecvBufBytes = lwip_recv(whttpd_Slots[SIdx].AcptSck, whttpd_Slots[SIdx].RecvBuf, WHTTPD_RECV_BUF_LEN, MSG_DONTWAIT); //non-blocking, returns -1 when nothing to read / 0 when connection lost or closed by client / positive number representing number of bytes read
				if(whttpd_Slots[SIdx].RecvBufBytes>0){ //we've received data
					DBG_WHTTPD_V("slot[%d]: received %d bytes\n", SIdx, whttpd_Slots[SIdx].RecvBufBytes);
					whttpd_Slots[SIdx].IdleLoopsCtr = 0; //reset timeout counter
#ifdef DO_DEBUG_WHTTPD_RECV_DATA
					printf("whttpd: slot[%d]: ---- recv data ----\n", SIdx);
					whttpd_Slots[SIdx].RecvBuf[whttpd_Slots[SIdx].RecvBufBytes] = 0; //add null termination (there's always space if #ifdef DO_DEBUG_WHTTPD_RECV_DATA - see how .RecvBuf is allocated)
					printf(whttpd_Slots[SIdx].RecvBuf);
					printf("\nwhttpd: slot[%d]: -------------------\n", SIdx);
#endif
					//
					whttpd_Slots[SIdx].RCARecvBufPos = 0;
					uint8_t RCAResFlags = 0; //combination of WHTTPD_RCARESFLAG_*
					if(whttpd_Slots[SIdx].RqFlags & WHTTPD_RQFLAG_IN_MPART_DATA) whttpd_Slots[SIdx].SSFPost.PostDataPtr = whttpd_Slots[SIdx].RecvBuf;
					else whttpd_Slots[SIdx].SSFPost.PostDataPtr = NULL;
					//
					do{ //loop for analyzing received data / analyzing POST data / passing POST data to proper POST data callback functions
						uint16_t OldRCARecvBufPos = whttpd_Slots[SIdx].RCARecvBufPos;
						//
						RCAResFlags = whttpd_rca_analyze(SIdx, (uint16_t*)&whttpd_Slots[SIdx].RCARecvBufPos); //.RCARecvBufPos is changed
						//
						if(whttpd_Slots[SIdx].RqType!=WHTTPD_RQ_TYPE_UNKNOWN) whttpd_Slots[SIdx].RqBodyRecvBytes += whttpd_Slots[SIdx].RCARecvBufPos-OldRCARecvBufPos; //count bytes of body that we've already analyzed
						whttpd_pass_post_data_from_truncpostdata_if_requested(SIdx);
						//
						if((whttpd_Slots[SIdx].RqType==WHTTPD_RQ_TYPE_UNKNOWN)&&(RCAResFlags & WHTTPD_RCARESFLAG_DOUBLE_CRLF)){ //end of HTTP header
							//
							//remember CRLF style as it's used in header that we got from client
							if(whttpd_Slots[SIdx].RCA[WHTTPD_RCAITEM_CRLF1_DETECT].State==WHTTPD_RCA_AFTER_SUFFIX) whttpd_Slots[SIdx].RqCRLFSize = 1;
							if(whttpd_Slots[SIdx].RCA[WHTTPD_RCAITEM_CRLF2_DETECT].State==WHTTPD_RCA_AFTER_SUFFIX) whttpd_Slots[SIdx].RqCRLFSize = 2;
							//
							//is it GET/POST? Set .RqType, for GET set WHTTPD_SLOTFLAG_RCV_RQ_ALL flag immediately, for POST prepare for receiving multipart POST data, decide .FEvtItemIdx
							char* Rq = NULL;
							if((Rq = whttpd_rca_get_item_outputbuf_ptr(SIdx, WHTTPD_RCAITEM_GET)) != NULL){ //RCA[WHTTPD_RCAITEM_GET] = GET method Request-URI
								DBG_WHTTPD("slot[%d]: GET request received (free heap: %d)\n", SIdx, system_get_free_heap_size());
								whttpd_Slots[SIdx].RqType = WHTTPD_RQ_TYPE_GET;
								whttpd_Slots[SIdx].Flags |= WHTTPD_SLOTFLAG_RCV_RQ_ALL;
							}
							else if((Rq = whttpd_rca_get_item_outputbuf_ptr(SIdx, WHTTPD_RCAITEM_POST)) != NULL){ //RCA[WHTTPD_RCAITEM_POST] = POST method Request-URI
								DBG_WHTTPD("slot[%d]: POST request received (free heap: %d)\n", SIdx, system_get_free_heap_size());
								whttpd_Slots[SIdx].RqType = WHTTPD_RQ_TYPE_POST;
								//
								whttpd_rca_free_and_reinit_item(SIdx, WHTTPD_RCAITEM_CRLF2_DETECT);
								whttpd_rca_free_and_reinit_item(SIdx, WHTTPD_RCAITEM_CRLF1_DETECT);
								whttpd_rca_free_and_reinit_item(SIdx, WHTTPD_RCAITEM_FORM_INPUT_NAME);
								whttpd_rca_free_and_reinit_item(SIdx, WHTTPD_RCAITEM_UPLD_FILE_NAME);
								whttpd_rca_free_and_reinit_item(SIdx, WHTTPD_RCAITEM_MP_BOUNDARY_DETECT);
								whttpd_RCAItems[WHTTPD_RCAITEM_MP_BOUNDARY_DETECT].Suffix = whttpd_rca_get_item_outputbuf_ptr(SIdx, WHTTPD_RCAITEM_MP_BOUNDARY); //set multipart boundary value for multipart boundary detector
								//
								char* S = whttpd_rca_get_item_outputbuf_ptr(SIdx, WHTTPD_RCAITEM_CONT_LEN); //content length (string representation)
								if(S!=NULL) whttpd_Slots[SIdx].RqBodyContentLen = strtol(S,NULL,0);
								//
							}
							//
							if(Rq!=NULL){ //we've just got Request-URI from the client's request header
								char* RqF = NULL;
								whttpd_str_split(Rq, &RqF, NULL, '?', 1); //split Rq (Request-URI) to RqF, RqParams by delimiter '?' (allocates memory - don't forget to free(...))
								whttpd_Slots[SIdx].FEvtItemIdx = whttpd_fevt_get_item_idx( ((RqF[0]=='/')&&(RqF[1]==0)) ? WHTTPD_DEFAULT_PAGE : ((RqF[0]=='/')?&RqF[1]:&RqF[0]) );
								if(RqF!=NULL) free(RqF);
								//
								if(whttpd_Slots[SIdx].FEvtItemIdx!=WHTTPD_FEVT_INVALID_ITEM_INDEX) whttpd_fevt_call_cb_req_hdr_received(whttpd_Slots[SIdx].FEvtItemIdx);
							}
							//
#ifdef DO_DEBUG_WHTTPD_VERBOSE
							uint8_t TC1;
							for(TC1=0;TC1<WHTTPD_RCA_ITEMS_CNT;TC1++){
								char* S = whttpd_rca_get_item_outputbuf_ptr(SIdx, TC1);
								if(S!=NULL) DBG_WHTTPD_V("-> RCA[%d].OutputBuf = '%s'\n", TC1, S);
							}
#endif
						}
						else if((whttpd_Slots[SIdx].RqType==WHTTPD_RQ_TYPE_POST)&&(!(whttpd_Slots[SIdx].RqFlags & WHTTPD_RQFLAG_IN_MPART_HEADER))&&(RCAResFlags & WHTTPD_RCARESFLAG_MP_BOUNDARY)){ //we're getting POST data and we've just encountered multipart boundary separator
							//now .RecvBuf[.RCARecvBufPos] points to next char (no range checking) after last char of .Suffix of boundary detector (RCA[WHTTPD_RCAITEM_MP_BOUNDARY_DETECT])
							whttpd_Slots[SIdx].RqFlags |= WHTTPD_RQFLAG_IN_MPART_HEADER;
							if(whttpd_Slots[SIdx].RqFlags & WHTTPD_RQFLAG_IN_MPART_DATA) whttpd_Slots[SIdx].RqFlags |= WHTTPD_RQFLAG_WAS_IN_MPART_DATA;
							whttpd_Slots[SIdx].RqFlags &= NOT_FLAG8(WHTTPD_RQFLAG_IN_MPART_DATA);
 							//
							whttpd_pass_post_data_from_recvbuf_if_any(SIdx); //! behaves according to state of various members in whttpd_Slots[SIdx]
							whttpd_finish_pass_post_data_if_needed(SIdx); //prev line will not produce ended data (WHTTPD_POST_CBFLAG_IS_LAST_DATA_BLOCK) in some special cases. We need to call this (it has no effect if prev. line produces ended data)
							//
							whttpd_rca_free_and_reinit_item(SIdx, WHTTPD_RCAITEM_CRLF2_DETECT);
							whttpd_rca_free_and_reinit_item(SIdx, WHTTPD_RCAITEM_CRLF1_DETECT);
							whttpd_rca_free_and_reinit_item(SIdx, WHTTPD_RCAITEM_FORM_INPUT_NAME);
							whttpd_rca_free_and_reinit_item(SIdx, WHTTPD_RCAITEM_UPLD_FILE_NAME);
							whttpd_rca_free_and_reinit_item(SIdx, WHTTPD_RCAITEM_MP_BOUNDARY_DETECT);
							//
							whttpd_Slots[SIdx].SSFPost.PostDataPtr = NULL;
							whttpd_Slots[SIdx].SSFPost.FormInputName = NULL;
							whttpd_Slots[SIdx].SSFPost.FileName = NULL;
						}
						else if((whttpd_Slots[SIdx].RqType==WHTTPD_RQ_TYPE_POST)&&(whttpd_Slots[SIdx].RqFlags & WHTTPD_RQFLAG_IN_MPART_HEADER)&&(RCAResFlags & WHTTPD_RCARESFLAG_DOUBLE_CRLF)){ //end of multipart data header
							//now .RecvBuf[.RCARecvBufPos] points to next char (no range checking) after last char of .Suffix of CRLF detector (RCA[WHTTPD_RCAITEM_CRLF2_DETECT] / RCA[WHTTPD_RCAITEM_CRLF1_DETECT])
							whttpd_Slots[SIdx].RqFlags &= NOT_FLAG8(WHTTPD_RQFLAG_IN_MPART_HEADER);
							whttpd_Slots[SIdx].RqFlags |= WHTTPD_RQFLAG_IN_MPART_DATA;
							//
							whttpd_rca_free_and_reinit_item(SIdx, WHTTPD_RCAITEM_MP_BOUNDARY_DETECT);
							//
							if(whttpd_Slots[SIdx].RCARecvBufPos<whttpd_Slots[SIdx].RecvBufBytes) whttpd_Slots[SIdx].SSFPost.PostDataPtr = &whttpd_Slots[SIdx].RecvBuf[whttpd_Slots[SIdx].RCARecvBufPos];
							else whttpd_Slots[SIdx].SSFPost.PostDataPtr = NULL;
							whttpd_Slots[SIdx].SSFPost.FormInputName = whttpd_rca_get_item_outputbuf_ptr(SIdx, WHTTPD_RCAITEM_FORM_INPUT_NAME);
							whttpd_Slots[SIdx].SSFPost.FileName = whttpd_rca_get_item_outputbuf_ptr(SIdx, WHTTPD_RCAITEM_UPLD_FILE_NAME);
							//
							whttpd_Slots[SIdx].RqFlags &= NOT_FLAG8(WHTTPD_RQFLAG_MPART_DATA_CB_ALREADY_CALLED);
						}
						//

					} while(whttpd_Slots[SIdx].RCARecvBufPos<whttpd_Slots[SIdx].RecvBufBytes); //continue in received data analysis if there's more data
					//now .RCARecvBufPos is pointing one char beyond .RecvBuf
					//
					whttpd_pass_post_data_from_truncpostdata_if_requested(SIdx);
					if(whttpd_Slots[SIdx].RqType==WHTTPD_RQ_TYPE_POST){
						whttpd_pass_post_data_from_recvbuf_if_any(SIdx); //! behaves according to state of various members in whttpd_Slots[SIdx]
						//
						if((whttpd_Slots[SIdx].RqBodyContentLen>0)&&(whttpd_Slots[SIdx].RqBodyRecvBytes==whttpd_Slots[SIdx].RqBodyContentLen)){
							whttpd_finish_pass_post_data_if_needed(SIdx);
							whttpd_Slots[SIdx].Flags |= WHTTPD_SLOTFLAG_RCV_RQ_ALL;
						}
					}
					//
					//if we've just got entire request from the client, start response processing
					if(whttpd_Slots[SIdx].Flags & WHTTPD_SLOTFLAG_RCV_RQ_ALL){
						whttpd_Slots[SIdx].Flags &= NOT_FLAG8(WHTTPD_SLOTFLAG_RCV_RQ_ALL); //clear flag WHTTPD_SLOTFLAG_RCV_RQ_ALL
						whttpd_Slots[SIdx].RespFIdx = WFOF_INVALID_INDEX;
						//
						if(whttpd_Slots[SIdx].RqParams!=NULL){ //just to be sure - if for some reason the actions related with WHTTPD_SLOTFLAG_RESPONSE_END didn't happen and this is second+ usage of the opened connection
							free(whttpd_Slots[SIdx].RqParams);
							whttpd_Slots[SIdx].RqParams = NULL;
						}
						whttpd_Slots[SIdx].SSFPreproc.RqParams = NULL; //reset SSFPreproc.RqParams - no request params
						//
						if(whttpd_Slots[SIdx].FEvtItemIdx!=WHTTPD_FEVT_INVALID_ITEM_INDEX) whttpd_fevt_call_cb_req_finished(whttpd_Slots[SIdx].FEvtItemIdx);
						//
						uint16_t ReportError = 200; //HTTP error code. ReportError==200 => all OK, don't report error; ReportError!=200 => report error using "__WHTTPD_err_resp.html"
						char* Rq = NULL;
						if(whttpd_Slots[SIdx].RqType==WHTTPD_RQ_TYPE_GET) Rq = whttpd_rca_get_item_outputbuf_ptr(SIdx, WHTTPD_RCAITEM_GET); //RCA[WHTTPD_RCAITEM_GET] = GET method Request-URI, get pointer to Request-URI string into Rq
						if(whttpd_Slots[SIdx].RqType==WHTTPD_RQ_TYPE_POST) Rq = whttpd_rca_get_item_outputbuf_ptr(SIdx, WHTTPD_RCAITEM_POST); //RCA[WHTTPD_RCAITEM_POST] = POST method Request-URI, get pointer to Request-URI string into Rq
						if(Rq!=NULL){ //we've got Request-URI from the client's request header
							char* RqF = NULL;
							char* RqFU = NULL;
							//
							whttpd_str_split(Rq, &RqF, (char**)&whttpd_Slots[SIdx].RqParams, '?', 0); //split Rq (Request-URI) to RqF, RqParams by delimiter '?' (allocates memory - don't forget to free(...))
							whttpd_Slots[SIdx].SSFPreproc.RqParams = whttpd_Slots[SIdx].RqParams;
							whttpd_str_uppercase(&RqFU, RqF); //make an uppercased copy of RqF into RqFU (allocates memory - don't forget to free(...)). Yes, we could use strcasestr later, but there are obscurities about strcasestr provided by ESP libraries
							DBG_WHTTPD("slot[%d]: RqF = '%s', RqParams = '%s'\n", SIdx, RqF, whttpd_Slots[SIdx].RqParams);
							DBG_WHTTPD_V("slot[%d]: RqFU = '%s'\n", SIdx, RqFU);
							//
							if(whttpd_Slots[SIdx].FEvtItemIdx!=WHTTPD_FEVT_INVALID_ITEM_INDEX) whttpd_fevt_call_cb_resp_started(whttpd_Slots[SIdx].FEvtItemIdx);
							//
							//fill FIdx, FSize for requested file (Request-URI), generate response header and set flags / set ReportError = 404 if requested file not found
							if(wfof_get_file_info( ((RqF[0]=='/')&&(RqF[1]==0)) ? WHTTPD_DEFAULT_PAGE : ((RqF[0]=='/')?&RqF[1]:&RqF[0]), &FIdx, &FSize) == 0){ //0 on success and -1 on failure
								//ok, we have requested file in the WFOF system and FIdx, FSize are filled
								/*
								 * Detect multiple request for the same resource from the same client and force closing of older connection(s).
								 * This is here because downloading of flash content don't work without this "hack". Client (various browsers)
								 * asked for the same resource multiple times 2 or 3 times. Why?!? Closing younger requests causes
								 * download termination. We have to close older ones.
								 */
								int8_t SIdx2;
								for(SIdx2=0;SIdx2<WHTTPD_MAX_CONNECTIONS;SIdx2++){ //go through all slots
									if((SIdx2!=SIdx)&&(whttpd_Slots[SIdx2].Flags!=0)&&(whttpd_Slots[SIdx2].AcptAddr.sin_addr.s_addr==whttpd_Slots[SIdx].AcptAddr.sin_addr.s_addr)&&(whttpd_Slots[SIdx2].RespFIdx==FIdx)){
										DBG_WHTTPD("slot[%d]: multiple request for the same resource from the same client - forcing close of older connection in slot[%d]\n", SIdx, SIdx2);
										whttpd_Slots[SIdx2].Flags |= WHTTPD_SLOTFLAG_FORCE_CLOSE;
									}
								}
								//
								//decide content type according to requested file extension
								char* ContType = NULL;
								while(1){ //to be able to use "break" and jump beyond the content type decision immediately
									if(((RqF[0]=='/')&&(RqF[1]==0))||(strstr(RqFU,".HTML")!=NULL)||(strstr(RqFU,".HTM")!=NULL)){ ContType = "text/html"; whttpd_Slots[SIdx].Flags |= WHTTPD_SLOTFLAG_USE_PREPROC; break; }
									if(strstr(RqFU,".CSS")!=NULL){ ContType = "text/css"; whttpd_Slots[SIdx].Flags |= WHTTPD_SLOTFLAG_USE_PREPROC; break; }
									if(strstr(RqFU,".JS")!=NULL){ ContType = "text/javascript"; whttpd_Slots[SIdx].Flags |= WHTTPD_SLOTFLAG_USE_PREPROC; break; }
									if(strstr(RqFU,".PNG")!=NULL){ ContType = "image/png"; break; }
									if((strstr(RqFU,".JPG")!=NULL)||(strstr(RqFU,".JPEG")!=NULL)){ ContType = "image/jpeg"; break; }
									if(strstr(RqFU,".GIF")!=NULL){ ContType = "image/gif"; break; }
									if(strstr(RqFU,".ICO")!=NULL){ ContType = "image/x-icon"; break; }
									if(strstr(RqFU,".BIN")!=NULL){ ContType = "application/octet-stream"; whttpd_Slots[SIdx].Flags |= WHTTPD_SLOTFLAG_USE_PREPROC; break; }
									if(strstr(RqFU,".ZIP")!=NULL){ ContType = "application/zip"; break; }
									if((strstr(RqFU,".GZ")!=NULL)||(strstr(RqFU,".GZIP")!=NULL)){ ContType = "application/x-gzip"; break; }
									break;
								}
								if(ContType==NULL) ContType = "text/plain";
								//
								//generate response header, set flag to enable sending requested file data
								whttpd_Slots[SIdx].SendBufBytes = sprintf(whttpd_Slots[SIdx].SendBuf, HdrS, 200, WHTTPD_ERR_MSG_200, WHTTPD_VER, ContType, "");
								whttpd_Slots[SIdx].Flags |= WHTTPD_SLOTFLAG_FILL_SENDBUF;
								//now FIdx and FSize are set to the requested file (last call of wfof_get_file_info(...))
							}
							else ReportError = 404; //requested file is not in the WFOF system
							//
							//clean-up
							if(RqF!=NULL) free(RqF);
							if(RqFU!=NULL) free(RqFU);
							//don't free whttpd_Slots[SIdx].RqParams here - we want to keep it at least until the end of response (for possible preprocessor actions) => we'll free it when entire response has been sent (see WHTTPD_SLOTFLAG_RESPONSE_END) OR in whttpd_slot_free_after_close(...) OR if the opened connection is used again
						}
						else ReportError = 400; //we don't have Request-URI from the client's request header
						//
						if(ReportError!=200){
							//fill FIdx, FSize for "__WHTTPD_err_resp.html", generate response header and set flags / generate failsafe "500 - Internal Server Error" response if "__WHTTPD_err_resp.html" not found
							if(wfof_get_file_info("__WHTTPD_err_resp.html", &FIdx, &FSize) == 0){ //0 on success and -1 on failure
								//generate response header, set flag to enable sending requested file data
								switch(ReportError){
									case 400: whttpd_Slots[SIdx].SendBufBytes = sprintf(whttpd_Slots[SIdx].SendBuf, HdrS, ReportError, WHTTPD_ERR_MSG_400, WHTTPD_VER, "text/html", ""); break;
									case 404: whttpd_Slots[SIdx].SendBufBytes = sprintf(whttpd_Slots[SIdx].SendBuf, HdrS, ReportError, WHTTPD_ERR_MSG_404, WHTTPD_VER, "text/html", ""); break;
									default: //strange, this shouldn't happen because of how ReportError is filled in the previous logic, but we're writing really reliable code, hm?
										ReportError = 500;
										whttpd_Slots[SIdx].SendBufBytes = sprintf(whttpd_Slots[SIdx].SendBuf, HdrS, ReportError, WHTTPD_ERR_MSG_500, WHTTPD_VER, "text/html", "");
								}
								whttpd_Slots[SIdx].Flags |= WHTTPD_SLOTFLAG_USE_PREPROC|WHTTPD_SLOTFLAG_FILL_SENDBUF;
								//now FIdx and FSize are set to "__WHTTPD_err_resp.html" (last call of wfof_get_file_info(...))
							}
							else{ //"__WHTTPD_err_resp.html" not found (did you delete this file?)
								//generate response header
								ReportError = 500;
								whttpd_Slots[SIdx].SendBufBytes = sprintf(whttpd_Slots[SIdx].SendBuf, HdrS, ReportError, WHTTPD_ERR_MSG_500, WHTTPD_VER, "text/plain", "");
								//
								//add failsafe "500 - Internal Server Error" chunked body to .SendBuf, set "send" & "response-end" flags
								strcpy(&(whttpd_Slots[SIdx].SendBuf[whttpd_Slots[SIdx].SendBufBytes]), WHTTPD_FAILSAFE_500_CHNK_BODY); //!make sure that WHTTPD_SEND_BUF_LEN is big enough to accommodate sprintf-ed response header + WHTTPD_FAILSAFE_500_CHNK_BODY
								whttpd_Slots[SIdx].SendBufBytes = strlen(whttpd_Slots[SIdx].SendBuf);
								whttpd_Slots[SIdx].Flags |= WHTTPD_SLOTFLAG_DO_SEND|WHTTPD_SLOTFLAG_RESPONSE_END;
								//now FIdx==WFOF_INVALID_INDEX and FSize==0 (last call of wfof_get_file_info(...))
							}
						}
						whttpd_Slots[SIdx].SSFPreproc.HTTPRespCode = ReportError;
						DBG_WHTTPD("slot[%d]: response HTTP code %d, %s\n", SIdx, ReportError, (whttpd_Slots[SIdx].Flags & WHTTPD_SLOTFLAG_USE_PREPROC)?"setting preproc":"no preproc");
						//
						whttpd_Slots[SIdx].RespFIdx = FIdx;
						whttpd_Slots[SIdx].RespFSize = FSize;
						whttpd_Slots[SIdx].RespFOffs = 0;
						//now we have full response header in whttpd_Slots[SIdx].SendBuf and whttpd_Slots[SIdx].RespF* are filled
						//(and in very special case: when (ReportError!=200) AND "__WHTTPD_err_resp.html" not found (did you delete this file?) => we have entire response (header + body) + we have set flags WHTTPD_SLOTFLAG_DO_SEND|WHTTPD_SLOTFLAG_RESPONSE_END)
						//
						//reset preprocessor if it should be used
						if(whttpd_Slots[SIdx].Flags & WHTTPD_SLOTFLAG_USE_PREPROC){
							if(whttpd_preproc_set(FIdx, FSize, (whttpd_slot_state_for_preproc_struct*)&whttpd_Slots[SIdx].SSFPreproc) != WHTTPD_PP_SET_RETCODE_PP_IN_USE){ //ok preprocessor set for us
								whttpd_Slots[SIdx].Flags |= WHTTPD_SLOTFLAG_USING_PREPROC;
							}
							else{ //preprocessor in use at the moment => set flag WHTTPD_SLOTFLAG_WAIT_WHILE_PP_IN_USE so we'll wait until it's ready
								DBG_WHTTPD("slot[%d]: preproc in use, waiting\n", SIdx);
								whttpd_Slots[SIdx].Flags |= WHTTPD_SLOTFLAG_WAIT_WHILE_PP_IN_USE;
							}
						}
						//
						whttpd_rca_free_and_reinit(SIdx); //free buffers allocated by whttpd_rca_analyze(...) (if any) and prepare for next request using the same connection => init received chunk analyzer (client's request analyzer)
					}
					//
				}
				else if(whttpd_Slots[SIdx].RecvBufBytes==0){ //connection lost or closed by client
					DBG_WHTTPD("slot[%d]: conn. lost or closed by client, forcing close\n", SIdx);
					whttpd_Slots[SIdx].Flags |= WHTTPD_SLOTFLAG_FORCE_CLOSE; //mark slot/connection for closing
				}
				else if(whttpd_Slots[SIdx].RecvBufBytes==-1){ //nothing to read (idle connection) - manage the idle connection timeout
					if(++whttpd_Slots[SIdx].IdleLoopsCtr>=WHTTPD_MAINLOOP_CONN_IDLE_TIMEOUT){ //we have a connection timeout (some browsers keep connection opened, but because we haven't much resources, we act as Apache - timeout the connection - see WHTTPD_MAINLOOP_CONN_IDLE_TIMEOUT)
						DBG_WHTTPD("slot[%d]: conn. timeout, forcing close\n", SIdx);
						whttpd_Slots[SIdx].Flags |= WHTTPD_SLOTFLAG_FORCE_CLOSE; //mark slot/connection for closing
					}
				}
				//
				//if connection marked for closing => re-init preproc if not properly finished, close connection, free memory used by buffers, free the slot
				if(whttpd_Slots[SIdx].Flags & WHTTPD_SLOTFLAG_FORCE_CLOSE){
					if(whttpd_Slots[SIdx].Flags & WHTTPD_SLOTFLAG_USING_PREPROC){
						DBG_WHTTPD("slot[%d]: forcing close and preproc not finished properly - forcing preproc init\n", SIdx);
						whttpd_preproc_init(); //forcing close and preprocessor didn't signal HaveAll - it didn't re-inited itself => we need to do it here (next call of whttpd_preproc_set(...) will return WHTTPD_PP_SET_RETCODE_OK)
					}
					whttpd_rca_free_and_reinit(SIdx); //free buffers allocated by whttpd_rca_analyze(...) (if any)
					lwip_close(whttpd_Slots[SIdx].AcptSck);
					whttpd_slot_free_after_close(SIdx);
					continue;
				}
				//
				//generate HTTP chunk of data into .SendBuf, set flag WHTTPD_SLOTFLAG_DO_SEND
				if((whttpd_Slots[SIdx].Flags & WHTTPD_SLOTFLAG_FILL_SENDBUF)&&(whttpd_Slots[SIdx].RespFIdx != WFOF_INVALID_INDEX)&&(!(whttpd_Slots[SIdx].Flags & WHTTPD_SLOTFLAG_DO_SEND))){ //we should fill .SendBuf by requested file data AND we know what file to fetch AND flag WHTTPD_SLOTFLAG_DO_SEND not set
					whttpd_Slots[SIdx].IdleLoopsCtr = 0; //reset timeout counter
					//
					uint8_t SkipHTTPChunkGen = 0;
					if((whttpd_Slots[SIdx].Flags & WHTTPD_SLOTFLAG_USE_PREPROC)&&(whttpd_Slots[SIdx].Flags & WHTTPD_SLOTFLAG_WAIT_WHILE_PP_IN_USE)){
						SkipHTTPChunkGen = 1;
						if(whttpd_preproc_set(whttpd_Slots[SIdx].RespFIdx, whttpd_Slots[SIdx].RespFSize, (whttpd_slot_state_for_preproc_struct*)&whttpd_Slots[SIdx].SSFPreproc) == WHTTPD_PP_SET_RETCODE_OK){ //ok, preprocessor if free for use again and we've just set it for this slot response successfully
							DBG_WHTTPD("slot[%d]: preproc free for use again, preproc set\n", SIdx);
							whttpd_Slots[SIdx].Flags |= WHTTPD_SLOTFLAG_USING_PREPROC;
							whttpd_Slots[SIdx].Flags &= NOT_FLAG8(WHTTPD_SLOTFLAG_WAIT_WHILE_PP_IN_USE); //clear flag WHTTPD_SLOTFLAG_WAIT_WHILE_PP_IN_USE
							SkipHTTPChunkGen = 0;
						}
						else{
							DBG_WHTTPD_V("slot[%d]: preproc still not free for use\n", SIdx);
						}
					}
					if(!SkipHTTPChunkGen){
						int16_t EmptySpaceForDataInSendBuf = WHTTPD_SEND_BUF_LEN-whttpd_Slots[SIdx].SendBufBytes-(5+2+5); //-(5+2+5) for "000\r\n", "\r\n" and terminating chunk "0\r\n\r\n" (the terminating chunk will be used only after last data chunk (when we've fetched all data), but we must count with it)
						if(EmptySpaceForDataInSendBuf>0){ //ok, we have space for data in .SendBuf
							uint16_t SendBufChunkStartPos = whttpd_Slots[SIdx].SendBufBytes;
							uint16_t SendBufChunkDataStartPos = SendBufChunkStartPos+5; //offset file data by 5 to have space for chunk size info
							uint32_t RetBytes = 0;
							uint8_t HaveAll = 0;
							if(whttpd_Slots[SIdx].Flags & WHTTPD_SLOTFLAG_USE_PREPROC){ //let's call preprocessor and fill up the free space in .SendBuf
								uint16_t IterRetBytes;
								while((EmptySpaceForDataInSendBuf>0)&&(!HaveAll)){ //we still have space for data in .SendBuf and preprocessor didn't finish yet
									DBG_WHTTPD_V("slot[%d]: gen. HTTP chunk, there's space for %d data bytes in .SendBuf, calling whttpd_preproc_get_data(%d, %p, %d, ...)\n", SIdx, EmptySpaceForDataInSendBuf, whttpd_Slots[SIdx].RespFOffs, &(whttpd_Slots[SIdx].SendBuf[SendBufChunkDataStartPos]), EmptySpaceForDataInSendBuf);
									whttpd_preproc_get_data((uint32_t*)&whttpd_Slots[SIdx].RespFOffs, &(whttpd_Slots[SIdx].SendBuf[SendBufChunkDataStartPos]), EmptySpaceForDataInSendBuf, &IterRetBytes, &HaveAll); //fills .SendBuf, IterRetBytes and HaveAll
									SendBufChunkDataStartPos += IterRetBytes;
									RetBytes += IterRetBytes;
									EmptySpaceForDataInSendBuf -= IterRetBytes;
									if(IterRetBytes==0){ //failsafe for case that whttpd_preproc_get_data(...) returned 0 (probably EmptySpaceForDataInSendBuf was so small that preprocessor couldn't generate any data (in some special cases like flash download where 4 bytes are minimum))
										DBG_WHTTPD_V("slot[%d]: last call of whttpd_preproc_get_data(...) returned 0 bytes, breaking the loop\n", SIdx);
										break;
									}
								}
								//when everything is ok and preprocessor have finished its work, HaveAll is now set and preprocessor have re-inited itself (next call of whttpd_preproc_set(...) will return WHTTPD_PP_SET_RETCODE_OK)
							}
							else{ //get next block of data from the requested file without preprocessing (binary data)
								DBG_WHTTPD_V("slot[%d]: gen. HTTP chunk, there's space for %d data bytes in .SendBuf, calling wfof_get_file_data(%d, %p, %d, %d)\n", SIdx, EmptySpaceForDataInSendBuf, whttpd_Slots[SIdx].RespFIdx, &(whttpd_Slots[SIdx].SendBuf[SendBufChunkDataStartPos]), whttpd_Slots[SIdx].RespFOffs, EmptySpaceForDataInSendBuf);
								RetBytes = wfof_get_file_data(whttpd_Slots[SIdx].RespFIdx, &(whttpd_Slots[SIdx].SendBuf[SendBufChunkDataStartPos]), whttpd_Slots[SIdx].RespFOffs, EmptySpaceForDataInSendBuf);
								whttpd_Slots[SIdx].RespFOffs += RetBytes;
								HaveAll = (whttpd_Slots[SIdx].RespFOffs>=whttpd_Slots[SIdx].RespFSize);
							}
							DBG_WHTTPD_V("slot[%d]: %d data bytes filled into .SendBuf, generating chunk header/footer\n", SIdx, RetBytes);
							whttpd_Slots[SIdx].SendBufBytes += 5+RetBytes;
							//add chunk size info before data from the requested file
							char* ChnkStart = "000\r\n";
							sprintf(ChnkStart, "%03X\r\n", RetBytes);
							strncpy(&(whttpd_Slots[SIdx].SendBuf[SendBufChunkStartPos]), ChnkStart, 5);
							//add chunk end
							strncpy(&(whttpd_Slots[SIdx].SendBuf[whttpd_Slots[SIdx].SendBufBytes]), "\r\n", 2);
							whttpd_Slots[SIdx].SendBufBytes += 2;
							//
							if(HaveAll){ //if we've just got all from the requested file (there will be no more data coming) => add terminating chunk, set flag WHTTPD_SLOTFLAG_RESPONSE_END
								DBG_WHTTPD_V("slot[%d]: we have all data, adding terminating chunk\n", SIdx);
								strncpy(&(whttpd_Slots[SIdx].SendBuf[whttpd_Slots[SIdx].SendBufBytes]), "0\r\n\r\n", 5);
								whttpd_Slots[SIdx].SendBufBytes += 5;
								whttpd_Slots[SIdx].Flags |= WHTTPD_SLOTFLAG_RESPONSE_END;
								if(whttpd_Slots[SIdx].Flags & WHTTPD_SLOTFLAG_USING_PREPROC) whttpd_Slots[SIdx].Flags &= NOT_FLAG8(WHTTPD_SLOTFLAG_USING_PREPROC); //clear flag WHTTPD_SLOTFLAG_USING_PREPROC (preprocessor signaled HaveAll, it re-inited itself (next call of whttpd_preproc_set(...) will return WHTTPD_PP_SET_RETCODE_OK))
							}
							//
							whttpd_Slots[SIdx].Flags |= WHTTPD_SLOTFLAG_DO_SEND;
						}
						else{ //this is really bad, the response header is so big and WHTTPD_SEND_BUF_LEN is so small that we don't have space for data in .SendBuf => change config of WHTTPD
							DBG_WHTTPD("slot[%d]: not enough space for data in .SendBuf, change WHTTPD_SEND_BUF_LEN (or generate smaller header), forcing close\n", SIdx);
							whttpd_Slots[SIdx].Flags |= WHTTPD_SLOTFLAG_FORCE_CLOSE; //mark slot/connection for closing
						}
					}
				}
				//
				//send data & manage flags after sending
				if(whttpd_Slots[SIdx].Flags & WHTTPD_SLOTFLAG_DO_SEND){ //send data requested by flag
					DBG_WHTTPD_V("slot[%d]: sending %d bytes\n", SIdx, whttpd_Slots[SIdx].SendBufBytes);
					lwip_write(whttpd_Slots[SIdx].AcptSck, whttpd_Slots[SIdx].SendBuf, whttpd_Slots[SIdx].SendBufBytes);
#ifdef DO_DEBUG_WHTTPD_SENT_DATA
					printf("whttpd: slot[%d]: ---- sent data ----\n", SIdx);
					whttpd_Slots[SIdx].SendBuf[whttpd_Slots[SIdx].SendBufBytes] = 0; //add null termination (there's always space if #ifdef DO_DEBUG_WHTTPD_SENT_DATA - see how .SendBuf is allocated)
					printf(whttpd_Slots[SIdx].SendBuf);
					printf("\nwhttpd: slot[%d]: -------------------\n", SIdx);
#endif
					whttpd_Slots[SIdx].Flags &= NOT_FLAG8(WHTTPD_SLOTFLAG_DO_SEND); //clear flag WHTTPD_SLOTFLAG_DO_SEND
					whttpd_Slots[SIdx].SendBufBytes = 0;
					//
					if(whttpd_Slots[SIdx].Flags & WHTTPD_SLOTFLAG_RESPONSE_END){ //clean-up for next request, leave connection opened (will be timed out or closed by client later)
						if(whttpd_Slots[SIdx].FEvtItemIdx!=WHTTPD_FEVT_INVALID_ITEM_INDEX) whttpd_fevt_call_cb_resp_finished(whttpd_Slots[SIdx].FEvtItemIdx);
						whttpd_slot_cleanup_after_send_all(SIdx);
					}
				}
				//
				vTaskDelay(WHTTPD_SLOTLOOP_DELAY); //let's make WHTTPD more CPU-load friendly
			}
			//
			vTaskDelay(WHTTPD_MAINLOOP_DELAY); //let's make WHTTPD more CPU-load friendly
			//
			if((UpgradeRebootWhenNoActiveSlot)&&(AllSlotsUnused)){
				DBG_WHTTPD("calling system_upgrade_reboot()\n");
				system_upgrade_reboot(); //used after FOTA upgrade
			}
		}
		//
		break; //the "while(1)" doesn't look so dangerous now, hm? Better than if(){ if(){ if(){ } } } ...
	}
	//if we're here, it's really bad. WHTTPD can't start/work. Let's do the clean-up
	DBG_WHTTPD("cleaning everything and stopping server\n");
	for(SIdx=0;SIdx<WHTTPD_MAX_CONNECTIONS;SIdx++){
		if(!WHTTPD_IS_VALID_SOCKFD(whttpd_Slots[SIdx].AcptSck)) continue; //skip unused slots
		//
		whttpd_rca_free_and_reinit(SIdx); //free buffers allocated by whttpd_rca_analyze(...) (if any)
		lwip_close(whttpd_Slots[SIdx].AcptSck);
		whttpd_slot_free_after_close(SIdx);
	}
	if(Sck!=-1){
		DBG_WHTTPD("closing main socket connection\n");
		lwip_close(Sck);
	}
	if(HdrS!=NULL) free(HdrS);
	vTaskDelete(NULL);
}

void ICACHE_FLASH_ATTR whttpd_upgrade_reboot_when_no_active_slot(void){
/* WHTTPD calls system_upgrade_reboot() when no active slot (serving of all files related to current response
 * finished and all connections timed out and were closed).
 * Used after FOTA upgrade.
 */
	UpgradeRebootWhenNoActiveSlot = 1;
}
