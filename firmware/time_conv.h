#ifndef _TIME_CONV_H_
#define _TIME_CONV_H_

#include <stdint.h>
#include <rtc.h>

uint32_t calendar_to_seconds(date_t date, time_t time);

void seconds_to_calendar(uint32_t s, date_t *date, time_t *time);

uint32_t calendar_to_alarm(date_t date, time_t time);


#endif
