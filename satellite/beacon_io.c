/*
 * wod beacon functions
 *
 *  Created on: 26/04/2016
 *      Author: Luyang
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <util/error.h>
#include <util/log.h>
#include <csp/csp.h>
#include <csp/csp_endian.h>

#include <io/nanopower2.h>
#include <io/nanocom.h>
#include <experiments.h>
#include <math.h>

#include <runlevels.h>
#include <util/clock_sync.h>
#include <wod/wod.h>
#include <wod/beacon_io.h>


static const char callsign[6] = {'O', 'N', '0', '2', 'A', 'U'};
static beacon_packet_t current_beacon;

beacon_config_t current_config;
int32_t last_gnd_contact = 0;

/* Booting initialise sequence */
void init_beacon_config(void) {
	int i = 0;
	current_config.tasks = 0;
	current_config.beacon_enable = 0;
	current_config.interval = 30;
	current_config.postpone = 900;
	for (i = 0; i < 6; i++) {
		current_config.callsign[i] = callsign[i];
	}
}

/* log the last contact from the ground */
void log_last_gnd_contact(void) {
	last_gnd_contact = getRealTime();
}

void create_current_beacon_packet(void) {
	int i = 0;
	for (i = 0; i < 6; i++) {
		current_beacon.callsign[i] = current_config.callsign[i];
	}
	current_beacon.timestamp = csp_hton32(getRealTime());
	current_beacon.flags = current_wod_data.flags;
	current_beacon.batt_voltage = current_wod_data.batt_voltage;
	current_beacon.current_in = current_wod_data.current_in;
	current_beacon.current_out = current_wod_data.current_out;
	current_beacon.rail3_current = current_wod_data.rail3_current;
	current_beacon.rail5_current = current_wod_data.rail5_current;
	current_beacon.com_temp = current_wod_data.com_temp;
	current_beacon.eps_temp = current_wod_data.eps_temp;
	current_beacon.bat_temp = current_wod_data.bat_temp;
}

/*For set config, tasks = 1
has a minimum interval of 10 seconds. */
void set_beacon_config(beacon_config_t * beacon_config) {
	int i = 0;
	current_config.beacon_enable = beacon_config->beacon_enable;
	current_config.interval = (csp_ntoh16(beacon_config->interval) < 10 ) ? 10 : csp_ntoh16(beacon_config->interval);
	current_config.postpone = (csp_ntoh16(beacon_config->postpone) < 10 ) ? 10 : csp_ntoh16(beacon_config->postpone);
	for (i = 0; i < 6; i++) {
		current_config.callsign[i] = beacon_config->callsign[i];
	}
	//return bitwise not when reached this function.
	beacon_config->tasks = ~(beacon_config->tasks);
}

/*For set config, tasks = 2*/
void get_beacon_config(beacon_config_t * beacon_config) {
	int i = 0;
	beacon_config->beacon_enable = current_config.beacon_enable;
	beacon_config->interval = csp_hton16(current_config.interval);
	beacon_config->postpone = csp_hton16(current_config.postpone);
	for (i = 0; i < 6; i++) {
		beacon_config->callsign[i] = current_config.callsign[i];
	}
	//return bitwise not when reached this function.
	beacon_config->tasks = ~(beacon_config->tasks);
}

/* 0 = off, else = on*/
void set_beacon_enable(uint8_t status) {
	if (status) {
		current_config.beacon_enable = 1;
		return;
	}
	current_config.beacon_enable = 0;
}

/* Send a beacon out to the space. return 0 for succss, -1 for failed in csp_transaction, -2 for beacon disabled. */
int wod_beacon_out(void) {
	if (current_config.beacon_enable) {
		create_current_beacon_packet();
		int ret = csp_transaction(CSP_PRIO_LOW, 10, 40, 100, (void *) &current_beacon, sizeof(beacon_packet_t), NULL, 0);
		//hex_dump(&current_beacon, sizeof(beacon_packet_t));
		return (ret == 1) ? 0 : -1;
	}
	return -2;
}

void cmd_wod_print_beacon_config(beacon_config_t * beacon_config) {
	printf("last contact at :%d, %d sec ago.\n", last_gnd_contact, getRealTime() - last_gnd_contact);
	printf("beacon status: %s\n", (beacon_config->beacon_enable) ? "on" : "off");
	printf("beacon interval: %u\n", beacon_config->interval);
	printf("beacon postpone: %u\n", beacon_config->postpone);
	printf("beacon callsign: %.6s\n", beacon_config->callsign);
}
