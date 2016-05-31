/*
 * testtools.c
 *
 *  Created on: 30/01/2016
 *      Author: Luyang Li
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <testtools.h>
#include <sys/stat.h>
#include <../a712/nanomind.h>
#include <command/command.h>
#include <util/timestamp.h>
#include <inttypes.h>
#include <time.h>
#include <util/log.h>
#include <csp/drivers/usart.h>
#include <csp/csp.h>
#include <csp/csp_endian.h>

#include <../lib/libftp/include/ftp/ftp_client.h>
#include <../lib/libftp/include/ftp/ftp_types.h>

#include <lzo/lzoutils.h>

#ifdef __linux__
#include <linux/limits.h> //[PATH_MAX]
#else
#ifndef PATH_MAX
#define PATH_MAX 2048
#endif
#endif


#define CONTROL(X)  ((X) - '@')
#define REMOTE_MESSAGE_SIZE 50


static unsigned int ftp_backend = 2; // vfs backend
static unsigned int ftp_chunk_size = 75;

static uint32_t ftp_size;

static char parse_start = '[';
static char parse_end = ']';
static char keys[] = "#[";



int tnc_timesync(int32_t timestamp) {

	char command[28] = "predict time ";
	char time_str[15];

	sprintf(time_str, "%d\r\n", timestamp);
	strcat(command, time_str);
	//printf("%s\n",command);
	usart_putstr(command, sizeof(command));

	return 0;
}

int get_current_time(void) {

	time_t current_time;
	int32_t sec;

	current_time = time(NULL);

	if (current_time == ((time_t) - 1))
	{
		printf("ERROR getting time.\r\n");
		return -1;
	}

	sec = (int32_t) current_time;

	printf("Current time is: %d\n%s\r\n", sec, ctime(&current_time));

	return sec;
}


int send_test_packet(void) {

	csp_packet_t * packet = csp_buffer_get(100);
	char * str = "Hello World\r\n";
	sprintf((char *) packet->data, str);
	packet->length = strlen(str) + 1;
	if (csp_sendto(CSP_PRIO_NORM, NODE_OBC, OBC_PORT_CADCS_HK, 0, CSP_O_CRC32 | CSP_O_HMAC | CSP_O_XTEA, packet, 0) < 0) {
		//printf("Error sending packet\r\n");
		csp_buffer_free(packet);
	}

	return 0;

}

/* Commands Below */

int cmd_testtools_obc_timesync(struct command_context *ctx) {

	int32_t sec = get_current_time();

	if (sec < 0)
		return CMD_ERROR_FAIL;

	printf("Setting OBC time to: %d\r\n", sec);
	timestamp_t t = {sec, 0};
	obc_timesync(&t, 6000);

	return CMD_ERROR_NONE;
}

int cmd_testtools_tnc_timesync(struct command_context *ctx ) {

	int32_t sec = get_current_time();

	if (sec < 0)
		return CMD_ERROR_FAIL;

	printf("Setting TNC time to: %d\r\n", sec);
	tnc_timesync(sec);

	return CMD_ERROR_NONE;
}


int cmd_testtools_packet(struct command_context *ctx) {

	if (ctx->argc != 2)
		return CMD_ERROR_SYNTAX;

	int8_t dest = atoi(ctx->argv[1]);
	int8_t d_port = OBC_PORT_CUSTOM_REMOTE_SHELL;
	char *message; // this goes to the space
	char *command; // got this from input
	command = calloc(REMOTE_MESSAGE_SIZE, sizeof(char));
	char c;
	int quit = 0, execute = 0;
	unsigned int cursor = 0;

	printf("Type the command here, hit enter to send, ctrl+x to cancel:\r\n");

	/* Wait for ^q to quit. */
	while (quit == 0) {
		/* Get character */
		c = getchar();
		switch (c) {
		/* CTRL + X */
		case 0x18:
			quit = 1;
			break;
		/* Backspace */
		case CONTROL('H'):
		case 0x7f:
			if (cursor > 0) {
				putchar('\b');
				putchar(' ');
				putchar('\b');
				cursor--;
			}
			break;
		/* RETURN */
		case '\r':
			execute = 1;
			quit = 1;
			break;
		default:
			putchar(c);
			if (command == NULL) {
				command = calloc(REMOTE_MESSAGE_SIZE, 1);
			}

			if ((command != NULL) && (cursor < REMOTE_MESSAGE_SIZE))
				command[cursor++] = c;
			break;
		}
	}
	if (execute) {
		printf("\n----------\nPress enter to send this command, or any key to abort:\r\n");
		printf("%s", command);
		c = getchar();
		if (c != '\r') {
			return CMD_ERROR_INVALID;
		}
	}
	putchar('\n');
	int overhead = 3;
	message = (char*)calloc(REMOTE_MESSAGE_SIZE + overhead, sizeof(char));
	//reply = (char * )calloc(REMOTE_MESSAGE_SIZE + overhead, sizeof(char));
	//the overhead of <,> and \0
	if (!message) {
		printf("calloc() failed!\n");
		return CMD_ERROR_FAIL;
	}
	message[0] = '<';
	//message[0] = '<'; message[1] = '|';

	for (uint8_t i = 0; i < cursor; i++) {
		if (command[i] == ' ') {
			message[i + 1] = '|';
		} else {
			message[i + 1] = command[i];
		}
	}
	/*
	message[cursor + 2] = '|'; message[cursor + 3] = '>';
	message[cursor + 4] = '\0';
	*/
	message[cursor + 1] = '>'; message[cursor + 2] = '\0'; message[cursor + 3] = '\0';
	printf("sending a message to host %d:%d.\"%s\".\r\n", dest, d_port, message);

	//int buffer_result = sprintf(buffer,"%s", message);
	//csp_packet_t * reply_packet;
	remoteShell_packet_t sending;
	memcpy(sending.command_text, message, 50);
	sending.returned = csp_hton16(-42); // yeah.


	remoteShell_packet_t reply;
	reply.returned = csp_hton16(-43);
	reply.command_text[0] = '\0';

	printf("packet size: %lu.\nBuffer content: %s.\n", sizeof(remoteShell_packet_t), sending.command_text);
	int packet_status = csp_transaction(CSP_PRIO_NORM, dest, d_port, 5000, &sending , sizeof(remoteShell_packet_t), &reply, sizeof(remoteShell_packet_t));
	reply.returned = csp_ntoh16(reply.returned);

	printf("status = %d, original message:%s\nReply = %d\n", packet_status, reply.command_text, reply.returned);


	free(message);
	free(command);
	return reply.returned;

}

char *my_basename_1(char *path) {
	char *base = strrchr(path, '/');
	return base ? base + 1 : path;
}

int decompress_file_to_buffer(char * path, void * dst, uint32_t dstlen) {

	int ret = -1;
	uint32_t header_len, comp_len, decomp_len;

	void * dst_temp;

	dst_temp = calloc(dstlen, 1);

	/* Validate buffer */
	if (dst_temp == NULL || dst == NULL || dstlen < 1) {
		printf("Buffer error");
		goto err_open;
	}

	/* Read decompressed size */
	if (lzo_header(path, &header_len, &decomp_len, &comp_len) != 0) {
		log_error("Failed to read header");
		goto err_open;
	}

	/* Open file */
	FILE * fp = fopen(path, "r");
	if (!fp) {
		log_error("Cannot open %s", path);
		goto err_open;
	}

	struct stat statbuf;
	stat(path, &statbuf);

	/* Read file into buffer */
	if (fread(dst_temp, 1, statbuf.st_size, fp) != (size_t) statbuf.st_size) {
		log_error("Failed to read complete file");
		goto err_read;
	}

	/* Decompress */
	if (lzo_decompress_buffer(dst_temp, statbuf.st_size, dst, dstlen, NULL) != 0) {
		log_error("Failed to decompress buffer");
		goto err_read;
	}

	ret = decomp_len;

err_read:
	fclose(fp);
err_open:
	free(dst_temp);
	return ret;

}

int cmd_testtools_ls2sd(struct command_context *ctx) {

	remoteShell_packet_t * message_send;
	remoteShell_packet_t * message_reply;
	char remote_path[REMOTE_MESSAGE_SIZE];

	message_send = (remoteShell_packet_t *) calloc(sizeof(remoteShell_packet_t), sizeof(char));
	message_reply = (remoteShell_packet_t *) calloc(sizeof(remoteShell_packet_t), sizeof(char));
	unsigned int now;

	char * out_buffer = calloc(10240, 1);

	if (!(message_send && message_reply)) {
		printf("Houston: calloc error.\n");
		goto err;
	}

	if (!(out_buffer)) {
		printf("Houston: malloc error.\n");
		goto err;
	}

	memset(remote_path, 0, REMOTE_MESSAGE_SIZE);

	int start = 0; // 0 means from start.
	int file_count = 0;
	int lastfilecount = 0;

	do {
		message_send->returned = csp_hton16(start);
		memcpy(message_reply, message_send, sizeof(remoteShell_packet_t));

		sprintf(remote_path, "/sd/");
		memcpy(message_send->command_text, remote_path, REMOTE_MESSAGE_SIZE);

		int packet_status = csp_transaction(CSP_PRIO_NORM, NODE_OBC, OBC_PORT_FTP_LIST, 6000,
		                                    message_send , sizeof(remoteShell_packet_t), message_reply, sizeof(remoteShell_packet_t));

		message_reply->returned = csp_ntoh16(message_reply->returned);
		file_count = message_reply->returned;

		printf("status = %d, ", packet_status);

		if (packet_status == sizeof(remoteShell_packet_t)) {
			now = (unsigned)time(NULL);
			if (message_reply->returned < 0) {
				printf("returned value = %d, ls failed. Re-formatting may required.\n", message_reply->returned);
				goto err;
			}
			memcpy(remote_path, message_reply->command_text, REMOTE_MESSAGE_SIZE);
			printf("%d files, path in sd is %s\n", message_reply->returned, remote_path);
			printf("successed listing files.\n");
		} else {
			printf("failed!\nNETWORK ERROR\n");
			goto err;
		}

		/* Then download list file. */

		char local_path[PATH_MAX]; // no need to malloc since it is part of remote_path
		memset(local_path, 0, PATH_MAX);
		getcwd(local_path, sizeof(local_path));
		strcat(local_path, "/");
		strcat(local_path, my_basename_1(remote_path));

		printf("local path is: %s\n", local_path);

		int download_status = ftp_download(NODE_OBC, OBC_PORT_FTP, local_path, ftp_backend, ftp_chunk_size, 0, 0, remote_path, &ftp_size);

		if (download_status != 0) {
			ftp_done(0);
			goto err;
		}

		if (ftp_status_reply() != 0) {
			ftp_done(0);
			goto err;
		}
		if (ftp_crc() != 0) {
			ftp_done(0);
			goto err;
		}

		ftp_done(1);

		usleep(100 * 1000); // 100 ms
		char remove_command[50];
		memset(remove_command, 0, 50);
		sprintf(remove_command, "<rm|%s>", remote_path);
		int remove_status = -1;
		send_remote_shell_command(remove_command, NULL, &remove_status);
		printf("remove status: %d\n", remove_status);


		int decompress_result = decompress_file_to_buffer(local_path, out_buffer, (uint32_t) 10240);

		if (decompress_result < 0) {
			printf("Decompress of %s failed. Maybe try to decompress this file in shell?\n", local_path);
			goto err;
		}

		char * pc_start;
		char * pc_end;
		int found_timestamp = 0;


		pc_start = strchr(out_buffer, parse_start);
		pc_end = strchr(out_buffer, parse_end);

		if (!(pc_start && pc_end))
			printf("timestamp not found. ugh.\n");
		else {
			found_timestamp = 1;
			pc_start += sizeof(char);
		}

		if (found_timestamp) {
			char list_time[32];
			memset(&list_time, 0, sizeof(list_time));
			memcpy(&list_time, pc_start, (pc_end - pc_start));

			printf("current time:%u, list time:%s\nTime difference: %d seconds.\n",
			       now , list_time, (now - (unsigned)atoi(list_time)));

			printf("%s\n", pc_end + sizeof(char));
		} else {
			printf("current time:%u, list time: unavailable\n", (unsigned)time(NULL));
			printf("%s\n", out_buffer);
		}

		char * last_legal_line = NULL;
		char * second_last_line = NULL;
		last_legal_line = strrchr(out_buffer, '\n');
		if (!last_legal_line) {
			printf("cannot find new line what happend?\n");
			break;
		}
		int i;
		for (i = 1; i < 32; ++i) {
			if (*(last_legal_line - i) == '\n' ) {
				second_last_line = last_legal_line - i;
				break;
			}
		}

		if (!second_last_line) {
			printf("cannot find new line what happend? quitting..\n");
			goto err;
		}

		char line[32] = {0};

		memcpy(line, second_last_line + 1, last_legal_line - second_last_line);
		sscanf (line, "%d\t", &lastfilecount);
		if (lastfilecount == file_count)
			printf("Finished listing.\n");
		else
			printf("downloading next listing file...\n");
		start = lastfilecount;
		memset(out_buffer, 0, 10240);

	}
	while (lastfilecount < file_count);

	/* cleaning starts. */
	if (out_buffer)
		free(out_buffer);
	if (message_send)
		free(message_send);
	if (message_reply)
		free(message_reply);

	return 0;

	/* Exception handlers */
err:
	if (out_buffer)
		free(out_buffer);
	if (message_send)
		free(message_send);
	if (message_reply)
		free(message_reply);
	return CMD_ERROR_FAIL;
}

/*
 * Send a remote shell command to obc.
 * @param command: the command to be sent.
 * @param response: the remote response from obc, NULL for no response needed.
 * @param returned: the returned value of the command.
 * Function returns the csp_transaction() value, which is normally the size of the payload.
 */
int send_remote_shell_command(char * command, char * response, int * returned) {
	int timeout = 5000;
	int8_t dest = 1;
	int8_t d_port = OBC_PORT_CUSTOM_REMOTE_SHELL;
	remoteShell_packet_t command_out;
	memset(&command_out, 0, sizeof(remoteShell_packet_t));
	printf("sending a message to host %d:%d.\"%s\".\r\n", dest, d_port, command);
	int reply_size = sizeof(remoteShell_packet_t);
	memcpy(command_out.command_text, command, strlen(command));
	command_out.returned = csp_hton16(-42); // yeah.

	remoteShell_packet_t * reply;
	if (!response) {
		timeout = 100;
		reply = NULL;
		reply_size = 0;
	} else {
		reply = calloc(sizeof(remoteShell_packet_t), 1);
	}

	printf("packet size: %lu.\nBuffer content: %s.\n", sizeof(remoteShell_packet_t), command_out.command_text);
	int packet_status = csp_transaction(CSP_PRIO_NORM, dest, d_port, timeout, &command_out , sizeof(remoteShell_packet_t), reply, reply_size);

	if (response) {
		reply->returned = csp_ntoh16(reply->returned);
		*returned = reply->returned;
		response = reply->command_text;
		printf("status = %d, original message:%s\nFunction returned = %d\n", packet_status, reply->command_text, reply->returned);
	} else {
		*returned = 0;
		printf("status = %d, NULL passed, not waiting for reply.\n", packet_status);
	}

	if (reply)
		free(reply);
	return packet_status;
}


/*
 * Parse a batch shell command in a batch file to a remote shell command defined in remoteShell.h
 * @param command: the command to be sent to the remote shell deamon.
 * return LINE_PARSE_SATELLITE if parsed successfully.
 * return LINE_PARSE_ERROR for any errors.
 */
int parse_remote_command(char * command) {
	char * ptr = strchr(command, parse_start);
	if (!ptr)
		return LINE_PARSE_ERROR;
	*ptr = '<';
	ptr = strchr(command, parse_end);
	if (!ptr || !(ptr + 1))
		return LINE_PARSE_ERROR;
	*ptr = '>'; *(ptr + 1) = '\0';
	ptr = strchr(command, ' ');

	while (ptr != NULL) {
		*ptr = '|';
		ptr = strchr(ptr + 1, ' ');
	}
	return LINE_PARSE_SATELLITE;
}

/*
 * Parse a batch shell command in a batch file to a normal shell command.
 * @param command: the command to be sent to the console
 * return LINE_PARSE_GROUND if parsed successfully.
 * return LINE_PARSE_ERROR for any errors.
 */
int parse_local_command(char * command) {
	char * ptr = strchr(command, parse_start);
	if (!ptr)
		return LINE_PARSE_ERROR;
	memmove(command, ptr + 1, strlen(command));
	ptr = strchr(command, parse_end);
	if (!ptr)
		return LINE_PARSE_ERROR;
	*ptr = '\0';
	return LINE_PARSE_GROUND;
}

/*
 * Parse a line in a batch file and do the task accordingly.
 * @param command: the command to be sent to the console
 * return LINE_PARSE_GROUND for ground station messages.
 * return LINE_PARSE_SATELLITE for ground station messages.
 * return LINE_PARSE_ERROR for any errors.
 */
int parse_line(char * command) {
	int pos = strcspn(command, keys);
	if (pos == strlen(command)) {
		return LINE_PARSE_EMPTY;
	}
	switch (*(command + pos)) {
	case '[':
		if (!(command + pos - 1))
			return LINE_PARSE_SYNTAX;
		switch (*(command + pos - 1)) {
		case 'G':
			memmove(command, command + pos, strlen(command));
			return parse_local_command(command);
			break;
		case 'S':
			memmove(command, command + pos, strlen(command));
			return parse_remote_command(command);
			break;
		default:
			break;
		}
		break;
	case '#':
		return LINE_PARSE_COMMENT;
		break;
	default:
		return LINE_PARSE_SYNTAX;
		break;
	}
	return LINE_PARSE_SYNTAX;
}

/*
 * Parse a line in a batch file and do the task accordingly.
 * @param command: the command to be sent to the console
 * return LINE_PARSE_GROUND for ground station messages.
 * return LINE_PARSE_SATELLITE for ground station messages.
 * return LINE_PARSE_ERROR for any errors.
 */
int parse_file(const char * batch_path) {

	char line[50];
	memset(line, 0, sizeof(line));
	char * pc_start;
	char * pc_end;
	int pos = 0;
	int replyed;
	struct stat statbuf;
	stat(batch_path, &statbuf);

	char * pc_batch_buf = (char * ) calloc(10240, 1);
	char * current_line = (char * ) calloc(128, 1);
	char * response = (char * ) calloc(128, 1);
	pc_start = pc_batch_buf;
	pc_end = pc_batch_buf;

	int i = 1;

	// Open file
	FILE * fp = fopen(batch_path, "r");
	if (!fp) {
		printf("Cannot open %s\n", batch_path);
		return -1;
	}


	if (((size_t) statbuf.st_size) > 10240)
	{
		printf("File %s too big.\n", batch_path);
		goto err;
	}


	if (!pc_batch_buf) {
		printf("malloc error!\n");
		goto err;
	}

	// Read file into buffer
	if (fread(pc_batch_buf, 1, statbuf.st_size, fp) != (size_t) statbuf.st_size) {
		printf("Failed to read complete file");
		goto err;
	}

	fclose(fp);
	fp = NULL;

	/* Finished loading to buffer. Main program starts from here. */


	while (strchr(pc_end + 1, '\n')) {
		pc_start = pc_batch_buf + pos;  //
		pc_end = strchr(pc_start, '\n');
		if (!(pc_end))
			break;
		memset(current_line, 0, 128);
		memset(response, 0, 128);
		memcpy(current_line, pc_start, (pc_end - pc_start));
		int ret = parse_line(current_line);
		switch (ret) {
		case LINE_PARSE_GROUND:
			printf("[%03d] \"%s\" to ground station.\n", i , current_line);
			command_run(current_line);
			usleep(10000);
			break;
		case LINE_PARSE_SATELLITE:
			printf("[%03d] \"%s\" to satellite.\n", i , current_line);
			send_remote_shell_command(current_line, response, &replyed);
			printf("%s, replyed: %d\n", response, replyed);
			usleep(10000);
			break;
		case LINE_PARSE_ERROR:
			printf("Error when processing line %d: %s\n", i, current_line);
			break;
		case LINE_PARSE_SYNTAX:
			printf("Please check the syntax of %d: %s\n", i, current_line);
			break;
		case LINE_PARSE_COMMENT:
		case LINE_PARSE_EMPTY:
		default:
			break;
		}
		pos = (pc_end - pc_batch_buf) + 1;
		i++;
	}

	if (current_line)
		free(current_line);
	if (response)
		free(response);
	if (pc_batch_buf)
		free(pc_batch_buf);
	return i;

err:
	if (current_line)
		free(current_line);
	if (response)
		free(response);
	if (pc_batch_buf)
		free(pc_batch_buf);
	if (fp)
		fclose(fp);

	return -1;
}

int cmd_testtools_batch_file(struct command_context *ctx ) {

	if (ctx->argc != 2)
		return CMD_ERROR_SYNTAX;

	int ret = parse_file(ctx->argv[1]);

	printf("Parsed %d lines in %s.\n", ret, ctx->argv[1]);

	return CMD_ERROR_NONE;
}


command_t __root_command testtools_commands[] = {
	{
		.name = "remote",
		.help = "send a test packet to obc.",
		.usage = "<DEST>",
		.handler = cmd_testtools_packet,
	}, {
		.name = "tncsync",
		.help = "Sync tnc clock with current system time.",
		.handler = cmd_testtools_tnc_timesync,
	}, {
		.name = "obcsync",
		.help = "Sync obc clock with current system time.",
		.handler = cmd_testtools_obc_timesync,
	}, {
		.name = "ls2sd",
		.help = "list files",
		.usage = "<DEST>",
		.handler = cmd_testtools_ls2sd,
	}, {
		.name = "batch_file",
		.help = "send a batch file to run.",
		.usage = "<FILENAME>",
		.handler = cmd_testtools_batch_file,
	}
};

void cmd_testtools_setup(void) {
	command_register(testtools_commands);
}
