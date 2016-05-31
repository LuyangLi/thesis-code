/*
 * wod logging tasks
 *
 *  Created on: 26/04/2016
 *      Author: Luyang
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <util/error.h>
#include <util/log.h>
#include <csp/csp.h>
#include <csp/csp_endian.h>
#include <../nanocom/nanocom.h>
#include <../nanopower/io/nanopower2.h>
#include <../a712/nanomind.h>
#include <math.h>

#include "wod.h"
#include "beacon_io.h"


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


/*For set config, tasks = 1
has a minimum interval of 10 seconds. */
void set_beacon_config(void) {
	beacon_config_t data = current_config;
	int i = 0;
	data.tasks = 1;
	data.interval = (current_config.interval < 10 ) ? csp_hton16(10) : csp_hton16(current_config.interval);
	data.postpone = (current_config.postpone < 10 ) ? csp_hton16(10) : csp_hton16(current_config.postpone);

	for (i = 0; i < 6; i++) {
		data.callsign[i] = current_config.callsign[i];
	}
	int packet_status = csp_transaction(CSP_PRIO_NORM, NODE_OBC, OBC_PORT_BEACON, 5000,
	                                    &data , sizeof(beacon_config_t), &data, sizeof(beacon_config_t));
	data.interval = csp_ntoh16(data.interval);
	data.postpone = csp_ntoh16(data.postpone);
	if ((packet_status == sizeof(beacon_config_t))) {
		printf("success\n");
		data.tasks = ~(data.tasks);
		current_config = data;
		cmd_wod_print_beacon_config(&current_config);
		return;
	} else {
		printf("NETWORK ERROR! %d\n", packet_status);
	}
}

/*For set config, tasks = 2*/
void get_beacon_config(void) {
	beacon_config_t data;
	int i = 0;
	data.tasks = 2;
	int packet_status = csp_transaction(CSP_PRIO_NORM, NODE_OBC, OBC_PORT_BEACON, 5000,
	                                    &data , sizeof(beacon_config_t), &data, sizeof(beacon_config_t));
	//return bitwise not when reached this function.
	if ((packet_status == sizeof(beacon_config_t))) {
		printf("success\n");
		data.interval = csp_ntoh16(data.interval);
		data.postpone = csp_ntoh16(data.postpone);
		data.tasks = ~(data.tasks);
		current_config = data;
		cmd_wod_print_beacon_config(&current_config);
		return;
	} else {
		printf("NETWORK ERROR! %d\n", packet_status);
	}
}


void set_beacon_enable(uint8_t status) {
	if (status) {
		current_config.beacon_enable = 1;
		return;
	}
	current_config.beacon_enable = 0;
}


void cmd_wod_print_beacon_config(beacon_config_t * beacon_config) {
	//printf("last contact at :%d, %d sec ago.\n", last_gnd_contact, getRealTime() - last_gnd_contact);
	printf("task: %u\n", beacon_config->tasks);
	printf("beacon status: %s\n", (beacon_config->beacon_enable) ? "on" : "off");
	printf("beacon interval: %u\n", beacon_config->interval);
	printf("beacon postpone: %u\n", beacon_config->postpone);
	printf("beacon callsign: %.6s\n", beacon_config->callsign);
}

