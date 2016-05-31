/*
 * WOD decoder
 *
 *  Created on: 01/05/2016
 *      Author: Luyang
 *  version 1.2
 * 
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <time.h>
#include <errno.h>
#include "wod.h"
#include "nanocom.h"
#include <time.h>

const uint32_t wod_magic = 0x54696D45; //  = TimE
const char directory[25] = "/sd/";

int32_t min, V_batt, uT_com, uT_eps, uT_bat;
int32_t T_com, T_eps, T_bat;
double I_3v, I_5v, I_in, I_out;
uint32_t magic_match;
uint8_t runlevel, flags;
uint8_t switches[6];
wod_data_t reader;
uint32_t timestamp;

int main(int argc, char const *argv[])
{
	if (argc < 2) {
		printf("UNSW QB50 WOD log decoder by Luyang Li.\n");
		printf("usage: decode <filename>\n");
		printf("will output a .csv file at the same directory.\n");
		return 0;
	}
	int i = 1;
	for (i = 1; i < argc; i++) {
		printf("processing file %s\n", argv[i]);
		FILE * pFile;
		pFile = fopen (argv[i], "r");

		if (pFile == NULL) {
			printf("%s\n", strerror(errno));
			return 0;
		}

		FILE * pOut;
		char filename[25];
		strcpy(filename, argv[i]);
		strcat(filename, ".csv");

		pOut = fopen (filename, "w");
		if (pOut == NULL) {
			printf("%s\n", strerror(errno));
			fclose(pFile);
			continue;
		}

		fread(&magic_match, sizeof(uint32_t), 1, pFile);
		//printf("in message:%x Original:%x\n", magic_match, wod_magic);
		if (magic_match == wod_magic)
			printf("magic matched.\n");
		else
			printf("magic not matched for file %s. Will try to decode anyway.\n", argv[i]);

		fread(&timestamp, sizeof(uint32_t), 1, pFile);
		time_t report_time = time(NULL);
		uint32_t timenow = (uint32_t)report_time;
		int32_t timediff = timenow - timestamp;
		time_t localtime_stamp = timestamp;
		char date[25] = "";
		snprintf(date,25,"%s",ctime(&localtime_stamp));

		printf("timestamp = %d, %s\ntimm_diff = %d days %d hours %d min %d sec %s.\n", timestamp, date,
		       ((timediff / 86400) > 0) ? (timediff / 86400) : -(timediff / 86400) , (timediff / 3600) % 24,
		       (timediff / 60) % 60, timediff % 60, (timediff > 0) ? "before" : "ahead from now");

		//printf("timestamp %u, time_now %u, time_diff %d\n", timestamp, timenow, timediff);

		fprintf(pOut, "OBC time: %s. Report generated: %s", date, ctime(&report_time));
		fprintf(pOut, "min,flags,runlevel,rail1,rail2,rail3,rail4,rail5,rail6,V_batt,I_in,I_out,I_3v,I_5v,T_com,T_eps,T_bat\n");
		fflush(pOut);
		while (fread(&reader, sizeof(wod_data_t), 1, pFile) == 1) {
			min = reader.minute_passed;
			flags = reader.flags;
			runlevel = ((flags & 0xc0) >> 6) + 1;
			switches[0] = ((flags & 0x20) >> 5);
			switches[1] = ((flags & 0x10) >> 4);
			switches[2] = ((flags & 0x08) >> 3);
			switches[3] = ((flags & 0x04) >> 2);
			switches[4] = ((flags & 0x02) >> 1);
			switches[5] = (flags & 0x01);
			V_batt = reader.batt_voltage;
			V_batt = V_batt * 16 + 4420;
			I_in = reader.current_in;
			I_in = I_in * (2700.0 / 255.0);
			I_out = reader.current_out;
			I_out = I_out * (4000.0 / 255.0);
			I_3v = reader.rail3_current;
			I_3v = I_3v * (5500.0 / 255.0);
			I_5v = reader.rail5_current;
			I_5v = I_5v * (4500.0 / 255.0);
			uT_com = reader.com_temp;
			T_com = uT_com;
			uT_eps = reader.eps_temp;
			T_eps = uT_eps;
			uT_bat = reader.bat_temp;
			T_bat = uT_bat;
			fprintf(pOut, "%d,0x%X,%u,%u,%u,%u,%u,%u,%u,%d,%f,%f,%f,%f,%d,%d,%d\n",
			        min, flags, runlevel, switches[0], switches[1], switches[2],
			        switches[3], switches[4], switches[5], V_batt, I_in, I_out, I_3v,
			        I_5v, T_com, T_eps, T_bat);
		}
		fclose(pOut);
	}
	return 0;
}
