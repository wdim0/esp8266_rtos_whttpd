/*
 * Created by Martin Winkelhofer 02,03/2016
 * W-Dimension / wdim / maarty.w@gmail.com
 *    _____ __          ____         ______         __
 *   / __(_) /__ ___   / __ \___    / __/ /__ ____ / /
 *  / _// / / -_|_-<  / /_/ / _ \  / _// / _ `(_-</ _ \
 * /_/ /_/_/\__/___/  \____/_//_/ /_/ /_/\_,_/___/_//_/
 *
 * This file is part of WFOF - W-Dimension's Files On Flash (for ESP8266).
 *
 * WFOF is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * WFOF is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with WFOF. If not, see <http://www.gnu.org/licenses/>.
 */

#include <espressif/esp_common.h>

#include "wfof.h"      //WFOF types/structures
#include "wfof_data.h" //WFOF data array with data to be written on FLASH (files in dir "wfof/content") <- generated by wfof_gen program

int8_t ICACHE_FLASH_ATTR wfof_get_file_info(char* FName, uint8_t* RetFileIndex, uint32_t* RetSize){
/* Tries to find file of name FName in the list of files that were in dir "wfof/content" when generating wfof_data.h
 * If such file exists (has been included into wfof_Data[] => is written on SPI FLASH):
 * - fills RetFileIndex by file index (that can be used by wfof_get_file_data(...))
 * - fills RetSize by file size
 * - returns 0
 * If such file doesn't exist:
 * - fills RetFileIndex by file WFOF_INVALID_INDEX
 * - fills RetSize by 0
 * - returns -1
 */
	uint8_t FIdx;
	for(FIdx=0;FIdx<wfof_FTab.FilesCnt;FIdx++){
		if(strcasecmp(wfof_FTab.Files[FIdx].Name,FName)==0){
			*RetFileIndex = FIdx;
			*RetSize = wfof_FTab.Files[FIdx].Size;
			return 0;
		}
	}
	*RetFileIndex = WFOF_INVALID_INDEX;
	*RetSize = 0;
	return -1;
}

uint32_t ICACHE_FLASH_ATTR wfof_get_file_data(uint8_t FileIndex, uint8_t* RetBuf, uint32_t Offs, uint32_t Bytes){
/* Fills buffer RetBuf by data of file specified by FileIndex.
 * Offs is zero based offset in respect of the start of the file data.
 * Bytes is how many bytes you want to get (you can get less than specified if the file is smaller than that).
 * Returns number of really copied bytes on success.
 * Returns 0 if such FileIndex doesn't exist OR the offset is beyond the end of the file OR reading of SPI FLASH failed.
 * spi_flash_read(...) sometimes crashes even if it's 4-bytes aligned :/
 */
	if((FileIndex>=wfof_FTab.FilesCnt)||(Bytes==0)) return 0;
	int32_t RemainingBytes = (int32_t)wfof_FTab.Files[FileIndex].Size - Offs;
	if(RemainingBytes<=0) return 0;
	if(RemainingBytes<Bytes) Bytes = RemainingBytes;
	//
	//uint32_t ReadAddr = ((uint32_t)wfof_Data) - WFOF_SPIFLASH_OFFS + wfof_FTab.Files[FileIndex].Offs + Offs; //when using spi_flash_read(...)
	uint32_t ReadAddr = ((uint32_t)wfof_Data) + wfof_FTab.Files[FileIndex].Offs + Offs;
	uint32_t FirstU32AlignedAddr = ReadAddr & 0xFFFFFFFC; //mask out last 2 bits => align to 4 bytes addressing
	uint8_t FirstU32AlignedOffs = ReadAddr % 4;
	uint32_t LastU32AlignedAddr = (ReadAddr+(Bytes-1)) & 0xFFFFFFFC;
	uint8_t LastU32AlignedOffs = (ReadAddr+(Bytes-1)) % 4;
	uint32_t U32Tmp;
	uint32_t Len;
	//
	/* Even if all reading is 4-bytes aligned, spi_flash_read(...) sometime crashes - Fatal exception (9)
	 * So we have to read memory directly: U32Tmp = *(uint32_t*)FirstU32AlignedAddr.
	 *
	if(spi_flash_read(FirstU32AlignedAddr, &U32Tmp, 4) != SPI_FLASH_RESULT_OK) return 0;
	Len = 4-FirstU32AlignedOffs;
	if(Len>Bytes) Len = Bytes;
	memcpy(&RetBuf[0], &(((uint8_t*)&U32Tmp)[FirstU32AlignedOffs]), Len);
	if(Len==Bytes) return Bytes;
	//
	if(spi_flash_read(LastU32AlignedAddr, &U32Tmp, 4) != SPI_FLASH_RESULT_OK) return 0;
	Len = LastU32AlignedOffs+1;
	memcpy(&RetBuf[(Bytes-1)-LastU32AlignedOffs], &U32Tmp, Len);
	//
	Len = LastU32AlignedAddr-FirstU32AlignedAddr; //Len can be here only multiples of 4 (0,4,8,16,...)
	//if((Len>4)&&(spi_flash_read(FirstU32AlignedAddr+4, (uint32_t*)&RetBuf[4-FirstU32AlignedOffs], Len-4) != SPI_FLASH_RESULT_OK)) return 0;
	if((Len>4)&&(spi_flash_read(FirstU32AlignedAddr+4, (uint32_t*)&(RetBuf[4-FirstU32AlignedOffs]), Len-4) != SPI_FLASH_RESULT_OK)) return 0;
	*/
	//this code doesn't crash
	//
	U32Tmp = *(uint32_t*)FirstU32AlignedAddr;
	Len = 4-FirstU32AlignedOffs;
	if(Len>Bytes) Len = Bytes;
	memcpy(&RetBuf[0], &(((uint8_t*)&U32Tmp)[FirstU32AlignedOffs]), Len);
	if(Len==Bytes) return Bytes;
	//
	U32Tmp = *(uint32_t*)LastU32AlignedAddr;
	Len = LastU32AlignedOffs+1;
	memcpy(&RetBuf[(Bytes-1)-LastU32AlignedOffs], &U32Tmp, Len);
	//
	Len = LastU32AlignedAddr-FirstU32AlignedAddr; //Len can be here only multiples of 4 (0,4,8,16,...)
	if(Len>4){ //there are still bytes pending to copy (in the middle between FirstU32AlignedAddr and LastU32AlignedAddr) - now is everything 4-bytes aligned
		uint32_t U32TmpArr[WFOF_U32_TMP_ARR_SIZE];
		uint8_t ArrIdx = 0;
		uint32_t Iters = (Len-4)/4; //-4 to discount already copied 4-byte
		uint32_t RetBufIdx = 4-FirstU32AlignedOffs;
		while(Iters-->0){ //iterate the loop Iters times
			FirstU32AlignedAddr += 4;
			//*(uint32_t*)(&RetBuf[RetBufIdx]) = *(uint32_t*)FirstU32AlignedAddr; //this also crashes :/
			U32TmpArr[ArrIdx++] = *(uint32_t*)FirstU32AlignedAddr;
			if((ArrIdx==WFOF_U32_TMP_ARR_SIZE)||(Iters==0)){ //we got full U32TmpArr[] or we're at the end of copying
				Len = (uint16_t)ArrIdx*4;
				memcpy(&RetBuf[RetBufIdx], U32TmpArr, Len);
				RetBufIdx += Len;
				ArrIdx = 0;
			}
		}
	}
	//
	return Bytes;
}

int8_t ICACHE_FLASH_ATTR wfof_find_char_pos(uint8_t FileIndex, uint32_t Offs, char Ch, uint32_t* RetPos){
/* Tries to locate first occurrence of char Ch in data of file specified by FileIndex,
 * starting the search at offset Offs (in respect to first data byte of the file).
 * Returns 0 and fills RetPos if Ch was found. RetPos is the relative to Offs.
 * Returns -1 if Ch was not found OR FileIndex doesn't exist OR the offset is beyond the end of the file OR reading of SPI FLASH failed.
 */
	if(FileIndex>=wfof_FTab.FilesCnt) return -1;
	if(Offs>=wfof_FTab.Files[FileIndex].Size) return -1;
	//
	uint32_t ReadAddr = ((uint32_t)wfof_Data) + wfof_FTab.Files[FileIndex].Offs + Offs;
	uint32_t U32AlignedAddr = ReadAddr & 0xFFFFFFFC; //mask out last 2 bits => align to 4 bytes addressing
	uint8_t U32AlignedOffs = ReadAddr % 4;
	uint32_t U32Tmp;
	//
	U32Tmp = *(uint32_t*)U32AlignedAddr; //load 4 bytes into U32Tmp
	//
	uint32_t Idx = ((uint32_t)0)-1; //because ++Idx in while(...) and we want to start at 0
	char CurrCh;
	while(++Idx+Offs < wfof_FTab.Files[FileIndex].Size){ //we'll not be reading beyond the file's end
		CurrCh = U32Tmp>>(U32AlignedOffs*8);
		if(CurrCh==Ch){
			*RetPos = Idx;
			return 0;
		}
		if(++U32AlignedOffs>3){
			U32AlignedAddr += 4;
			U32AlignedOffs = 0;
			U32Tmp = *(uint32_t*)U32AlignedAddr; //load next 4 bytes into U32Tmp
		}
	}
	return -1;
}
