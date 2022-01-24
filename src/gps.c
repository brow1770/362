#include "stm32f0xx.h"
#include "stm32f0xx_rtc.h"
#include "gps.h"

extern RTC_TimeTypeDef rtc_time;
extern RTC_DateTypeDef rtc_date;
extern char* time_str;

void init_usart1(void)
{
    RCC->AHBENR |= (RCC_AHBENR_GPIOAEN);
    GPIOA->MODER &= ~(GPIO_MODER_MODER9);
    GPIOA->MODER |= (GPIO_MODER_MODER9_1); //pin 9 for alternate function
    GPIOA->MODER &= ~(GPIO_MODER_MODER10);
    GPIOA->MODER |= (GPIO_MODER_MODER10_1); //pin 10 for alternate function

    GPIOA->AFR[1] |= 1<<(4); //route pin 9 to USART1_TX
    GPIOA->AFR[1] |= 1<<(4*2); //route pin 10 to USART1_RX

    RCC->APB2ENR |= RCC_APB2ENR_USART1EN; //enable RCC clock to USART1 peripheral
    USART1->CR1 &= ~(USART_CR1_UE); //disable, turn off UE bit
    USART1->CR1 &= ~(USART_CR1_M); //word size 8 bits
    USART1->CR2 &= ~(USART_CR2_STOP); //set 1 stop bit
    USART1->CR1 &= ~(USART_CR1_PCE); //no parity
    USART1->CR1 &= ~(USART_CR1_OVER8); //16x oversampling
    USART1->BRR = 0x1388; //9600 Baud, BRR = 0x1388
    USART1->CR1 |= (USART_CR1_TE | USART_CR1_RE); //enable transmitter and receiver
    USART1->CR1 |= USART_CR1_UE; //enable USART
    while(!((USART1->ISR & USART_ISR_REACK) && (USART1->ISR & USART_ISR_TEACK))); //wait for TE and RE to be acknowledged.
}

void get_GPS_data(void) {
    char GPRMC[GPRMC_LEN]; //maybe 7, used to check for GPRMC sentence
    uint8_t is_GPRMC_valid = 0;

    if (USART1->ISR & USART_ISR_ORE) // Overrun Error
    {
        USART1->ICR = USART_ICR_ORECF;
    }

    if (USART1->ISR & USART_ISR_NE) // Noise Error
    {
        USART1->ICR = USART_ICR_NCF;
    }

    if (USART1->ISR & USART_ISR_FE) // Framing Error
    {
        USART1->ICR = USART_ICR_FECF;
    }

    // Parse GPRMC data, ignore other sentences
    while (!is_GPRMC_valid) {
        memset(GPRMC, 0, GPRMC_LEN);
        while (1) {
            while (!(USART1 -> ISR & USART_ISR_RXNE));
            char curr = USART1 -> RDR;
            if (curr == '$') {
                break;
            }
        }
        char sentence_format[5];
        for (int i = 0; i < 5; i++) {
            while (!(USART1 -> ISR & USART_ISR_RXNE));
            char curr = USART1 -> RDR;
            sentence_format[i] = curr;
        }
        if (is_sentence_format_GPRMC(sentence_format)) {
            char curr = 0;
            for (int i = 0; i < GPRMC_LEN && (curr != '\r'); i++) {
                while (!(USART1 -> ISR & USART_ISR_RXNE));
                curr = USART1 -> RDR;
                GPRMC[i] = curr;
            }
        }
        if (GPRMC[12] == 'A') {
            is_GPRMC_valid = 1;
        }
    }

    parse_GPRMC(GPRMC, TZ_OFFSET);
}

void fill_utc_time(char* GPRMC, struct tm *utc_time) {
    // Fill in time of UTC_time struct
    utc_time -> tm_hour = (GPRMC[1] - 48) * 10 + GPRMC[2] - 48;
    utc_time -> tm_min = (GPRMC[3] - 48) * 10 + GPRMC[4] - 48;
    utc_time -> tm_sec = (GPRMC[5] - 48) * 10 + GPRMC[6] - 48;


    // Fill in date of UTC_time struct, GPRMC_idx starts at ddmmyy
    int GPRMC_idx = 7;
    for (int comma_cnt = 0; GPRMC[GPRMC_idx] != '\r' && comma_cnt < 8; GPRMC_idx++) {
        if (GPRMC[GPRMC_idx] == ',') {
            comma_cnt++;
        }
    }
    utc_time -> tm_mday = (GPRMC[GPRMC_idx] - 48) * 10 + GPRMC[GPRMC_idx + 1] - 48;
    utc_time -> tm_mon = (GPRMC[GPRMC_idx + 2] - 48) * 10 + GPRMC[GPRMC_idx + 3] - 48 - 1;
    utc_time -> tm_year = (GPRMC[GPRMC_idx + 4] - 48) * 10 + GPRMC[GPRMC_idx + 5] - 48 + 100;
}

void parse_GPRMC(char* GPRMC, int offset) {
    struct tm time;
    time_t stamp;
    putenv("TZ=UTC"); // start in UTC time zone
    fill_utc_time(GPRMC, &time);
    stamp = mktime(&time) + (3600 * offset); // convert struct to UNIX timestamp with TZ offset
    time = *(gmtime(&stamp));
    time_str = asctime(&time);

    char day[3] = {time_str[0], time_str[1], time_str[2]};

    rtc_date.RTC_Year = time.tm_year - 100;
    rtc_date.RTC_Month = time.tm_mon + 1;
    rtc_date.RTC_Date = time.tm_mday;
    if (strncmp(day, "Mon", 3) == 0) {
        rtc_date.RTC_WeekDay = RTC_Weekday_Monday;
    } else if (strncmp(day, "Tue", 3) == 0) {
        rtc_date.RTC_WeekDay = RTC_Weekday_Tuesday;
    } else if (strncmp(day, "Wed", 3) == 0) {
        rtc_date.RTC_WeekDay = RTC_Weekday_Wednesday;
    } else if (strncmp(day, "Thu", 3) == 0) {
        rtc_date.RTC_WeekDay = RTC_Weekday_Thursday;
    } else if (strncmp(day, "Fri", 3) == 0) {
        rtc_date.RTC_WeekDay = RTC_Weekday_Friday;
    } else if (strncmp(day, "Sat", 3) == 0) {
        rtc_date.RTC_WeekDay = RTC_Weekday_Saturday;
    } else {
        rtc_date.RTC_WeekDay = RTC_Weekday_Sunday;
    }

    int hour = time.tm_hour;
    if (hour > 12) {
        hour = hour % 12;
        rtc_time.RTC_H12 = RTC_H12_PM;
    } else {
        if (hour == 0) {
            hour = 12;
        }
        rtc_time.RTC_H12 = RTC_H12_AM;
    }
    rtc_time.RTC_Hours = hour;
    rtc_time.RTC_Minutes = time.tm_min;
    rtc_time.RTC_Seconds = time.tm_sec;
}

int is_sentence_format_GPRMC(char* format) {
    if (format[0] == 'G' && (format[1] == 'P' || format[1] == 'N' || format[1] == 'L') && format[2] == 'R' && format[3] == 'M' && format[4] == 'C') {
        return 1;
    }
    return 0;
}
