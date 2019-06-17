#include "time_conv.h"
#include "usart.h"

uint32_t calendar_to_seconds(date_t date, time_t time)
{
  uint32_t y, m, d, H, M, S;
  uint32_t era, yoe, doy, doe;

  y = FROM_BCD((date>>16)&0xFF)+2000;
  m = FROM_BCD((date>>8)&0x1F);
  d = FROM_BCD(date&0x3F);
  H = FROM_BCD((time>>16)&0x3F);
  M = FROM_BCD((time>>8)&0xFF);
  S = FROM_BCD(time&0x7F);

  //usart_printf("> %u-%u-%uT%u:%u:%u\n", y, m, d, H, M, S); 

  y -= (m<2);
  era = ((y>=0)?y:y-399)/400;
  yoe = y - era*400;                                  // [0-399]
  doy = (153*(m + (m > 2 ? -3 : 9)) + 2)/5 + d-1;     // [0-365]
  doe = yoe * 365 + yoe/4 - yoe/100 + doy;            // [0-146096]

  return (era * 146097 + doe - 719468)*86400 + H*3600 + M*60 + S;
}

void seconds_to_calendar(uint32_t s, date_t *date, time_t *time)
{
  uint32_t era, doe, yoe, doy, mp, y, m, d, H, M, S;
  uint32_t z = (s / 86400)+719468;
  uint32_t x = s % 86400;

  era = (z >= 0 ? z : z - 146096) / 146097;
  doe = z - era * 146097;
  yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365; 
  y = yoe + era * 400;
  doy = doe - (365*yoe + yoe/4 - yoe/100);
  mp = (5*doy + 2)/153;    
  d = doy - (153*mp+2)/5 + 1;
  m = mp + (mp < 10 ? 3 : -9);
  y += (m <= 2);
  H = x/3600;
  M = (x%3600)/60;
  S = x%60;
  *date = bcd_date(0, TO_BCD(d), TO_BCD(m), TO_BCD(y-2000));
  *time = bcd_time(TO_BCD(H), TO_BCD(M), TO_BCD(S));
}

uint32_t calendar_to_alarm(date_t date, time_t time)
{
  uint32_t alarm = time;
  alarm |= (date & 0xFF)<<24;
  return alarm;
}
