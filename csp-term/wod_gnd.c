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

#include <csp/csp.h>

#include <../nanopower/io/nanopower2.h>
#include <../a712/nanomind.h>
#include <../nanocom/nanocom.h>
#include <math.h>
#include "sat_to_gnd.h"
#include "wod.h"


static const uint32_t wod_magic = 0x54696D45; //  = TimE
static const char directory[25] = "/sd/";
static const uint8_t count = 1;

static int32_t current_number = 0;
static uint32_t current_entry = 0;
static char path[25] = "/sd/";
static int iteration = 0;

nanocom_hk_t wod_com_hk;
eps_hk_t wod_eps_hk;
uint8_t wod_enable = 1;
wod_data_t current_wod_data;


/*any none zero would turn it on*/
int wod_status(int value) {
	if (value)
		wod_enable = 1;
	return 0;
	wod_enable = 0;
	return 0;
}

/* Updates the file name of the current opening file. */
void update_filename(int number) {
	char filename[25] = "";
	memset(path, 0, sizeof(path));
	strcpy(path, directory);
	snprintf(filename, sizeof(path), "%02X.wod", number);
	strcat(path, filename);
}

/* Update the wodnum file with the given number. Make sure your file is already open.
returns the number of the successful writes. should be one in this case.*/
int set_wodnum(FILE * wodnum, int32_t number) {
	rewind(wodnum);
	int ret = fwrite(&number, sizeof(int32_t), 1, wodnum);
	fflush(wodnum);
	current_number = number;
	current_entry = 0;
	update_filename(number);
	return ret;
}

/* Write the number in wodnum to pointer number. failed with nagtive returned values.
 Make sure your file is already open.
 returns the number of the successful writes. should be one in this case.*/
int get_wodnum(FILE * wodnum, int32_t * number) {
	rewind(wodnum);
	int ret = fread(number, sizeof(int32_t), 1, wodnum);
	return ret;
}

/* Update the wodnum file with the given number. failed with nagtive returned values.
 It will open the file for you.
 returns the number of the successful writes. should be one in this case.*/
int set_wodnum_cmd(int32_t number) {
	if (number < 0 || number > 255)
		return -1;
	FILE * wodnum = NULL;
	wodnum = fopen("/sd/wodnum", "w");
	if (!wodnum) {
		log_info("wod", "cannot open to write wodnum");
		return -1;
	}
	int ret = set_wodnum(wodnum, number);
	fclose(wodnum);
	return ret;
}


/* Write the number in wodnum to pointer number. failed with nagtive returned values.
 It will open the file for you.
 returns the number of the successful writes. should be one in this case.*/
int get_wodnum_cmd(int32_t * number) {
	FILE * wodnum = NULL;
	wodnum = fopen("/sd/wodnum", "r");
	if (!wodnum) {
		log_info("wod", "cannot open to read wodnum");
		return -1;
	}
	int ret = get_wodnum(wodnum, number);
	fclose(wodnum);
	return ret;
}

/*
 open a wod log file and read its timestamp.
 @param path file path
 @param time_diff the time difference in seconds.
 returns 0 for no error, -1 if failed to open the file, -2 if magic not match.
*/
int read_log_time(const char * path, int32_t * time_diff) {
	FILE * logFile = NULL;
	uint32_t time_now = 0;
	uint32_t match_magic = 0;
	uint32_t time_in_log = 0;

	logFile = fopen(path, "r");
	if (!logFile) {
		log_info("wod", "cannot read %s", path);
		return -1;
	}
	fread(&match_magic, sizeof(uint32_t), 1, logFile);
	fread(&time_in_log, sizeof(uint32_t), 1, logFile);
	fclose(logFile);

	if (!(match_magic == wod_magic)) {
		log_info("wod", "magic not matched.\n");
		return -2;
	}
	time_now = (uint32_t)getRealTime();
	*time_diff = time_now - time_in_log;
	return 0;
}

/* Init the logfile with timestamp and first wod log.*/
int init_wodlog(const char * path) {

	FILE * logFile;
	uint32_t time_now;
	logFile = fopen(path, "r"); // check is this file already exist.
	if (!logFile) {
		// no. then create it.
		logFile = fopen(path, "w");
		if (!logFile) {
			log_info("wod", "cannot create wod logfiles.");
			return -1;
		}
		if (fwrite(&wod_magic, sizeof(uint32_t), 1, logFile) != 1) {
			log_info("wod", "cannot write to wod logfiles.");
			fclose(logFile);
			return -1;
		}
		time_now = (uint32_t)getRealTime();
		if (fwrite(&time_now, sizeof(uint32_t), 1, logFile) != 1) {
			log_info("wod", "cannot write to wod logfiles.");
			fclose(logFile);
			return -1;
		}
		if (wod_enable) {
			if (wod_log_now(logFile) != 1) {
				log_info("wod", "cannot write to wod logfiles.");
				fclose(logFile);
				return -1;
			}
		}
		fclose(logFile);
		current_entry = 0;
		return 1;
	} else {
		//yes. lets delete it and create it again.
		fclose(logFile); logFile = NULL;
		remove(path); // in fat.c the fopen with w flag would *not* erease the old file, but overwrite it from the begining.
		logFile = fopen(path, "w");
		if (!logFile) {
			log_info("wod", "cannot create wod logfiles.");
			return -1;
		}
		if (fwrite(&wod_magic, sizeof(uint32_t), 1, logFile) != 1) {
			log_info("wod", "cannot write to wod logfiles.");
			if (logFile) {fclose(logFile); logFile = NULL;}
			return -1;
		}
		time_now = (uint32_t)getRealTime();
		if (fwrite(&time_now, sizeof(uint32_t), 1, logFile) != 1) {
			log_info("wod", "cannot write to wod logfiles.");
			if (logFile) {fclose(logFile); logFile = NULL;}
			return -1;
		}
		if (wod_enable) {
			if (wod_log_now(logFile) != 1) {
				log_info("wod", "cannot write to wod logfiles.");
				if (logFile) {fclose(logFile); logFile = NULL;}
				return -1;
			}
		}
		fclose(logFile);
		current_entry = 0;
		return 1;
	}
}

//prevent direct truncate caused abnormal values return "Normal" readings.
static uint8_t uint16_to_uint8(uint16_t data) {
	data = data > 255 ? 255 : data;
	return (uint8_t)data;
}

static uint8_t int16_to_uint8(int16_t data) {
	data = data > 255 ? 255 : data;
	data = data < 0 ? 0 : data;
	return (uint8_t)data;
}
static int8_t int16_to_int8(int data) {
	data = data > 127 ? 127 : data;
	data = data < -127 ? -127 : data;
	return (int8_t)data;
}


/* write the hk data according to the wod format. */
//curout[6][5V1 5V2 5V3 3.3V1 3.3V2 3.3V3]
int wod_get(wod_data_t * data) {
	uint16_t batt_voltage = 0;
	uint16_t rail3_current = 0;
	uint16_t rail5_current = 0;
	int32_t batt_current = 0;
	uint8_t flags = 0;

	batt_voltage = ((wod_eps_hk.vbatt - 4420) / 16);

	batt_current = wod_eps_hk.cursun - wod_eps_hk.cursys;

	rail3_current = wod_eps_hk.curout[0] + wod_eps_hk.curout[1] + wod_eps_hk.curout[2];
	rail3_current = (uint16_t)floor((double)rail3_current * (255.0 / 5500.0));

	rail5_current = wod_eps_hk.curout[3] + wod_eps_hk.curout[4] + wod_eps_hk.curout[5];
	rail5_current = (uint16_t)floor((double)rail5_current * (255.0 / 4500.0));

	flags = ((wod_eps_hk.battmode - 1)) << 6 |
	        (wod_eps_hk.output[0] << 5) |
	        (wod_eps_hk.output[1] << 4) |
	        (wod_eps_hk.output[2] << 3) |
	        (wod_eps_hk.output[3] << 2) |
	        (wod_eps_hk.output[4] << 1) |
	        wod_eps_hk.output[5];

	data->minute_passed = (uint8_t)current_entry;
	data->flags = flags;
	data->batt_voltage = int16_to_uint8(batt_voltage);
	data->batt_current = int16_to_int8(batt_current);
	data->rail3_current = int16_to_uint8(rail3_current);
	data->rail5_current = int16_to_uint8(rail5_current);
	data->com_temp = int16_to_int8(wod_com_hk.temp_a);
	data->eps_temp = int16_to_int8((wod_eps_hk.temp[0] + wod_eps_hk.temp[1] + wod_eps_hk.temp[2] + wod_eps_hk.temp[3]) / 4);
	data->bat_temp = int16_to_int8((wod_eps_hk.temp[0] + wod_eps_hk.temp[1]) / 2);

	return 0;
}

/* return 1 if success, 0 if write failed, -1 faild to open.*/
int wod_log() {
	FILE * logFile = NULL;
	if (current_entry >= 256)
		iteration++;
	current_entry = current_entry % 256;
	//reset at iteration more than 4
	if (iteration > 3) {
		return wod_number_increment();
	}
	logFile = fopen(path, "a");
	if (!logFile) {
		log_info("wod", "cannot write to wod logfile.");
		return -1;
	}
	int ret = wod_log_now(logFile);
	if (logFile) fclose(logFile);
	com_get_hk(&wod_com_hk, &count , 5 , 1000);
	return ret;
}

int wod_number_increment(void) {
	FILE * wodnum = NULL;
	iteration = 0;
	current_entry = 0;
	current_number++;
	current_number = current_number % 256;
	update_filename(current_number);
	wodnum = fopen("/sd/wodnum", "w");
	if (!wodnum) {
		log_info("wod", "failed to open wodNum.");
		return -1;
	} else {
		set_wodnum(wodnum, current_number);
		if (wodnum) fclose(wodnum);
	}
	return init_wodlog(path);
}

/* Log the current wod log to the given file. Make sure your file is already open.
make sure you file's position is at the end of the file as this function won't check it. */
int wod_log_now(FILE * log_file) {

	wod_get(&current_wod_data);

	current_entry++;
	int ret = fwrite(&current_wod_data, sizeof(wod_data_t), 1, log_file);
	fflush(log_file);
	return ret;
}

int wod_init(void) {
	FILE * wodnum = NULL;
	wodnum = fopen("/sd/wodnum", "r");

	if (!wodnum) {
		log_info("wod", "wodNum not exist, creating one.");
		wodnum = fopen("/sd/wodnum", "w");
		if (!wodnum) {
			log_info("wod", "failed to create wodNum.");
			return 0;
			// PANIC!
		}
		current_number = 0;
		current_entry = 0;
		if (set_wodnum(wodnum, current_number) != 1) {
			log_info("wod", "failed to write to wodNum.");
			fclose(wodnum);
			return 0;
		}
		update_filename(current_number);
		init_wodlog(path);
		fclose(wodnum);
		return 0;
	} else {
		int32_t read = 0;
		if (get_wodnum(wodnum, &read) != 1) {
			log_info("wod", "can't read the wodnum file.");
			if (wodnum) {fclose(wodnum); wodnum = NULL;}
			return 0;
		}
		if (wodnum) {fclose(wodnum); wodnum = NULL;}
		wodnum = fopen("/sd/wodnum", "w");
		if (!wodnum) {
			log_info("wod", "failed to create wodNum.");
			return 0;
		}

		if (read < 0 || read > 0xFF) { // FF is the largest number for the logs.
			current_number = 0;
			current_entry = 0;
		} else {
			int32_t time_diff = 0;
			update_filename(read);
			int ret = read_log_time(path, &time_diff);

			switch (ret) {
			case -2: log_info("wod", "cannot read %s", path); break;
			case -1: log_info("wod", "%s magic mismatch", path);
			default: break;
			}
			//if longer than 15 minutes go for next, else just overwrite it.
			if (time_diff > 900)
				current_number = ++read;
			else
				current_number = read;
			// This number makes sence, use the incremented number.
			current_entry = 0;
		}
		//update the number.
		if (set_wodnum(wodnum, current_number) != 1)
			log_info("wod", "failed to write to wodNum.");
		if (wodnum) {fclose(wodnum); wodnum = NULL;}

		update_filename(current_number);
		int inner_loop = 0;
		//try write 5 times;
		for (inner_loop = 0; inner_loop < 5; inner_loop++) {
			if (init_wodlog(path) == 1)
				break;
			current_number = current_number + 1;
			update_filename(current_number);
		}
		return 0;
	}
	return 0;
}

/* Reset the current number and entry to zero.*/
void wodnum_reset(void) {
//THIS METHOD DOESN'T GURANTEE ITS RESULT.
//if the files were opened, close them now.
	FILE * wodnum;

	log_info("wod", "resetting wod number to zero.");
	current_number = 0;
	current_entry = 0;
	//open the wodnum for number update;
	wodnum = fopen("/sd/wodnum", "w");
	if (!wodnum) {
		log_info("wod", "cannot write to wodnum while resetting.");
	} else {
		set_wodnum(wodnum, current_number);
	}
	//close the wodnum
	if (wodnum) {fclose(wodnum); wodnum = NULL;}
}

/* Full reset of the wod logging system -- reset the wodnum and
delete all the files then start it again.
*/
void wod_reset(void) {
	wodnum_reset();
	FILE * logFile = NULL;
	uint16_t lognumber = 0;
	char filename[25] = "";
	for (lognumber = 0; lognumber < 0x100; lognumber++) {
		memset(filename, 0, 25);
		snprintf(filename, 25, "/sd/%02X.wod", lognumber);
		logFile = fopen(filename, "r");
		if (logFile) {
			fclose(logFile); logFile = NULL;
			int ret = remove(filename);
			if (!ret) {
				log_info("wod", "cannot remove %s.", filename);
			}
		}
	}
	wod_init();
}
