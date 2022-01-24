/*
 * oled.h
 *
 *  Created on: Dec 8, 2021
 *      Author: kystlen
 */

#ifndef OLED_H_
#define OLED_H_

void init_spi2(void);
void init_dma1_c5(void);
void nano_wait(unsigned int n);
void spi2_cmd(unsigned int data);
void spi2_data(unsigned int data);
void spi_line2(const char* string);
void init2_oled(void);

#endif /* OLED_H_ */
