/**
 * Whole Orbit Data & beacon
 *
 * @author Luyang
 * 
 */

#ifndef _WOD_H
#define _WOD_H

#include <stdio.h>
#include <stdint.h>

/** Shared data-structures 
subscript 'char' means represented in char. other means the real values.
minute_passed: a file on SD card with a list of each file with started timestamp. +1  
    = offset by one miniute. 
flags: MSB [  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  ] LSB
           [ runlevels |rail1|rail2|rail3|rail4|rail5|rail6]
            runlevels: 0 critical 
                       1 safe
                       2 Normal
                       3 full
                       rails: 1 for on, 0 for off.
batt_voltage: battary voltage ranging from 4420mV to 8500mV with 16mV resolution.
    Voltage range taken from EPS tests and datasheet.
    conversion: V_mv = V_char * 16 + 4420; 0<=V_char<=255
    V_char = (V_mv - 4420) / 16;
batt_current: battary currents, batt_current = I_in - I_sys. small-endian (ARM and x86s)
rail3_current: 3.3v rails current with 21.48mV resolution, from 0mA to 5500mA(5.5A).
    conversion: I_char = floor(I_ma * (float)(255/5500));
    I_ma = I_char * (float)(5500/255);
rail5_current: 5v rails current with 17.58mV resolution, from 0mA to 4500mA(4.5A).
    conversion: I_char = floor(I_ma * (float)(255/4500));
    I_ma = I_char * (float)(4500/255);
com_temp: reading of temprature sensor A on NanoCom ranging from -40 to 125 deg. C. with 1 deg. C resolution
eps_temp: averaged reading of 4 temp. sensors on NanoPower ranging from -40 to 125 deg. C. with 1 deg. C resolution
bat_temp: averaged reading of 2 temp. sensors on battery ranging from -40 to 125 deg. C. with 1 deg. C resolution
In above data fields, both 0 and 255 are most likely to be outliers. Ignore if possible.
*/

typedef struct __attribute__((packed)) {
    uint8_t minute_passed;
    uint8_t flags;
    uint8_t batt_voltage;
    int16_t batt_current;
    uint8_t rail3_current;
    uint8_t rail5_current;
    int8_t com_temp;
    int8_t eps_temp;
    int8_t bat_temp;
} wod_data_t;


extern uint8_t wod_enable;
extern nanocom_hk_t wod_com_hk;
extern eps_hk_t wod_eps_hk;
extern wod_data_t current_wod_data;

int wod_status(int value);
void update_filename(int number);
int set_wodnum(FILE * wodnum, int32_t number);
int get_wodnum(FILE * wodnum, int32_t * number);
int set_wodnum_cmd(int32_t number);
int get_wodnum_cmd(int32_t * number);
int read_log_time(const char * path, int32_t * time_diff);
int init_wodlog(const char * path);
int wod_get(wod_data_t * data);
int wod_log();
int wod_number_increment();
int wod_log_now(FILE * log_file);
int wod_init();
void wodnum_reset(void);
void wod_reset(void);

#endif /* _WOD_H */
