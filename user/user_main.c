/*
 * Created by Martin Winkelhofer 02,03/2016
 * W-Dimension / wdim / wdim0 / maarty.w@gmail.com
 */

//#include <freertos/FreeRTOS.h>
//#include <freertos/task.h>
#include <espressif/esp_common.h>
#include <whttpd.h>

#include "user_config.h"

void ICACHE_FLASH_ATTR user_init(void){
	DBG("user_init() started\n");
	wifi_set_opmode(SOFTAP_MODE);
	xTaskCreate(whttpd_main_task, "whttpd", WHTTPD_STACK_SIZE, NULL, 2, NULL);
}
