/*
 * oled.c
 *
 *  Created on: Dec 8, 2021
 */

#include "stm32f0xx.h"
#include <math.h>
#include "lcd.h"

extern short oled_memory_map[34];
extern int is_alarm_on;
extern char* time_str;

//OLED DRIVER
//OLED SPI2 INIT
void init_spi2(void) {
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    GPIOB->MODER &= ~0xcf000000;
    GPIOB->MODER |= 0x8a000000; //no config for pb14, seems to be dummy pin for MISO
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
    SPI2->CR1 &= ~SPI_CR1_SPE;
    SPI2->CR1 |= SPI_CR1_BR;
    SPI2->CR2 = SPI_CR2_DS_0 | SPI_CR2_DS_3;
    SPI2->CR1 |= SPI_CR1_MSTR;
    SPI2->CR2 |= SPI_CR2_SSOE | SPI_CR2_NSSP | SPI_CR2_TXDMAEN;
    SPI2->CR1 |= SPI_CR1_SPE;
}

//DMA1_Channel5 INIT (SPI2_TX)
void init_dma1_c5(void) {
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    DMA1_Channel5->CCR &= ~DMA_CCR_EN;
    DMA1_Channel5->CPAR = (uint32_t) &(SPI2->DR);
    DMA1_Channel5->CMAR = (uint32_t) oled_memory_map;
    DMA1_Channel5->CNDTR = 68;
    DMA1_Channel5->CCR |= DMA_CCR_DIR;
    DMA1_Channel5->CCR |= DMA_CCR_MINC;
    DMA1_Channel5->CCR |= DMA_CCR_CIRC;
    DMA1_Channel5->CCR |= DMA_CCR_EN;
}

void nano_wait(unsigned int n) {
    asm(    "       mov r0,%0\n"
            "repeat: sub r0,#83\n"
            "       bgt repeat\n" : : "r"(n) : "r0", "cc");
}

//SPI INIT/WRITE FUNCTIONS
void spi2_cmd(unsigned int data) {
    while(!(SPI2->SR & SPI_SR_TXE));
    SPI2->DR = data;
}

void spi2_data(unsigned int data) {
    while(!(SPI2->SR & SPI_SR_TXE));
    //data |= 0x200;
    SPI2->DR = data | 0x200;
}

void spi_line2(const char* string) {
    spi2_cmd(0x02);
    char *char_ptr = string;
    while(*char_ptr != '\0'){
        spi2_data(*char_ptr);
        char_ptr++;
    }
}
void init2_oled(void) {
    nano_wait(1000000);
    spi2_cmd(0x38);
    spi2_cmd(0x08);
    spi2_cmd(0x01);
    nano_wait(2000000);
    spi2_cmd(0x06);
    spi2_cmd(0x02);
    spi2_cmd(0x0c);
}
