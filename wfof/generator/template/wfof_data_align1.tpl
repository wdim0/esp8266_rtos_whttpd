/*
 * GENERATED BY [[APPNAME]] on [[DATETIME]]
 *
 * Created by Martin Winkelhofer 02,03/2016
 * W-Dimension / wdim / wdim0 / winkelhofer.m@gmail.com / https://github.com/wdim0
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
#ifndef __WFOF_DATA_H__
#define __WFOF_DATA_H__
//
#include "wfof.h"
//
//main WFOF structure describing content of wfof_Data
const wfof_ftab_struct wfof_FTab = {
	.FilesCnt=[[FILESCNT]],
	.AlignBytes=[[ALIGNBYTES]],
	.Files={
[[FILES]]
	}
};
//
//binary data to be written into SPI FLASH
const ICACHE_STORE_ATTR ICACHE_RODATA_ATTR uint8_t wfof_Data[]={
[[FILESDATA]]
};
//
#endif
