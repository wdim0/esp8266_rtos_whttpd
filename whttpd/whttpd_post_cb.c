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
#include <espressif/upgrade.h>
#include <ssl/ssl_crypto.h> //to have access to MD5

#include "whttpd_defs.h"
#include "whttpd_post.h"
#include "whttpd_post_cb.h"

//WHTTPD POST DATA HANDLING (processing POST data currently being received from client) - POST DATA CALLBACK FUNCTIONS
//! when you change code here, don't forget to edit whttpd_PostItems[] array definition in whttpd_post.c respectively

//-------- FOTA upgrade functions

void ICACHE_FLASH_ATTR fota_log_add(whttpd_post_fota_log_type_enum LogType, char* Line);

char FotaLogLine[WHTTPD_POST_FOTA_LOG_MAX_LINE_LEN];

char* FotaLog = NULL;
uint16_t FotaLogLen = 0;
uint16_t FotaLogPos = 0;
uint8_t FotaLogDisable = 0;

//----

typedef enum {
	CB_FFA_RES_INITED = 0, CB_FFA_RES_OK, CB_FFA_RES_TOO_BIG, CB_FFA_RES_WRITE_ERR, CB_FFA_RES_WRITE_TIMEOUT
} cb_ffa_res_enum;

uint8_t* cb_ffa_SectorImg = NULL;
uint32_t cb_ffa_CopiedBytes = 0;
uint8_t cb_ffa_Result = CB_FFA_RES_INITED; //one of cb_ffa_res_enum values
uint32_t cb_ffa_FreeUserbinAddr = 0;
uint32_t cb_ffa_MaxLen = 0;
MD5_CTX cb_ffa_MD5Ctx;
uint8_t cb_ffa_MD5Digest[MD5_SIZE];

void ICACHE_FLASH_ATTR cb_fota_flash_data_wr(void){
/* Write sector to flash - unification of flash erase sector + write for cb_fota_flash_data(...)
 * ! Uses global vars:
 * - cb_ffa_SectorImg
 * - cb_ffa_CopiedBytes
 * - cb_ffa_FreeUserbinAddr
 * - cb_ffa_Result
 * and fills FotaLogLine + calls fota_log_add(...)
 */
	uint16_t Sec = (cb_ffa_CopiedBytes-1)/SPI_FLASH_SEC_SIZE;
	//
	uint8_t Res = spi_flash_erase_sector((cb_ffa_FreeUserbinAddr/SPI_FLASH_SEC_SIZE)+Sec);
	if(Res==SPI_FLASH_RESULT_OK){
		Res = spi_flash_write(cb_ffa_FreeUserbinAddr+((uint32_t)Sec*SPI_FLASH_SEC_SIZE), (uint32_t*)cb_ffa_SectorImg, SPI_FLASH_SEC_SIZE);
		if(Res==SPI_FLASH_RESULT_ERR) cb_ffa_Result=CB_FFA_RES_WRITE_ERR;
		else if(Res==SPI_FLASH_RESULT_TIMEOUT) cb_ffa_Result=CB_FFA_RES_WRITE_TIMEOUT;
	}
	else if(Res==SPI_FLASH_RESULT_ERR) cb_ffa_Result=CB_FFA_RES_WRITE_ERR;
	else if(Res==SPI_FLASH_RESULT_TIMEOUT) cb_ffa_Result=CB_FFA_RES_WRITE_TIMEOUT;
	//
	DBG_WHTTPD_FOTA("cb_fota_flash_data_wr(): erase sector = %d, write addr = 0x%05X, size = %d\n", (cb_ffa_FreeUserbinAddr/SPI_FLASH_SEC_SIZE)+Sec, cb_ffa_FreeUserbinAddr+((uint32_t)Sec*SPI_FLASH_SEC_SIZE), SPI_FLASH_SEC_SIZE);
	snprintf(FotaLogLine, WHTTPD_POST_FOTA_LOG_MAX_LINE_LEN, "Sect. %d, 0x%05X <- %d bytes", (cb_ffa_FreeUserbinAddr/SPI_FLASH_SEC_SIZE)+Sec, cb_ffa_FreeUserbinAddr+((uint32_t)Sec*SPI_FLASH_SEC_SIZE), SPI_FLASH_SEC_SIZE);
	fota_log_add(WHTTPD_POST_FOTA_LOG_NORMAL, FotaLogLine);
	//
	if(Res==SPI_FLASH_RESULT_ERR){
		DBG_WHTTPD_FOTA("cb_fota_flash_data_wr(): flash write error\n");
		snprintf(FotaLogLine, WHTTPD_POST_FOTA_LOG_MAX_LINE_LEN, "Flash write error");
		fota_log_add(WHTTPD_POST_FOTA_LOG_ERR, FotaLogLine);
	}
	else if(Res==SPI_FLASH_RESULT_TIMEOUT){
		DBG_WHTTPD_FOTA("cb_fota_flash_data_wr(): flash write timeout\n");
		snprintf(FotaLogLine, WHTTPD_POST_FOTA_LOG_MAX_LINE_LEN, "Flash write timeout");
		fota_log_add(WHTTPD_POST_FOTA_LOG_ERR, FotaLogLine);
	}
}


int8_t ICACHE_FLASH_ATTR cb_fota_flash_data(uint8_t Flags, uint8_t* InputData, uint16_t Bytes){
/* POST DATA CALLBACK FUNCTION for "fota_flash_data" (<form><input type="file" name="fota_flash_data" ... >)
 * --------------------------------------------
 * ! See whttpd_post_item_struct structure definition to know what is passed to / what is expected from tag callback function.
 * ! This function could be called multiple times, as POST data are received.
 */
	if(Flags & WHTTPD_POST_CBFLAG_IS_FIRST_DATA_BLOCK){
		if(SPI_FLASH_SEC_SIZE > NOT_FLAG16(0)) return -1; //uint16_t Bytes can't hold the sizes we would be working with
		//
		if(cb_ffa_SectorImg!=NULL) free(cb_ffa_SectorImg); //this can happen only when caller doesn't work properly (flag WHTTPD_POST_CBFLAG_IS_LAST_DATA_BLOCK not set for the last call)
		cb_ffa_SectorImg = malloc(SPI_FLASH_SEC_SIZE);
		cb_ffa_CopiedBytes = 0;
		cb_ffa_Result = CB_FFA_RES_INITED;
		//
		uint32_t StartAddr = system_get_userbin_addr(); //slot in use
		uint32_t ID = spi_flash_get_id();
		uint32_t FlashSizeB = ((uint32_t)1)<<((uint8_t*)&ID)[2]; //flash size in bytes
		cb_ffa_MaxLen = (FlashSizeB/2) - ((StartAddr>(FlashSizeB/2))?StartAddr-(FlashSizeB/2):StartAddr) - 0x04000; //(FlashSizeB/2) - start offset of usercode - 16 kB for system parameter area
		//
		if(FlashSizeB>(uint32_t)2*1024*1024) return -1; //memory layouts for flash > 2 MB are not supported yet (we know only symmetric slots)
		//
		cb_ffa_FreeUserbinAddr = (StartAddr>(FlashSizeB/2)) ? StartAddr-(FlashSizeB/2) : StartAddr+(FlashSizeB/2);
		//
		MD5_Init(&cb_ffa_MD5Ctx);
	}
	if((cb_ffa_SectorImg==NULL)||(cb_ffa_FreeUserbinAddr==0)) return -1;
	//
	uint16_t InDataPos = 0;
	while(Bytes>0){
		uint16_t SecWrPos = cb_ffa_CopiedBytes % SPI_FLASH_SEC_SIZE;
		uint16_t CpSz = Bytes;
		if(CpSz>SPI_FLASH_SEC_SIZE-SecWrPos) CpSz = SPI_FLASH_SEC_SIZE-SecWrPos;
		//
		memcpy(&cb_ffa_SectorImg[SecWrPos], &InputData[InDataPos], CpSz);
		DBG_WHTTPD_FOTA("cb_fota_flash_data(): memcpy SectorImg[%d] <- InputData[%d], %d bytes\n", SecWrPos, InDataPos, CpSz);
		//
		MD5_Update(&cb_ffa_MD5Ctx, &InputData[InDataPos], CpSz);
		cb_ffa_CopiedBytes += CpSz;
		InDataPos += CpSz;
		Bytes -= CpSz;
		//
		if(cb_ffa_Result==CB_FFA_RES_INITED){ //all ok so far
			if(cb_ffa_CopiedBytes>cb_ffa_MaxLen){
				DBG_WHTTPD_FOTA("cb_fota_flash_data(): received data too big, MaxLen = %d bytes\n", cb_ffa_MaxLen);
				snprintf(FotaLogLine, WHTTPD_POST_FOTA_LOG_MAX_LINE_LEN, "Received data too big, MaxLen = %d bytes", cb_ffa_MaxLen);
				fota_log_add(WHTTPD_POST_FOTA_LOG_ERR, FotaLogLine);
				//
				cb_ffa_Result = CB_FFA_RES_TOO_BIG;
			}
			if((cb_ffa_Result==CB_FFA_RES_INITED)&&(cb_ffa_CopiedBytes % SPI_FLASH_SEC_SIZE == 0)&&(cb_ffa_CopiedBytes>=SPI_FLASH_SEC_SIZE)){ //we have full sector image => write sector to flash
				cb_fota_flash_data_wr();
			}
		}
	}
	//
	if(Flags & WHTTPD_POST_CBFLAG_IS_LAST_DATA_BLOCK){
		if((cb_ffa_Result==CB_FFA_RES_INITED)&&(cb_ffa_CopiedBytes % SPI_FLASH_SEC_SIZE != 0)){ //some data are remaining to be written (cb_ffa_SectorImg contains data, but not full)
			//
			MD5_Final(cb_ffa_MD5Digest, &cb_ffa_MD5Ctx);
			//
			//fill up remaining space by 0xFF
			uint16_t SecWrPos = cb_ffa_CopiedBytes % SPI_FLASH_SEC_SIZE;
			uint16_t FillFFSz = SPI_FLASH_SEC_SIZE - SecWrPos;
			memset(&cb_ffa_SectorImg[SecWrPos], 0xFF, FillFFSz);
			DBG_WHTTPD_FOTA("cb_fota_flash_data(): memset SectorImg[%d] <- %d x 0xFF\n", SecWrPos, FillFFSz);
			//
			cb_fota_flash_data_wr();
			//
			if(cb_ffa_Result==CB_FFA_RES_INITED) cb_ffa_Result=CB_FFA_RES_OK;
		}
		if(cb_ffa_Result!=CB_FFA_RES_OK) MD5_Init(&cb_ffa_MD5Ctx);
		//
		char DigestS[(MD5_SIZE*2)+1]; //+1 for null termination
		uint8_t Idx;
		for(Idx=0;Idx<MD5_SIZE;Idx++) sprintf(&DigestS[Idx*2], "%02X", cb_ffa_MD5Digest[Idx]); //last sprintf leaves the right null termination
		//
		DBG_WHTTPD_FOTA("cb_fota_flash_data(): DONE, received %d bytes, Result = %d, MD5 = '%s'\n", cb_ffa_CopiedBytes, cb_ffa_Result, DigestS);
		snprintf(FotaLogLine, WHTTPD_POST_FOTA_LOG_MAX_LINE_LEN, "Received %d bytes, Result = %d, MD5 = '%s'", cb_ffa_CopiedBytes, cb_ffa_Result, DigestS);
		fota_log_add((cb_ffa_Result==CB_FFA_RES_OK)?WHTTPD_POST_FOTA_LOG_OK:WHTTPD_POST_FOTA_LOG_ERR, FotaLogLine);
		//
		free(cb_ffa_SectorImg);
		cb_ffa_SectorImg = NULL;
	}
	return 0;
}

char cb_ffdmd5_RecvDigestS[(MD5_SIZE*2)+1]; //+1 for null termination
uint8_t cb_ffdmd5_RecvPos = 0;
uint8_t cb_ffdmd5_OK = 0;
int8_t ICACHE_FLASH_ATTR cb_fota_flash_data_md5(uint8_t Flags, uint8_t* InputData, uint16_t Bytes){
/* POST DATA CALLBACK FUNCTION for "fota_flash_data_md5" (<form><input type="file" name="fota_flash_data_md5" ... >)
 * --------------------------------------------
 * ! See whttpd_post_item_struct structure definition to know what is passed to / what is expected from tag callback function.
 * ! This function could be called multiple times, as POST data are received.
 */
	if(Flags & WHTTPD_POST_CBFLAG_IS_FIRST_DATA_BLOCK){
		cb_ffdmd5_RecvPos = 0;
		cb_ffdmd5_OK = 0;
	}
	//
	uint8_t Cnt = 0;
	while((cb_ffdmd5_RecvPos<(MD5_SIZE*2))&&(Cnt<Bytes)){ //fill cb_ffdmd5_RecvDigestS[] up to MD5_SIZE*2 chars
		cb_ffdmd5_RecvDigestS[cb_ffdmd5_RecvPos++] = InputData[Cnt++];
	}
	//
	if(Flags & WHTTPD_POST_CBFLAG_IS_LAST_DATA_BLOCK){
		cb_ffdmd5_RecvDigestS[MD5_SIZE*2]=0; //add null termination
		//
		char DigestS[(MD5_SIZE*2)+1]; //+1 for null termination
		uint8_t Idx;
		for(Idx=0;Idx<MD5_SIZE;Idx++) sprintf(&DigestS[Idx*2], "%02X", cb_ffa_MD5Digest[Idx]); //last sprintf leaves the right null termination
		cb_ffdmd5_OK = (strcmp(cb_ffdmd5_RecvDigestS, DigestS)==0);
		//
		DBG_WHTTPD_FOTA("cb_fota_flash_data_md5(): received control MD5 = '%s' => %s\n", cb_ffdmd5_RecvDigestS, (cb_ffdmd5_OK)?"OK":"mismatch");
		snprintf(FotaLogLine, WHTTPD_POST_FOTA_LOG_MAX_LINE_LEN, "Received control MD5 = '%s' => %s", cb_ffdmd5_RecvDigestS, (cb_ffdmd5_OK)?"OK":"mismatch");
		fota_log_add((cb_ffdmd5_OK)?WHTTPD_POST_FOTA_LOG_OK:WHTTPD_POST_FOTA_LOG_ERR, FotaLogLine);
	}
	return 0;
}

char* cb_fp_RecvDigestS = NULL;
uint8_t cb_fp_RecvPos = 0;
uint8_t cb_fp_OK = 0;
int8_t ICACHE_FLASH_ATTR cb_fota_pwd(uint8_t Flags, uint8_t* InputData, uint16_t Bytes){
/* POST DATA CALLBACK FUNCTION for "fota_pwd" (<form><input type="password" name="fota_pwd" ... >)
 * --------------------------------------------
 * ! See whttpd_post_item_struct structure definition to know what is passed to / what is expected from tag callback function.
 * ! This function could be called multiple times, as POST data are received.
 */
	if(Flags & WHTTPD_POST_CBFLAG_IS_FIRST_DATA_BLOCK){
		if(cb_fp_RecvDigestS!=NULL) free(cb_fp_RecvDigestS); //this can happen only when caller doesn't work properly (flag WHTTPD_POST_CBFLAG_IS_LAST_DATA_BLOCK not set for the last call)
		cb_fp_RecvDigestS = malloc((MD5_SIZE*2)+1); //+1 for null termination
		cb_fp_RecvPos = 0;
		cb_fp_OK = 0;
	}
	if(cb_fp_RecvDigestS==NULL) return -1;
	//
	uint8_t Cnt = 0;
	while((cb_fp_RecvPos<(MD5_SIZE*2))&&(Cnt<Bytes)){ //fill cb_fp_RecvPos[] up to MD5_SIZE*2 chars
		cb_fp_RecvDigestS[cb_fp_RecvPos++] = InputData[Cnt++];
	}
	//
	if(Flags & WHTTPD_POST_CBFLAG_IS_LAST_DATA_BLOCK){
		cb_fp_RecvDigestS[MD5_SIZE*2]=0; //add null termination
		char* Prefix = (char*)whttpd_preproc_fota_get_pwd_prefix();
		MD5_CTX Ctx;
		MD5_Init(&Ctx);
		MD5_Update(&Ctx, Prefix, strlen(Prefix));
		MD5_Update(&Ctx, WHTTPD_FOTA_UPGRADE_PWD, strlen(WHTTPD_FOTA_UPGRADE_PWD));
		uint8_t Digest[MD5_SIZE];
		MD5_Final(Digest, &Ctx);
		//
		char DigestS[(MD5_SIZE*2)+1]; //+1 for null termination
		uint8_t Idx;
		for(Idx=0;Idx<MD5_SIZE;Idx++) sprintf(&DigestS[Idx*2], "%02X", Digest[Idx]); //last sprintf leaves the right null termination
		cb_fp_OK = (strcmp(cb_fp_RecvDigestS, DigestS)==0);
		//
		DBG_WHTTPD_FOTA("cb_fota_pwd(): prefix = '%s', computed MD5 = '%s', received MD5 = '%s' => %s\n", Prefix, DigestS, cb_fp_RecvDigestS, (cb_fp_OK)?"OK":"mismatch");
		snprintf(FotaLogLine, WHTTPD_POST_FOTA_LOG_MAX_LINE_LEN, "Password %s", (cb_fp_OK)?"OK":"mismatch");
		fota_log_add((cb_fp_OK)?WHTTPD_POST_FOTA_LOG_OK:WHTTPD_POST_FOTA_LOG_ERR, FotaLogLine);
		//
		free(cb_fp_RecvDigestS); //this can happen only when caller of cb_fota_pwd doesn't work properly (flag WHTTPD_POST_CBFLAG_IS_LAST_DATA_BLOCK not set for the last call)
		cb_fp_RecvDigestS = NULL;
	}
	return 0;
}

//----

void ICACHE_FLASH_ATTR fota_init(void){
	cb_ffa_CopiedBytes = 0;
	cb_ffa_Result = CB_FFA_RES_INITED;
	cb_ffdmd5_OK = 0;
	cb_fp_OK = 0;
	//
	if(FotaLog!=NULL) free(FotaLog);
	FotaLog = NULL;
	FotaLogLen = 0;
	FotaLogPos = 0;
	FotaLogDisable = 0;
}

char** ICACHE_FLASH_ATTR fota_commit(void){
	if((cb_ffa_Result==CB_FFA_RES_OK)&&(cb_ffdmd5_OK)&&(cb_fp_OK)){
		DBG_WHTTPD_FOTA("fota_commit(): ALL OK, switching active FOTA slot, rebooting ...\n");
		snprintf(FotaLogLine, WHTTPD_POST_FOTA_LOG_MAX_LINE_LEN, "ALL OK, switching active FOTA slot, rebooting ...");
		fota_log_add(WHTTPD_POST_FOTA_LOG_OK, FotaLogLine);
		//
		system_upgrade_flag_set(UPGRADE_FLAG_FINISH);
		whttpd_upgrade_reboot_when_no_active_slot();
		//
		return &FotaLog;
	}
	else if(cb_ffa_Result==CB_FFA_RES_INITED){
		DBG_WHTTPD_FOTA("fota_commit(): no or incomplete firmware data\n");
		snprintf(FotaLogLine, WHTTPD_POST_FOTA_LOG_MAX_LINE_LEN, "No or incomplete firmware data");
		fota_log_add(WHTTPD_POST_FOTA_LOG_ERR, FotaLogLine);
	}
	//already reported:
	//else if(cb_ffa_Result==CB_FFA_RES_TOO_BIG)
	//else if(cb_ffa_Result==CB_FFA_RES_WRITE_ERR)
	//else if(cb_ffa_Result==CB_FFA_RES_WRITE_TIMEOUT)
	//else if(!cb_ffdmd5_OK)
	//else if(!cb_fp_OK)
	//
	DBG_WHTTPD_FOTA("fota_commit(): aborting FOTA upgrade\n");
	snprintf(FotaLogLine, WHTTPD_POST_FOTA_LOG_MAX_LINE_LEN, "Aborting FOTA upgrade");
	fota_log_add(WHTTPD_POST_FOTA_LOG_ERR, FotaLogLine);
	return &FotaLog;
}

void ICACHE_FLASH_ATTR fota_log_add(whttpd_post_fota_log_type_enum LogType, char* Line){
/* Allocates memory for FotaLog. Don't forget to call fota_finish().
 * ! WHTTPD_POST_FOTA_LOG_MAX_LINE_LEN must be big enough to accommodate all possible combinations of log lines + prefixes + suffixes + null termination
 */
	if(FotaLogDisable) return;
	//
	//now let's wrap Line into the HTML prefix, suffix and newline
	char* Prefix = "";
	char* Suffix = "";
	switch(LogType){
		case WHTTPD_POST_FOTA_LOG_NORMAL:
			Prefix = WHTTPD_POST_FOTA_LOG_HTML_NORMAL_PREFIX;
			Suffix = WHTTPD_POST_FOTA_LOG_HTML_NORMAL_SUFFIX;
			break;
		case WHTTPD_POST_FOTA_LOG_ACCENT:
			Prefix = WHTTPD_POST_FOTA_LOG_HTML_ACCENT_PREFIX;
			Suffix = WHTTPD_POST_FOTA_LOG_HTML_ACCENT_SUFFIX;
			break;
		case WHTTPD_POST_FOTA_LOG_OK:
			Prefix = WHTTPD_POST_FOTA_LOG_HTML_OK_PREFIX;
			Suffix = WHTTPD_POST_FOTA_LOG_HTML_OK_SUFFIX;
			break;
		case WHTTPD_POST_FOTA_LOG_ERR:
			Prefix = WHTTPD_POST_FOTA_LOG_HTML_ERR_PREFIX;
			Suffix = WHTTPD_POST_FOTA_LOG_HTML_ERR_SUFFIX;
			break;
	}
	//
	uint16_t LineLen = strlen(Line);
	//
	//add prefix to Line
	uint8_t AddLen;
	AddLen = strlen(Prefix);
	memmove(&Line[AddLen], Line, LineLen+1); //make space for prefix - shift right (+1 to copy also null termination)
	strncpy(Line, Prefix, AddLen); //prefix without null termination
	LineLen += AddLen;
	//
	//add suffix to Line
	strcpy(&Line[LineLen], Suffix); //suffix with copy also null termination
	LineLen += strlen(Suffix);
	//
	//add newline ending to Line
	strcpy(&Line[LineLen], WHTTPD_POST_FOTA_LOG_HTML_NEWLINE);
	LineLen += strlen(WHTTPD_POST_FOTA_LOG_HTML_NEWLINE);
	//
	//add to FotaLog (make space if needed, jump by WHTTPD_POST_FOTA_LOG_MALLOC_STEP)
	uint16_t RemSpace = FotaLogLen - FotaLogPos;
	if(RemSpace<LineLen+1){ //need realloc
		uint16_t NewSize = FotaLogLen + ((LineLen+1) - RemSpace);
		NewSize = ((NewSize/WHTTPD_POST_FOTA_LOG_MALLOC_STEP)+1)*WHTTPD_POST_FOTA_LOG_MALLOC_STEP;
		char* NewPtr = realloc(FotaLog, NewSize);
		if(NewPtr==NULL){ //can't reallocate => free and turn logging off
			if(FotaLog!=NULL) free(FotaLog);
			FotaLog = NULL;
			FotaLogLen = 0;
			FotaLogPos = 0;
			FotaLogDisable = 1;
			return;
		}
		FotaLog = NewPtr;
		FotaLogLen = NewSize;
	}
	strcpy(&FotaLog[FotaLogPos], Line);
	FotaLogPos += LineLen;
}
