#ifndef _SAT_TO_GND_
#define _SAT_TO_GND_

#include <stdio.h>
#include <sys/types.h>
#include <time.h>

#define getRealTime() {time(NULL)}


#define log_debug(event, format, ...) { do { printf("%s: ", event); printf(format, ##__VA_ARGS__); printf("\r\n"); } while(0); }
#define log_info(event, format, ...) { do { printf("%s: ", event); printf(format, ##__VA_ARGS__); printf("\r\n"); } while(0); }
#define log_warning(event, format, ...) { do { printf("%s: ", event); printf(format, ##__VA_ARGS__); printf("\r\n"); } while(0); }
#define log_error(event, format, ...) { do { printf("%s: ", event); printf(format, ##__VA_ARGS__); printf("\r\n"); } while(0); }


#endif