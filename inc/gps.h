/*
 * gps.h
 *
 *  Created on: Dec 8, 2021
 *      Author: kystlen
 */

#ifndef GPS_H_
#define GPS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define GPRMC_LEN 100
#define TZ_OFFSET -5

void init_usart1(void);
void get_GPS_data(void);
void fill_utc_time(char* GPRMC, struct tm *utc_time);
void parse_GPRMC(char* GPRMC, int offset);
int is_sentence_format_GPRMC(char* format);

#endif /* GPS_H_ */
