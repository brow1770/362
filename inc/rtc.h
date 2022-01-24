/*
 * rtc.h
 */


#define RTC_H_

#define ALARM_GPS  ((uint8_t)0x00)
#define ALARM_PROG ((uint8_t)0xFF)

void RTC_Clock_Enable(void);
void RTC_Init_Structs(void);
void RTC_Update_DateTime(void);
void RTC_Alarm_Enable(void);
void RTC_SetAlarm_Prog(uint8_t* set_alarm_prog);
void RTC_TurnAlarmOff(void);
