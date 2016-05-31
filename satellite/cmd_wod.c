/*
 * cmd_wod.c
 *
 *  Created on: 01/05/2016
 *      Author: Luyang
 */
#include <stdlib.h>
#include <string.h>
#include <io/nanocom.h>
#include <io/nanopower2.h>
#include <command/command.h>
#include <inttypes.h>

#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <command/command.h>
#include <inttypes.h>
#include <util/log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <util/console.h>
#include <util/bytesize.h>
#include <util/crc32.h>
#include <util/hexdump.h>
#include <dev/usart.h>
#include <dev/cpu.h>
#include <fcntl.h>
#include <wod/wod.h>
#include <wod/beacon_io.h>
#include <conf_gomspace.h>

static uint8_t previous = 0;

int cmd_wod_log_rmall(struct command_context *ctx) {
	previous = wod_enable;
	wod_enable = 0;
	wod_reset();
	wod_enable = previous;
	return CMD_ERROR_NONE;
}

int cmd_wod_log_reset(struct command_context *ctx) {
	previous = wod_enable;
	wod_enable = 0;
	if (set_wodnum_cmd(0) != 1) {
		wod_enable = 1;
		return CMD_ERROR_FAIL;
	}
	wod_enable = previous;
	return CMD_ERROR_NONE;
}

int cmd_wod_log_hk(struct command_context *ctx) {
	uint32_t number = 0;
	int32_t time_diff = 0;
	char path[25] = "";
	if (get_wodnum_cmd(&number) != 1)
		return CMD_ERROR_FAIL;
	snprintf(path, 25, "/sd/%02X.wod", number);
	if (read_log_time(path, &time_diff) != 0) {
		printf("cannot open %s.\n", path);
		return CMD_ERROR_FAIL;
	}
	printf("Current wodnum %02X, wod status: %s, time_diff = %d min %d sec.\n", number, wod_enable ? "on" : "off", (int)time_diff / 60, (int)time_diff % 60 );
	return CMD_ERROR_NONE;
}

int cmd_wod_log_on(struct command_context *ctx) {
	wod_enable = 1;
	return CMD_ERROR_NONE;
}

int cmd_wod_log_off(struct command_context *ctx) {
	wod_enable = 0;
	return CMD_ERROR_NONE;
}

int cmd_wod_set_wodnum(struct command_context *ctx) {
	char * args = command_args(ctx);
	uint32_t wodnum;
	char filename[15] = "";
	if (sscanf(args, "%x", &wodnum) != 1)
		return CMD_ERROR_SYNTAX;
	if (wodnum > 0xFF) {
		printf("invaild input!\n");
		return CMD_ERROR_SYNTAX;
	}
	previous = wod_enable;
	wod_enable = 0;
	if (set_wodnum_cmd(wodnum) != 1) {
		wod_enable = previous;
		return CMD_ERROR_FAIL;
	}
	snprintf(filename, sizeof(filename), "/sd/%02X.wod", wodnum);
	init_wodlog(filename);
	wod_enable = previous;
	return CMD_ERROR_NONE;
}

//beacon stuff
int cmd_wod_beacon_off(struct command_context *ctx) {
	current_config.beacon_enable = 0;
	return CMD_ERROR_NONE;
}

int cmd_wod_beacon_on(struct command_context *ctx) {
	current_config.beacon_enable = 1;
	return CMD_ERROR_NONE;
}

int cmd_wod_set_beacon_intervel(struct command_context *ctx) {
	char * args = command_args(ctx);
	uint32_t interval;
	if (sscanf(args, "%u", &interval) != 1)
		return CMD_ERROR_SYNTAX;
	current_config.interval = (interval < 10) ? 10 : interval;
	printf("setting interval to:%u\n", (interval < 10) ? 10 : interval);
	return CMD_ERROR_NONE;
}

int cmd_wod_set_beacon_postpone(struct command_context *ctx) {
	char * args = command_args(ctx);
	uint32_t postpone;
	if (sscanf(args, "%u", &postpone) != 1)
		return CMD_ERROR_SYNTAX;
	current_config.postpone = (postpone < 10) ? 10 : postpone;
	printf("setting postpone to:%u\n", (postpone < 10) ? 10 : postpone);
	return CMD_ERROR_NONE;
}

int cmd_wod_set_beacon_callsign(struct command_context *ctx) {
	char * args = ctx->argv[1];
	if (!args)
		return CMD_ERROR_SYNTAX;
	if (strlen(args) < 6)
		return CMD_ERROR_SYNTAX;
	printf("setting %s\n", args);
	memcpy(&current_config.callsign, args, 6);
	printf("setting callsign to:%.6s\n", current_config.callsign);
	return CMD_ERROR_NONE;
}

int cmd_wod_get_beacon_config(struct command_context *ctx) {
	cmd_wod_print_beacon_config(&current_config);
	return CMD_ERROR_NONE;
}
int cmd_wod_beacon_refresh(struct command_context *ctx) {
	log_last_gnd_contact();
	return CMD_ERROR_NONE;
}

command_t __sub_command wodlog_subcommands[] = {
	{
		.name = "rmall",
		.help = "Remove all log Files and reset",
		.handler = cmd_wod_log_rmall,
	},
	{
		.name = "reset",
		.help = "start log numbers from 0",
		.handler = cmd_wod_log_reset,
	},
	{
		.name = "hk",
		.help = "wod hk",
		.handler = cmd_wod_log_hk,
	},
	{
		.name = "on",
		.help = "enable wod looger",
		.handler = cmd_wod_log_on,
	},
	{
		.name = "off",
		.help = "disable wod looger",
		.handler = cmd_wod_log_off,
	},
	{
		.name = "setwodnum",
		.help = "disable wod looger",
		.handler = cmd_wod_set_wodnum,
	}
};

command_t __sub_command wodbeacon_subcommands[] = {
	{
		.name = "on",
		.help = "Turn on the beacon",
		.handler = cmd_wod_beacon_on,
	},
	{
		.name = "off",
		.help = "Turn off the beacon",
		.handler = cmd_wod_beacon_off,
	},
	{
		.name = "interval",
		.help = "set beacon interval",
		.handler = cmd_wod_set_beacon_intervel,
	},
	{
		.name = "postpone",
		.help = "set beacon postpone",
		.handler = cmd_wod_set_beacon_postpone,
	},
	{
		.name = "callsign",
		.help = "set beacon callsign",
		.handler = cmd_wod_set_beacon_callsign,
	},
	{
		.name = "hk",
		.help = "get the beacon configuration",
		.handler = cmd_wod_get_beacon_config,
	},
	{
		.name = "refresh",
		.help = "get the beacon configuration",
		.handler = cmd_wod_beacon_refresh,
	}
};

command_t __root_command wodlog_commands[] = {
	{
		.name = "wod",
		.help = "wod log subsystem",
		.chain = INIT_CHAIN(wodlog_subcommands),
	},
	{
		.name = "beacon",
		.help = "wod beacon subsystem",
		.chain = INIT_CHAIN(wodbeacon_subcommands),
	}
};

void cmd_wod_log_setup() {
	command_register(wodlog_commands);
}