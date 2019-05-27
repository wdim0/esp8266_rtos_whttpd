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
#include <wfof.h>

#include "whttpd_defs.h"
#include "whttpd_preproc.h"
#include "whttpd_preproc_cb.h"

//WHTTPD PREPROCESSOR (processing tags in file currently being sent to client) - MAIN

volatile whttpd_pp_struct whttpd_PP; //we have intentionally only one global preprocessor to assure minimum memory usage (we could give each connection it's own preprocessor, but then we're very vulnerable to out of memory problem when there are more simultaneous connections, each doing heavy preprocessor stuff)

//-------- whttpd_PPItems[] array definition

const whttpd_pp_item_struct whttpd_PPItems[WHTTPD_PP_ITEMS_CNT] = {
		//Here we define all tags that will WHTTPD know and it's callback functions.
		{.Tag = "[[WHTTPD_VERSION]]",			.func_cb = cb_get_version},
		{.Tag = "[[WHTTPD_ERROR_CODE]]",		.func_cb = cb_get_err_code},
		{.Tag = "[[WHTTPD_ERROR_CODE_MSG]]",	.func_cb = cb_get_err_code_msg},
		//
		{.Tag = "[[FLASH_GET_INFO]]",			.func_cb = cb_flash_get_info},
		{.Tag = "[[FLASH_READ_ALL]]",			.func_cb = cb_flash_read_all},
		{.Tag = "[[FOTA_SLOT_IN_USE]]",			.func_cb = cb_fota_slot_in_use},
		{.Tag = "[[FOTA_SLOT_FREE]]",			.func_cb = cb_fota_slot_free},
		{.Tag = "[[FOTA_SLOT_IN_USE_INFO]]",	.func_cb = cb_fota_slot_in_use_info},
		{.Tag = "[[FOTA_PWD_PREFIX]]",			.func_cb = cb_fota_gen_and_get_pwd_prefix},
		{.Tag = "[[FOTA_COMMIT]]",				.func_cb = cb_fota_commit},
		//
		{.Tag = "[[GPIO_SET_GPIO2]]",			.func_cb = cb_gpio_set_gpio2}
		//
		// ... you can add your own items here
		//
		//! if you change the number of items here, don't forget to change WHTTPD_PP_ITEMS_CNT respectively !
};

//-------- helpers for less code in callback functions

void ICACHE_FLASH_ATTR whttpd_preproc_manage_cb_output(char* Dest, char* Src, uint16_t* AlreadyCopiedBytes, uint16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetDone){
	if((Src==NULL)||(Dest==NULL)){
		*RetBytes = 0;
		*RetDone = 1;
		return;
	}
	//
	*RetBytes = strlen(Src)-*AlreadyCopiedBytes;
	if(*RetBytes>MaxBytes) *RetBytes = MaxBytes;
	//
	strncpy(Dest, &(Src[*AlreadyCopiedBytes]), *RetBytes);
	//
	*AlreadyCopiedBytes += *RetBytes;
	*RetDone = (*AlreadyCopiedBytes==strlen(Src));
	if(*RetDone) *AlreadyCopiedBytes = 0; //reset for next start (if the tag is multiple times in the preprocessed input data)
}

uint8_t ICACHE_FLASH_ATTR whttpd_preproc_get_req_param_value_ptr(char* SearchFor, char** PtrValue, uint16_t *Len){
/* Tries to locate string SearchFor in whttpd_PP.SSFPreproc->RequestParams.
 * If not found, fills *PtrValue=NULL, *Len=0 and returns 0.
 * If found, *PtrValue points to the first char of the value (after SearchFor string in whttpd_PP.SSFPreproc->RequestParams),
 * *Len is filled with the length of the value string and returns 1.
 */
	*PtrValue = NULL;
	*Len = 0;
	if((whttpd_PP.SSFPreproc->RqParams==NULL)||(SearchFor==NULL)) return 0;
	//
	*PtrValue = strstr(whttpd_PP.SSFPreproc->RqParams, SearchFor);
	if(*PtrValue==NULL) return 0;
	//
	*PtrValue += strlen(SearchFor); //point to first char beyond SearchFor
	if((*PtrValue)[0]==0) return 1; //but we're pointing to null termination
	//
	uint16_t Offs=0;
	do{
		char Ch = (*PtrValue)[Offs];
		if(Ch=='&') break;
		if(Ch==';') break; //yes, this also separates key=value pairs
	} while((*PtrValue)[++Offs]!=0); //exit loop when we are on separator char (see breaks inside the loop) OR we're at the end of string (next iteration would point to null termination)
	//
	*Len = Offs;
	return 1;
}

//-------- main

void ICACHE_FLASH_ATTR whttpd_preproc_first_init(){
//Call this only once at the start of the WHTTPD task.
	memset((uint8_t*)&whttpd_PP, 0, sizeof(whttpd_pp_struct)); //fills everything by 0 (all flags are cleared)
	whttpd_PP.RespFIdx = WFOF_INVALID_INDEX;
}

void ICACHE_FLASH_ATTR whttpd_preproc_init(){
//Call this whenever you need to reset preprocessor whatever it's doing and make it free for use.
	if(whttpd_PP.LineBuf!=NULL) free(whttpd_PP.LineBuf);
	whttpd_preproc_first_init(); //fills everything by 0 (all flags are cleared)
}

int8_t ICACHE_FLASH_ATTR whttpd_preproc_set(uint8_t RespFIdx, uint32_t RespFSize, whttpd_slot_state_for_preproc_struct* SSFPreproc){
/* Call this to check if preprocessor is free for use and to set preprocessor for the specific file.
 * Returns WHTTPD_PP_SET_RETCODE_*
 */
	if(whttpd_PP.Flags & WHTTPD_PPFLAG_IN_USE) return WHTTPD_PP_SET_RETCODE_PP_IN_USE;
	//
	DBG_WHTTPD_PP("started\n");
	//
	whttpd_preproc_init(); //fills everything by 0 (all flags are cleared)
	//
	whttpd_PP.RespFIdx = RespFIdx;
	whttpd_PP.RespFSize = RespFSize;
	whttpd_PP.SSFPreproc = SSFPreproc;
	whttpd_PP.PPItemIdx = WHTTPD_PP_INVALID_ITEM_INDEX;
	whttpd_PP.Flags |= WHTTPD_PPFLAG_IN_USE;
	//
	return WHTTPD_PP_SET_RETCODE_OK;
}

volatile whttpd_pp_struct* ICACHE_FLASH_ATTR whttpd_preproc_get_PP_ptr(void){
	return &whttpd_PP;
}

void ICACHE_FLASH_ATTR whttpd_preproc_find_next_tag(){ //called very often
/* Analyzes .LineBuf starting at offset .LineReadStartPos (including).
 * and fills: .LineReadStartPos, .TagStartPos, .LineReadEndPos, .PPItemIdx (see comments in whttpd_pp_struct definition).
 */
	if(whttpd_PP.LineReadStartPos<whttpd_PP.LineBufSize-1){ //.LineReadStartPos is not pointing beyond end of .LineBuf string (-1 to discount null termination - get the same as strlen(.LineBuf))
		//
		//find tag / set .LineReadStartPos, .TagStartPos, .LineReadEndPos, .PPItemIdx
		char* ClosestTagPtr = NULL;
		uint8_t ClosestPPItemIdx = WHTTPD_PP_INVALID_ITEM_INDEX;
		//
		uint8_t PPItemIdx;
		for(PPItemIdx=0;PPItemIdx<WHTTPD_PP_ITEMS_CNT;PPItemIdx++){ //go through all tags
			char* CurrStartPtr = strstr(&whttpd_PP.LineBuf[whttpd_PP.LineReadStartPos], whttpd_PPItems[PPItemIdx].Tag);
			if((CurrStartPtr!=NULL)&&((ClosestTagPtr==NULL)||(CurrStartPtr<ClosestTagPtr))){ //we've located tag whttpd_PPItems[PPItemIdx].Tag AND (it's the first tag we've encountered OR it's before the one we already have)
				ClosestTagPtr = CurrStartPtr;
				ClosestPPItemIdx = PPItemIdx;
			}
		}
		//
		if(ClosestTagPtr!=NULL){ //tag(s) are in .LineBuf, ClosestTagPtr is pointing to the closest tag (to char '[') and whttpd_PPItems[ClosestPPItemIdx] specifies the tag
			whttpd_PP.TagStartPos = ClosestTagPtr - &whttpd_PP.LineBuf[whttpd_PP.LineReadStartPos] + whttpd_PP.LineReadStartPos;
			whttpd_PP.LineReadEndPos = whttpd_PP.TagStartPos + strlen(whttpd_PPItems[ClosestPPItemIdx].Tag) - 1;
		}
		else{ //no tag in .LineBuf and ClosestPPItemIdx == WHTTPD_PP_INVALID_ITEM_INDEX
			whttpd_PP.LineReadEndPos = (whttpd_PP.LineBufSize-1)-1; //point to last char (char before null termination)
			whttpd_PP.TagStartPos = whttpd_PP.LineReadEndPos;
		}
		whttpd_PP.PPItemIdx = ClosestPPItemIdx;
	}
}

void ICACHE_FLASH_ATTR whttpd_preproc_get_data(uint32_t* RespFOffs, uint8_t* RetBuf, int16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetHaveAll){
/* This is the main data interface between preprocessor and WHTTPD output data buffer (passed as RetBuf here).
 * This function is called again and again (with different MaxBytes) until the whole WHTTPD output data buffer is filled
 * or until you set RetHaveAll to non-zero value.
 * This function sets RetBytes to indicate how many bytes was really copied into RetBuf.
 * The RespFOffs is also altered - this is the offset where to read preprocessed file (specified by whttpd_PP.RespF*).
 * Call whttpd_preproc_set(...) first to be able to use this function.
 */
	*RetBytes = 0;
	*RetHaveAll = 0;
	if(MaxBytes<=0) return;
	//
	if((!(whttpd_PP.Flags & WHTTPD_PPFLAG_HAVE_LINE))&&(!(whttpd_PP.Flags & WHTTPD_PPFLAG_FAILSAFE_MODE))){
		//
		//get line into .LineBuf (including "\r\n" at the end) OR get all remaining bytes if no newline char found in the rest of input file data
		//(.LineBuf is always null terminated (you can use it as string), LineBufSize is filled by string size + null termination)
		uint32_t GetBytes;
		if(wfof_find_char_pos(whttpd_PP.RespFIdx, *RespFOffs, '\n', &GetBytes) == 0){ //next newline char found
			GetBytes++; //to have proper space for data
		}
		else{ //next newline char not found
			GetBytes = whttpd_PP.RespFSize-*RespFOffs; //get all remaining bytes
		}
		if(GetBytes>0){
			if(whttpd_PP.LineBuf!=NULL){
				free(whttpd_PP.LineBuf);
				whttpd_PP.LineBuf = NULL;
				whttpd_PP.LineBufSize = 0;
			}
			//
			if(GetBytes+1<=WHTTPD_PP_LINE_BUF_MAX_SIZE){
				whttpd_PP.LineBuf = malloc(GetBytes+1); //+1 to have space for null termination
				if(whttpd_PP.LineBuf!=NULL){ //malloc ok
					whttpd_PP.LineBufSize = GetBytes+1;
					//read input file line into .LineBuf, add null termination
					uint32_t BytesRead = wfof_get_file_data(whttpd_PP.RespFIdx, whttpd_PP.LineBuf, *RespFOffs, GetBytes);
					*RespFOffs += BytesRead; //BytesRead will be always == GetBytes (because of how the GetBytes is computed using wfof_find_char_pos(...))
					whttpd_PP.LineBuf[BytesRead] = 0; //add null termination
					//
					whttpd_PP.LineReadStartPos = 0;
					whttpd_PP.TagStartPos = 0;
					whttpd_PP.LineReadEndPos = 0;
					whttpd_PP.PPItemIdx = WHTTPD_PP_INVALID_ITEM_INDEX;
					//
					whttpd_PP.Flags |= WHTTPD_PPFLAG_HAVE_LINE; //set flag HAVE_LINE
					if(*RespFOffs>=whttpd_PP.RespFSize) whttpd_PP.Flags |= WHTTPD_PPFLAG_END_OF_FILE; //set flag END_OF_FILE
					else whttpd_PP.Flags &= NOT_FLAG16(WHTTPD_PPFLAG_END_OF_FILE); //clear flag END_OF_FILE
					whttpd_PP.Flags &= NOT_FLAG16(WHTTPD_PPFLAG_GETTING_BEF_TAG_DATA); //clear flag GETTING_BEF_TAG_DATA
					whttpd_PP.Flags &= NOT_FLAG16(WHTTPD_PPFLAG_GETTING_TAG_DATA); //clear flag GETTING_TAG_DATA
					//
					DBG_WHTTPD_PP("got line '%s', .LineBufSize=%d \n", whttpd_PP.LineBuf, whttpd_PP.LineBufSize);
				}
				else{ //malloc failed - this is bad. The line in input file is so long that we can't allocate memory for it => switch preprocessor to failsafe mode and output data without preprocessing
					DBG_WHTTPD_PP("can't allocate memory for line in input file (too long), turning preprocessor off\n");
					whttpd_PP.Flags &= NOT_FLAG16(WHTTPD_PPFLAG_HAVE_LINE); //clear flag HAVE_LINE
					whttpd_PP.Flags |= WHTTPD_PPFLAG_FAILSAFE_MODE; //set flag FAILSAFE_MODE
				}
			}
			else{
				DBG_WHTTPD_PP("line in input file too long (can have max %d chars including CR LF), turning preprocessor off\n", WHTTPD_PP_LINE_BUF_MAX_SIZE);
				whttpd_PP.Flags &= NOT_FLAG16(WHTTPD_PPFLAG_HAVE_LINE); //clear flag HAVE_LINE
				whttpd_PP.Flags |= WHTTPD_PPFLAG_FAILSAFE_MODE; //set flag FAILSAFE_MODE
			}
		}
		else{
			*RetHaveAll = 1; //ok, that's all, there's no more data to be passed through preprocessor => set *RetHaveAll (this will cause entering the clean-up section at the end)
		}
		//
	}
	//
	if((whttpd_PP.Flags & WHTTPD_PPFLAG_HAVE_LINE)&&(!(whttpd_PP.Flags & WHTTPD_PPFLAG_FAILSAFE_MODE))){

		if((!(whttpd_PP.Flags & WHTTPD_PPFLAG_GETTING_BEF_TAG_DATA))&&(!(whttpd_PP.Flags & WHTTPD_PPFLAG_GETTING_TAG_DATA))){
			//
			whttpd_preproc_find_next_tag(); //analyzes .LineBuf starting at offset .LineReadStartPos (including) and fills: .LineReadStartPos, .TagStartPos, .LineReadEndPos, .PPItemIdx
			DBG_WHTTPD_PP(".LineReadStartPos=%d, .TagStartPos=%d, .LineReadEndPos=%d, .PPItemIdx=%d\n", whttpd_PP.LineReadStartPos, whttpd_PP.TagStartPos, whttpd_PP.LineReadEndPos, whttpd_PP.PPItemIdx);
			//
			whttpd_PP.Flags |= WHTTPD_PPFLAG_TAG_FUNC_FIRST_CALL; //set flag TAG_FUNC_FIRST_CALL
			whttpd_PP.TagFuncCallsCtr = 0;
			//
			if(whttpd_PP.LineReadStartPos<whttpd_PP.LineBufSize-1) whttpd_PP.Flags |= WHTTPD_PPFLAG_HAVE_DATA_BEF_TAG; //we have some data to copy (.LineReadStartPos is not pointing beyond end of .LineBuf string (-1 to discount null termination - get the same as strlen(.LineBuf)))
			else whttpd_PP.Flags &= NOT_FLAG16(WHTTPD_PPFLAG_HAVE_DATA_BEF_TAG); //clear flag HAVE_DATA_BEF_TAG
			//
			if(whttpd_PP.TagStartPos!=whttpd_PP.LineReadEndPos) whttpd_PP.Flags |= WHTTPD_PPFLAG_HAVE_TAG; //we have tag (see .TagStartPos definition)
			else whttpd_PP.Flags &= NOT_FLAG16(WHTTPD_PPFLAG_HAVE_TAG); //clear flag HAVE_TAG
			//
			if(whttpd_PP.LineReadEndPos>=(whttpd_PP.LineBufSize-1)-1) whttpd_PP.Flags |= WHTTPD_PPFLAG_END_OF_LINE; //we've just analyzed entire .LineBuf string (.LineReadEndPos is pointing to last char of .LineBuf string (-1 to discount null termination - get the same as strlen(.LineBuf) and another -1 to point to last char))
			else whttpd_PP.Flags &= NOT_FLAG16(WHTTPD_PPFLAG_END_OF_LINE); //clear flag END_OF_LINE
			//
			if(whttpd_PP.Flags & WHTTPD_PPFLAG_HAVE_DATA_BEF_TAG) whttpd_PP.Flags |= WHTTPD_PPFLAG_GETTING_BEF_TAG_DATA; //set flag GETTING_BEF_TAG_DATA
			else if(whttpd_PP.Flags & WHTTPD_PPFLAG_HAVE_TAG) whttpd_PP.Flags |= WHTTPD_PPFLAG_GETTING_TAG_DATA; //set flag GETTING_TAG_DATA
		}
		if((whttpd_PP.Flags & WHTTPD_PPFLAG_GETTING_BEF_TAG_DATA)&&(MaxBytes>0)&&(!(whttpd_PP.Flags & WHTTPD_PPFLAG_FAILSAFE_MODE))){
			//
			//CurrRetBytes = get max up to MaxBytes data from .LineBuf[LineReadStartPos .. RealLineReadEndPos] -> RetBuf
			uint16_t RealLineReadEndPos = whttpd_PP.LineReadEndPos;
			if(whttpd_PP.TagStartPos<RealLineReadEndPos) RealLineReadEndPos = whttpd_PP.TagStartPos-1; //if we have tag, point to last char before first tag char ('[')
			uint16_t CurrRetBytes = (RealLineReadEndPos-whttpd_PP.LineReadStartPos)+1;
			uint8_t CurrHaveAll = 1;
			if(CurrRetBytes>MaxBytes){
				CurrRetBytes = MaxBytes;
				CurrHaveAll = 0;
			}
			DBG_WHTTPD_PP("copying %d bytes starting from .LineBuf[%d] to RetBuf\n", CurrRetBytes, whttpd_PP.LineReadStartPos);
			strncpy(RetBuf, &whttpd_PP.LineBuf[whttpd_PP.LineReadStartPos], CurrRetBytes);
			RetBuf += CurrRetBytes; //in case that tag callback function will be called and write to data, offset pointer where to start to write
			//
			whttpd_PP.LineReadStartPos += CurrRetBytes; //point to next data (for possible next calling of whttpd_preproc_get_data(...))
			*RetBytes += CurrRetBytes;
			MaxBytes -= CurrRetBytes;
			//
			if(CurrHaveAll){ //we've copied all data before tag (or all line data if no tag in the line)
				whttpd_PP.Flags &= NOT_FLAG16(WHTTPD_PPFLAG_GETTING_BEF_TAG_DATA); //clear flag GETTING_BEF_TAG_DATA
				if(whttpd_PP.Flags & WHTTPD_PPFLAG_HAVE_TAG) whttpd_PP.Flags |= WHTTPD_PPFLAG_GETTING_TAG_DATA; //set flag GETTING_TAG_DATA
				else if(whttpd_PP.Flags & WHTTPD_PPFLAG_END_OF_LINE) whttpd_PP.Flags &= NOT_FLAG16(WHTTPD_PPFLAG_HAVE_LINE); //clear flag HAVE_LINE
			}
		}
		if((whttpd_PP.Flags & WHTTPD_PPFLAG_GETTING_TAG_DATA)&&(MaxBytes>0)&&(whttpd_PP.PPItemIdx!=WHTTPD_PP_INVALID_ITEM_INDEX)&&(!(whttpd_PP.Flags & WHTTPD_PPFLAG_FAILSAFE_MODE))){
			//
			DBG_WHTTPD_PP(">> calling tag callback function for '%s', MaxBytes=%d\n", whttpd_PPItems[whttpd_PP.PPItemIdx].Tag, MaxBytes);
			//CurrRetBytes = call tag callback function and get max up to MaxBytes data -> RetBuf
			uint16_t CurrRetBytes;
			uint8_t CurrRetDone;
			int8_t CBRetCode = whttpd_PPItems[whttpd_PP.PPItemIdx].func_cb(((whttpd_PP.Flags & WHTTPD_PPFLAG_TAG_FUNC_FIRST_CALL)!=0), RetBuf, MaxBytes, &CurrRetBytes, &CurrRetDone);
			//RetBuf += CurrRetBytes; //no need to do this (there will be no more writing to RetBuf in this function now)
			DBG_WHTTPD_PP("<< tag callback function filled %d bytes into RetBuf, CurrRetDone=%d, CBRetCode=%d\n", CurrRetBytes, CurrRetDone, CBRetCode);
			//
			*RetBytes += CurrRetBytes;
			MaxBytes -= CurrRetBytes;
			//
			whttpd_PP.Flags &= NOT_FLAG16(WHTTPD_PPFLAG_TAG_FUNC_FIRST_CALL); //clear flag TAG_FUNC_FIRST_CALL
			whttpd_PP.TagFuncCallsCtr++;
			//
			if((CurrRetDone)||(CBRetCode<0)||(whttpd_PP.TagFuncCallsCtr>=WHTTPD_PP_TAG_CB_CALLS_CNT_LIMIT)){
				whttpd_PP.Flags &= NOT_FLAG16(WHTTPD_PPFLAG_GETTING_TAG_DATA); //clear flag GETTING_TAG_DATA
				whttpd_PP.LineReadStartPos = whttpd_PP.LineReadEndPos+1; //point to next char after last char of our tag (']')
				if(whttpd_PP.Flags & WHTTPD_PPFLAG_END_OF_LINE) whttpd_PP.Flags &= NOT_FLAG16(WHTTPD_PPFLAG_HAVE_LINE); //clear flag HAVE_LINE
			}
		}
		if((!(whttpd_PP.Flags & WHTTPD_PPFLAG_HAVE_LINE))&&(whttpd_PP.Flags & WHTTPD_PPFLAG_END_OF_FILE)&&(!(whttpd_PP.Flags & WHTTPD_PPFLAG_FAILSAFE_MODE))) *RetHaveAll = 1; //ok, that's all, there's no more data to be passed through preprocessor => set *RetHaveAll (this will cause entering the clean-up section at the end)

	}
	//
	if(whttpd_PP.Flags & WHTTPD_PPFLAG_FAILSAFE_MODE){
		*RetBytes = wfof_get_file_data(whttpd_PP.RespFIdx, RetBuf, *RespFOffs, MaxBytes);
		*RespFOffs += *RetBytes;
		*RetHaveAll = (*RespFOffs>=whttpd_PP.RespFSize);
	}
	//
	//clean-up
	if(*RetHaveAll){
		whttpd_preproc_init();
		//
		DBG_WHTTPD_PP("finished\n");
	}
}
