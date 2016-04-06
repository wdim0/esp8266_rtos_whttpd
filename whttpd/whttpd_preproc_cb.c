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
#include <espressif/upgrade.h>
#include <driver/gpio.h>

#include "whttpd_defs.h"
#include "whttpd_preproc.h"
#include "whttpd_preproc_cb.h"

//WHTTPD PREPROCESSOR (processing tags in file currently being sent to client) - TAG CALLBACK FUNCTIONS
//! when you change code here, don't forget to edit whttpd_PPItems[] array definition in whttpd_preproc.c respectively

//-------- WHTTPD basic info functions

uint16_t cb_gv_CopiedBytes = 0;
int8_t ICACHE_FLASH_ATTR cb_get_version(uint8_t IsFirstCall, char* OutputData, uint16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetDone){
/* TAG CALLBACK FUNCTION for [[WHTTPD_VERSION]]
 * --------------------------------------------
 * ! See whttpd_pp_item_struct structure definition to know what is passed to / what is expected from tag callback function.
 * ! This function could be called multiple times for one tag, if we signal by RetDone that we didn't end yet (probably because MaxBytes was less that what we needed).
 */
	if(IsFirstCall) cb_gv_CopiedBytes = 0;
	whttpd_preproc_manage_cb_output(OutputData, WHTTPD_VER, &cb_gv_CopiedBytes, MaxBytes, RetBytes, RetDone); //output into OutputData (if MaxBytes is less than size of our data, manage subsequent chunked output throughout more calls of this tag callback function using global variable *_CopiedBytes)
	return 0; //no error (don't abort tag preprocessing)
}

uint16_t cb_gec_CopiedBytes = 0;
int8_t ICACHE_FLASH_ATTR cb_get_err_code(uint8_t IsFirstCall, char* OutputData, uint16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetDone){
/* TAG CALLBACK FUNCTION for [[WHTTPD_ERROR_CODE]]
 * --------------------------------------------
 * ! See whttpd_pp_item_struct structure definition to know what is passed to / what is expected from tag callback function.
 * ! This function could be called multiple times for one tag, if we signal by RetDone that we didn't end yet (probably because MaxBytes was less that what we needed).
 */
	//allocate memory for OutString (local)
	char* OutString = malloc(6);
	if(OutString==NULL){
		*RetBytes = 0;
		*RetDone = 1;
		return -1;
	}
	if(IsFirstCall) cb_gec_CopiedBytes = 0;
	//
	//>> ---- generate whole OutString (enter your code here)
	whttpd_pp_struct* PPPtr = (whttpd_pp_struct*)whttpd_preproc_get_PP_ptr();
	sprintf(OutString, "%d", PPPtr->SSFPreproc->HTTPRespCode);
	//<< ----
	//
	whttpd_preproc_manage_cb_output(OutputData, OutString, &cb_gec_CopiedBytes, MaxBytes, RetBytes, RetDone); //output into OutputData (if MaxBytes is less than size of our data, manage subsequent chunked output throughout more calls of this tag callback function using global variable *_CopiedBytes)
	free(OutString); //free allocated memory
	return 0; //no error (don't abort tag preprocessing)
}

uint16_t cb_gecm_CopiedBytes = 0;
int8_t ICACHE_FLASH_ATTR cb_get_err_code_msg(uint8_t IsFirstCall, char* OutputData, uint16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetDone){
/* TAG CALLBACK FUNCTION for [[WHTTPD_ERROR_CODE_MSG]]
 * --------------------------------------------
 * ! See whttpd_pp_item_struct structure definition to know what is passed to / what is expected from tag callback function.
 * ! This function could be called multiple times for one tag, if we signal by RetDone that we didn't end yet (probably because MaxBytes was less that what we needed).
 */
	//allocate memory for OutString (local)
	char* OutString = malloc(30); //!make sure that all your error code strings will fit
	if(OutString==NULL){
		*RetBytes = 0;
		*RetDone = 1;
		return -1;
	}
	if(IsFirstCall) cb_gecm_CopiedBytes = 0;
	//
	//>> ---- generate whole OutString (enter your code here)
	whttpd_pp_struct* PPPtr = (whttpd_pp_struct*)whttpd_preproc_get_PP_ptr();
	switch(PPPtr->SSFPreproc->HTTPRespCode){
		case 200: strcpy(OutString, WHTTPD_ERR_MSG_200); break;
		case 400: strcpy(OutString, WHTTPD_ERR_MSG_400); break;
		case 404: strcpy(OutString, WHTTPD_ERR_MSG_404); break;
		case 500: strcpy(OutString, WHTTPD_ERR_MSG_500); break;
		default: strcpy(OutString, "Unknown error code");
	}
	//<< ----
	//
	whttpd_preproc_manage_cb_output(OutputData, OutString, &cb_gecm_CopiedBytes, MaxBytes, RetBytes, RetDone); //output into OutputData (if MaxBytes is less than size of our data, manage subsequent chunked output throughout more calls of this tag callback function using global variable *_CopiedBytes)
	free(OutString); //free allocated memory
	return 0; //no error (don't abort tag preprocessing)
}

//-------- flash management and FOTA upgrade functions

uint16_t cb_fgi_CopiedBytes = 0;
int8_t ICACHE_FLASH_ATTR cb_flash_get_info(uint8_t IsFirstCall, char* OutputData, uint16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetDone){
/* TAG CALLBACK FUNCTION for [[FLASH_GET_INFO]]
 * --------------------------------------------
 * ! See whttpd_pp_item_struct structure definition to know what is passed to / what is expected from tag callback function.
 * ! This function could be called multiple times for one tag, if we signal by RetDone that we didn't end yet (probably because MaxBytes was less that what we needed).
 */
	//allocate memory for OutString (local)
	char* OutString = malloc(35);
	if(OutString==NULL){
		*RetBytes = 0;
		*RetDone = 1;
		return -1;
	}
	if(IsFirstCall) cb_fgi_CopiedBytes = 0;
	//
	//>> ---- generate whole OutString (enter your code here)
	uint32_t ID = spi_flash_get_id();
	sprintf(OutString, "Size: %d kB, ID: 0x%08X", (((uint32_t)1)<<((uint8_t*)&ID)[2])/1024, ID);
	//<< ----
	//
	whttpd_preproc_manage_cb_output(OutputData, OutString, &cb_fgi_CopiedBytes, MaxBytes, RetBytes, RetDone); //output into OutputData (if MaxBytes is less than size of our data, manage subsequent chunked output throughout more calls of this tag callback function using global variable *_CopiedBytes)
	free(OutString); //free allocated memory
	return 0; //no error (don't abort tag preprocessing)
}

uint32_t cb_fra_CopiedBytes = 0;
int8_t ICACHE_FLASH_ATTR cb_flash_read_all(uint8_t IsFirstCall, char* OutputData, uint16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetDone){
/* TAG CALLBACK FUNCTION for [[FLASH_READ_ALL]]
 * --------------------------------------------
 * ! See whttpd_pp_item_struct structure definition to know what is passed to / what is expected from tag callback function.
 * ! This function could be called multiple times for one tag, if we signal by RetDone that we didn't end yet (probably because MaxBytes was less that what we needed).
 */
	*RetBytes = 0;
	*RetDone = 0;
	if(IsFirstCall) cb_fra_CopiedBytes = 0;
	//
	uint32_t ID = spi_flash_get_id();
	uint32_t FlashSizeB = ((uint32_t)1)<<((uint8_t*)&ID)[2]; //flash size in bytes
	MaxBytes &= 0xFFFC; //mask out 2 last bits - the same like MaxBytes = (MaxBytes/4)*4
	if(MaxBytes==0) return 0; //we are not able to read at least 4 bytes - not enough space in OutputData => skip this call, report no error (don't abort tag preprocessing - it will be called again with more MaxBytes)
	//
	//this code using spi_flash_read(...) crashes - why? reads are 4-bytes aligned
	//if(spi_flash_read(cb_get_flash_download_CopiedBytes, (uint32_t*)OutputData, MaxBytes)!=SPI_FLASH_RESULT_OK) return -1; //error (abort tag preprocessing)
	//cb_get_flash_download_CopiedBytes += MaxBytes;
	//
	//read flash exactly like in wfof.c (after many experiments, this is the code that doesn't crash)
	uint32_t U32Tmp;
	uint16_t Cnt = 0;
	while((Cnt<MaxBytes)&&(cb_fra_CopiedBytes<FlashSizeB)){
		U32Tmp = *((uint32_t*)(cb_fra_CopiedBytes+WHTTPD_SPIFLASH_OFFS));
		memcpy(&OutputData[Cnt], &U32Tmp, 4);
		cb_fra_CopiedBytes += 4;
		Cnt += 4;
	}
	*RetBytes += Cnt;
	//
	if(cb_fra_CopiedBytes>=FlashSizeB) *RetDone = 1;
	if(*RetDone) cb_fra_CopiedBytes = 0; //reset for next start (if the tag is multiple times in the preprocessed input data)
	return 0; //no error (don't abort tag preprocessing)
}

int8_t ICACHE_FLASH_ATTR cb_fota_slot_in_use(uint8_t IsFirstCall, char* OutputData, uint16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetDone){
/* TAG CALLBACK FUNCTION for [[FOTA_SLOT_IN_USE]]
 * --------------------------------------------
 * ! See whttpd_pp_item_struct structure definition to know what is passed to / what is expected from tag callback function.
 * ! This function could be called multiple times for one tag, if we signal by RetDone that we didn't end yet (probably because MaxBytes was less that what we needed).
 */
	*RetBytes = 0;
	*RetDone = 0;
	if(MaxBytes<1) return 0; //we are not able pass at least 1 byte - not enough space in OutputData => skip this call, report no error (don't abort tag preprocessing - it will be called again with more MaxBytes)
	OutputData[0] = (system_upgrade_userbin_check()==UPGRADE_FW_BIN1)?'1':'2';
	*RetBytes = 1;
	*RetDone = 1;
	return 0; //no error (don't abort tag preprocessing)
}

int8_t ICACHE_FLASH_ATTR cb_fota_slot_free(uint8_t IsFirstCall, char* OutputData, uint16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetDone){
/* TAG CALLBACK FUNCTION for [[FOTA_SLOT_FREE]]
 * --------------------------------------------
 * ! See whttpd_pp_item_struct structure definition to know what is passed to / what is expected from tag callback function.
 * ! This function could be called multiple times for one tag, if we signal by RetDone that we didn't end yet (probably because MaxBytes was less that what we needed).
 */
	*RetBytes = 0;
	*RetDone = 0;
	if(MaxBytes<1) return 0; //we are not able pass at least 1 byte - not enough space in OutputData => skip this call, report no error (don't abort tag preprocessing - it will be called again with more MaxBytes)
	OutputData[0] = (system_upgrade_userbin_check()==UPGRADE_FW_BIN1)?'2':'1';
	*RetBytes = 1;
	*RetDone = 1;
	return 0; //no error (don't abort tag preprocessing)
}

uint16_t cb_fsiui_CopiedBytes = 0;
int8_t ICACHE_FLASH_ATTR cb_fota_slot_in_use_info(uint8_t IsFirstCall, char* OutputData, uint16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetDone){
/* TAG CALLBACK FUNCTION for [[FOTA_SLOT_IN_USE_INFO]]
 * --------------------------------------------
 * ! See whttpd_pp_item_struct structure definition to know what is passed to / what is expected from tag callback function.
 * ! This function could be called multiple times for one tag, if we signal by RetDone that we didn't end yet (probably because MaxBytes was less that what we needed).
 */
	//allocate memory for OutString (local)
	char* OutString = malloc(30);
	if(OutString==NULL){
		*RetBytes = 0;
		*RetDone = 1;
		return -1;
	}
	if(IsFirstCall) cb_fsiui_CopiedBytes = 0;
	//
	//>> ---- generate whole OutString (enter your code here)
	uint32_t StartAddr = system_get_userbin_addr(); //slot in use
	uint32_t ID = spi_flash_get_id();
	uint32_t FlashSizeB = ((uint32_t)1)<<((uint8_t*)&ID)[2]; //flash size in bytes
	uint32_t Len = (FlashSizeB/2) - ((StartAddr>(FlashSizeB/2))?StartAddr-(FlashSizeB/2):StartAddr) - 0x04000; //(FlashSizeB/2) - start offset of usercode - 16 kB for system parameter area
	sprintf(OutString, "0x%05X - 0x%05X, %d kB", StartAddr, StartAddr+Len-1, Len/1024);
	//<< ----
	//
	whttpd_preproc_manage_cb_output(OutputData, OutString, &cb_fsiui_CopiedBytes, MaxBytes, RetBytes, RetDone); //output into OutputData (if MaxBytes is less than size of our data, manage subsequent chunked output throughout more calls of this tag callback function using global variable *_CopiedBytes)
	free(OutString); //free allocated memory
	return 0; //no error (don't abort tag preprocessing)
}

char cb_fgagpp_Prefix[WHTTPD_PP_FOTA_PWD_PREFIX_LEN+1]; //WHTTPD_PP_FOTA_PWD_PREFIX_LEN + null termination

char* ICACHE_FLASH_ATTR whttpd_preproc_fota_get_pwd_prefix(void){
//! Can be called after cb_fota_generate_and_get_pwd_prefix(1, ...) has been already called.
	return cb_fgagpp_Prefix;
}

uint16_t cb_fgagpp_CopiedBytes = 0;
int8_t ICACHE_FLASH_ATTR cb_fota_gen_and_get_pwd_prefix(uint8_t IsFirstCall, char* OutputData, uint16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetDone){
/* TAG CALLBACK FUNCTION for [[FOTA_PWD_PREFIX]]
 * --------------------------------------------
 * ! See whttpd_pp_item_struct structure definition to know what is passed to / what is expected from tag callback function.
 * ! This function could be called multiple times for one tag, if we signal by RetDone that we didn't end yet (probably because MaxBytes was less that what we needed).
 */
	if(IsFirstCall){
		cb_fgagpp_CopiedBytes = 0;
		//
		//generate string cb_fgagpp_Prefix
		uint8_t Idx;
		for(Idx=0;Idx<WHTTPD_PP_FOTA_PWD_PREFIX_LEN;Idx++) cb_fgagpp_Prefix[Idx] = 'a'+RAND_0_TO_X('z'-'a');
		cb_fgagpp_Prefix[WHTTPD_PP_FOTA_PWD_PREFIX_LEN] = 0;
	}
	whttpd_preproc_manage_cb_output(OutputData, cb_fgagpp_Prefix, &cb_fgagpp_CopiedBytes, MaxBytes, RetBytes, RetDone); //output into OutputData (if MaxBytes is less than size of our data, manage subsequent chunked output throughout more calls of this tag callback function using global variable *_CopiedBytes)
	return 0; //no error (don't abort tag preprocessing)
}

uint16_t cb_fc_CopiedBytes = 0;
char* cb_fc_FotaCommitLog = NULL;
int8_t ICACHE_FLASH_ATTR cb_fota_commit(uint8_t IsFirstCall, char* OutputData, uint16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetDone){
/* TAG CALLBACK FUNCTION for [[FOTA_COMMIT]]
 * --------------------------------------------
 * ! See whttpd_pp_item_struct structure definition to know what is passed to / what is expected from tag callback function.
 * ! This function could be called multiple times for one tag, if we signal by RetDone that we didn't end yet (probably because MaxBytes was less that what we needed).
 */
	if(IsFirstCall){
		cb_fc_CopiedBytes = 0;
		cb_fc_FotaCommitLog = *(char**)fota_commit(); //this is defined in whttpd_post_cb.c (all FOTA stuff at one place)
	}
	whttpd_preproc_manage_cb_output(OutputData, cb_fc_FotaCommitLog, &cb_fc_CopiedBytes, MaxBytes, RetBytes, RetDone); //output into OutputData (if MaxBytes is less than size of our data, manage subsequent chunked output throughout more calls of this tag callback function using global variable *_CopiedBytes)
	return 0; //no error (don't abort tag preprocessing)
}

//-------- GPIO functions

uint8_t cb_gsg2_State = 1;

uint16_t cb_gsg2_CopiedBytes = 0;
int8_t ICACHE_FLASH_ATTR cb_gpio_set_gpio2(uint8_t IsFirstCall, char* OutputData, uint16_t MaxBytes, uint16_t* RetBytes, uint8_t* RetDone){
/* TAG CALLBACK FUNCTION for [[GPIO_SET_GPIO2]]
 * --------------------------------------------
 * ! See whttpd_pp_item_struct structure definition to know what is passed to / what is expected from tag callback function.
 * ! This function could be called multiple times for one tag, if we signal by RetDone that we didn't end yet (probably because MaxBytes was less that what we needed).
 */
	//allocate memory for OutString (local)
	char* OutString = malloc(5); //!make sure that all your error code strings will fit
	if(OutString==NULL){
		*RetBytes = 0;
		*RetDone = 1;
		return -1;
	}
	if(IsFirstCall) cb_gsg2_CopiedBytes = 0;
	//
	//>> ---- generate whole OutString (enter your code here)
	char* Ptr;
	uint16_t Len;
	if((IsFirstCall)&&(whttpd_preproc_get_req_param_value_ptr("gpio2=", &Ptr, &Len))){ //parameter found in parameters passed in Request-URI
		cb_gsg2_State = (Ptr[0]!='0');
		PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
		GPIO_OUTPUT_SET(2, cb_gsg2_State);
	}
	sprintf(OutString, "%d", cb_gsg2_State);
	//<< ----
	//
	whttpd_preproc_manage_cb_output(OutputData, OutString, &cb_gsg2_CopiedBytes, MaxBytes, RetBytes, RetDone); //output into OutputData (if MaxBytes is less than size of our data, manage subsequent chunked output throughout more calls of this tag callback function using global variable *_CopiedBytes)
	free(OutString); //free allocated memory
	return 0; //no error (don't abort tag preprocessing)
}
