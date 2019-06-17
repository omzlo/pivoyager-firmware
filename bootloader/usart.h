#ifndef _USART_H
#define _USART_H

#include <stdarg.h>

int usart_init(uint32_t baud);
int usart_deinit();
int usart_putc(int c);
int usart_getc();
int usart_available(void);

int usart_vprintf(const char *format, va_list ap);
int usart_printf(const char *format, ...);

extern unsigned usart_debug_enable;
int usart_debug_printf(const char *format, ...);


#endif

