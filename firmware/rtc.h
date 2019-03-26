#ifndef _RTC_H_
#define _RTC_H_
#include <stdint.h>

typedef uint32_t time_t;

typedef uint32_t date_t;

inline uint8_t TO_BCD(uint8_t b) { return ((((b)/10)<<4)|((b)%10)); }

inline uint8_t FROM_BCD(uint8_t b) { return ((((b)>>4)*10)+((b)&0xf)); }

int rtc_init(void);

void rtc_disable_write_protection(void);

void rtc_enable_write_protection(void);

time_t bcd_time(uint32_t hours, uint32_t minutes, uint32_t seconds);

date_t bcd_date(uint32_t weekday, uint32_t day, uint32_t month, uint32_t year);

void rtc_set_time(time_t tm);
time_t rtc_get_time(void);

void rtc_set_date(date_t dt);
date_t rtc_get_date(void);

void rtc_enable_calendar_init(void);

void rtc_disable_calendar_init(void);

void rtc_set_alarm(uint32_t alarm);

void rtc_enable_alarm(void);

void rtc_disable_alarm(void);

#endif
