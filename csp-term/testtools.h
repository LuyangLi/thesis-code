/*
 * UNSW QB50 toolchain
 * testtools.h
 *
 *  Created on: 30/01/2016
 *      Author: Luyang Li
 */

#ifndef TESTTOOLS_H_
#define TESTTOOLS_H_
#include <stdint.h>


#define OBC_PORT_CADCS_HK			35


#define LINE_PARSE_GROUND		2
#define LINE_PARSE_SATELLITE	1
#define LINE_PARSE_EMPTY		-1
#define LINE_PARSE_COMMENT		-2
#define LINE_PARSE_SYNTAX		-3
#define LINE_PARSE_ERROR		-10

//int cmd_testtools_timesync(struct command_context *ctx);
int tnc_timesync(int32_t timestamp);
int send_test_packet(void);
int get_current_time(void);
int send_remote_shell_command(char * command, char * respond, int * returned);
// int send_to_tnc(int8_t silent);

typedef struct __attribute__((packed)) {
	char command_text[50];			// Remote command, terminate with '\0'
	int16_t returned;		// returned value
} remoteShell_packet_t;

#endif /* TESTTOOLS_H_ */
