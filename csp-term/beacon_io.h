/**
 * beacon_io.h
 * Whole Orbit Data beacon
 *
 * @author Luyang
 *
 */

#ifndef _BEACON_IO
#define _BEACON_IO

#include <stdio.h>
#include <stdint.h>

typedef struct __attribute__((packed)) {
  uint8_t tasks;              //set = 1, other would be get, reply should be bitwise not;
  uint8_t beacon_enable;
  uint16_t interval;
  uint16_t postpone;
  char callsign[6];
} beacon_config_t;

typedef struct __attribute__((packed)) {
  int32_t timestamp;
  char callsign[6];
  uint8_t flags;
  uint8_t batt_voltage;
  int16_t batt_current;
  uint8_t rail3_current;
  uint8_t rail5_current;
  int8_t com_temp;
  int8_t eps_temp;
  int8_t bat_temp;
} beacon_packet_t;

extern int32_t last_gnd_contact;
extern beacon_config_t current_config;


void init_beacon_config(void); 
void set_beacon_config(void);
void get_beacon_config(void);
void set_beacon_enable(uint8_t status);
void cmd_wod_print_beacon_config(beacon_config_t * beacon_config);

#endif /* _BEACON_IO */
