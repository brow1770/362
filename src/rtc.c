/*
 * rtc.c
 */

#include "stm32f0xx.h"
#include "gps.h"
#include "player.h"
#include "midi.h"
#include "rtc.h"
#include "lcd.h"

extern RTC_InitTypeDef rtc_init;
extern RTC_TimeTypeDef rtc_time;
extern RTC_DateTypeDef rtc_date;
extern RTC_AlarmTypeDef rtc_alarm_gps;
extern RTC_AlarmTypeDef rtc_alarm_prog;
extern uint8_t alarm_mode;
extern uint8_t midifile[];
extern uint8_t is_alarm_on;
extern uint8_t is_alarm_set;

void RTC_Clock_Enable(void) {
    // enable write access to backup domain
    if ((RCC -> APB1ENR & RCC_APB1ENR_PWREN) == 0) {
        // enable power interface clk
        RCC -> APB1ENR |= RCC_APB1ENR_PWREN;
        // short delay after enable an RCC peripheral clk
        (void) RCC -> APB1ENR;
    }

    if ((PWR -> CR & PWR_CR_DBP) == 0){
       // enable write access to RTC and registers in backup domain
       PWR -> CR |= PWR_CR_DBP;
       // wait until the backup domain write protection has been disabled
       while ((PWR -> CR & PWR_CR_DBP) == 0);
    }

    // reset LSION bit before configuring LSI
    // BDCR = Backup domain control reg
    // CSR = Control/status reg
    RCC -> CSR &= ~RCC_CSR_LSION;

    // RTC clk selection can be changed only if the backup domain is in reset
    RCC -> BDCR |= RCC_BDCR_BDRST;
    RCC -> BDCR &= ~RCC_BDCR_BDRST;

    // wait until LSI clk is ready
    while ((RCC -> CSR & RCC_CSR_LSIRDY) == 0) {
        RCC -> CSR |= RCC_CSR_LSION; // enable LSI oscillator
    }

    // select LSI as RTC clk source
    // RTCSEL[1:0]: 00 = no clk, 01 = LSE, 10 = LSI, 11 = HSE
    RCC -> BDCR &= ~RCC_BDCR_RTCSEL; // clear RTCSEL bits
    RCC -> BDCR |= RCC_BDCR_RTCSEL_1; // LSI as RTC clk

    // Disable power interface clk
    RCC -> APB1ENR &= ~RCC_APB1ENR_PWREN;

    RCC -> BDCR |= RCC_BDCR_RTCEN;
}

void RTC_Init_Structs(void) {
    // Init struct for RTC_Init
    rtc_init = (RTC_InitTypeDef) {.RTC_HourFormat = RTC_HourFormat_12,
                                  .RTC_AsynchPrediv = 99,
                                  .RTC_SynchPrediv = 399};
    // Time struct for RTC_SetTime
    RTC_TimeStructInit(&rtc_time); //***
    // Date struct for RTC_SetDate
    RTC_DateStructInit(&rtc_date);

    // GPS Alarm struct for RTC_SetAlarm, AlarmMask set to mask hour and day
    RTC_AlarmStructInit(&rtc_alarm_gps);
    rtc_alarm_gps.RTC_AlarmMask = 0x80800000;

    // Programmable Alarm struct for RTC_SetAlarm, AlarmMask set to mask day
    RTC_AlarmStructInit(&rtc_alarm_prog);
    rtc_alarm_prog.RTC_AlarmMask = 0x80000000;

    alarm_mode = ALARM_GPS; //***
}

void RTC_Update_DateTime(void) {
    get_GPS_data();
    RTC_SetTime(RTC_Format_BIN, &rtc_time);
    RTC_SetDate(RTC_Format_BIN, &rtc_date);
}

void RTC_Alarm_Enable(void) {
    // Configure EXTI 17, connected to the RTC Alarm event
    // Select triggering edge
    EXTI -> RTSR |= EXTI_RTSR_TR17; // 1 = trigger at rising edge

    // Interrupt mask reg
    EXTI -> IMR |= EXTI_IMR_MR17; // 1 = enable EXTI 17 line

    // Event mask reg
    EXTI -> EMR |= EXTI_EMR_MR17; // 1 = enable EXTI 17 line

    // Interrupt pending reg
    EXTI -> PR |= EXTI_PR_PR17; // Write 1 to clear pending interrupt

    // Enable RTC interrupt
    NVIC -> ISER[0] |= 1 << RTC_IRQn;

    // Set interrupt priority as the most urgent
    NVIC_SetPriority(RTC_IRQn, 0);
}

void RTC_IRQHandler(void) {
    // RTC initialization and status reg (RTC_ISR)
    // Hardware sets the alarm A flag (ALRAF) when the time/date regs
    // (RTC_TR and RTC_DR) match the alarm A reg (RTC_ALRMAR), according
    // to the mask bits
    // alarm_mode: 0x00 = GPS, 0xFF = PROG
    // If RTC alarm takes interrupt takes place:
    // 1) Check if alarm mode is GPS or PROG
    // 2) If GPS:
    //  1) Call GPS_GetTime() every hour, Call GPS_GetDate() every day
    //  2) RTC_SetTime() and RTC_SetDate() accordingly
    //  3) Check if alarm function switches from GPS --> PROG
    //  4) If time == PROG time, run PROG steps
    // 3) If PROG:
    //  1) Switch alarm function from PROG --> GPS
    //  2) Play music until turned off
    // May have issues when PROG alarm == GPS alarm, or when Update_DateTime sends time back
    if (RTC -> ISR & RTC_ISR_ALRAF) {
        GPIOC -> ODR ^= 1 << 6;
        if (alarm_mode == ALARM_GPS) {
            GPIOC -> ODR ^= 1 << 7;
            uint32_t rtc_tr = RTC -> TR;
            int rtc_hr = ((rtc_tr >> 16 & 0x30) | (rtc_tr >> 16 & 0xF));
            if (rtc_alarm_prog.RTC_AlarmTime.RTC_Hours == rtc_hr) {
                alarm_mode = ALARM_PROG;
                RTC_AlarmCmd(RTC_Alarm_A, DISABLE);
                RTC_SetAlarm(RTC_Format_BCD, RTC_Alarm_A, &rtc_alarm_prog);
                RTC_AlarmCmd(RTC_Alarm_A, ENABLE);
            }
            RTC_Update_DateTime();
        }
        if (alarm_mode == ALARM_PROG && is_alarm_set) {
            alarm_mode = ALARM_GPS;
            GPIOC -> ODR ^= 1 << 8;
            RTC_AlarmCmd(RTC_Alarm_A, DISABLE);
            RTC_SetAlarm(RTC_Format_BCD, RTC_Alarm_A, &rtc_alarm_gps);
            RTC_AlarmCmd(RTC_Alarm_A, ENABLE);

            // Play music, change clkface to alarm face
            is_alarm_on = 1;
            makeAlarmFace();

            MIDI_Player *mp = midi_init(midifile);
            init_TIM2(10417);
        }
        RTC -> ISR &= ~(RTC_ISR_ALRAF);
    }

    // Clear the EXTI line 17
    EXTI -> PR |= EXTI_PR_PR17; // write 1 to clear pending interrupt
}

void RTC_SetAlarm_Prog(uint8_t* set_alarm_prog) {
    rtc_alarm_prog.RTC_AlarmTime.RTC_Hours = (set_alarm_prog[0] << 4) | set_alarm_prog[1];
    rtc_alarm_prog.RTC_AlarmTime.RTC_Minutes = (set_alarm_prog[2] << 4) | set_alarm_prog[3];
    rtc_alarm_prog.RTC_AlarmTime.RTC_Seconds = (set_alarm_prog[4] << 4) | set_alarm_prog[5];
    rtc_alarm_prog.RTC_AlarmTime.RTC_H12 = set_alarm_prog[6];
    is_alarm_set = 1;
    uint32_t rtc_tr = RTC -> TR;
    int rtc_hr = ((rtc_tr >> 16 & 0x30) | (rtc_tr >> 16 & 0xF));
    if (rtc_alarm_prog.RTC_AlarmTime.RTC_Hours == rtc_hr) {
        alarm_mode = ALARM_PROG;
        RTC_AlarmCmd(RTC_Alarm_A, DISABLE);
        RTC_SetAlarm(RTC_Format_BCD, RTC_Alarm_A, &rtc_alarm_prog);
        RTC_AlarmCmd(RTC_Alarm_A, ENABLE);
    } else {
        alarm_mode = ALARM_GPS;
    }
}

void RTC_TurnAlarmOff(void) {
    TIM2->CR1 &=~ TIM_CR1_CEN;
    for (int i = 0; i < 128; i++)
    {
       note_off(0, 0, i, 0);
    }

    makeClockFace();
    is_alarm_on = 0;
}
