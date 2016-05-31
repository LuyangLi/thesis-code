/*
 * wod beacon daemon
 *
 *  Created on: 06/05/2016
 *      Author: luyang
 */

#include <stdio.h>
#include <unistd.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <util/error.h>
#include <util/clock_sync.h>
#include <io/nanopower2.h>
#include <io/nanocom.h>
#include <wod/wod.h>
#include <wod/beacon_io.h>


void vTaskWodDaemon(void * pvParameters) {
	portTickType xLastBeacon = xTaskGetTickCount();
	log_last_gnd_contact();
	init_beacon_config();
	while (1) {
		if (wod_enable) {
			eps_hk_get(&wod_eps_hk);
			wod_log(); // log BEFORE you get hk
		}
		if ((current_config.beacon_enable) && (getRealTime() - last_gnd_contact > current_config.postpone)) {
			wod_beacon_out();
			vTaskDelayUntil(&xLastBeacon, (current_config.interval < 10) ? 10 : current_config.interval * configTICK_RATE_HZ);
		} else {
			vTaskDelayUntil(&xLastBeacon, 10.0 * configTICK_RATE_HZ);
		}
		xLastBeacon = xTaskGetTickCount();
	}
	//should never reach here.
	vTaskDelete(NULL);
}
