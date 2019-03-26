#include <stm32f0xx.h>
#include <stm32f0_helpers.h>
#include "usart.h"

unsigned usart_debug_enable = 0;

#ifdef USE_USART4

/* NOTES:
 * - Configures PA0(TX) and PA1(RX) as USART4
 *   Works only on STM32F072 not 042
 */

int usart_init(uint32_t baud)
{
    /* Enable the peripheral clock of GPIOA */
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    /* Enable clock for USART4 */
    RCC->APB1ENR |= RCC_APB1ENR_USART4EN; 

    // Configure TX pin
    GPIO_CONFIGURE_ALTERNATE_FUNCTION(GPIOA, 0, GPIO_AF_4);
    // Configure RX pin
    GPIO_CONFIGURE_ALTERNATE_FUNCTION(GPIOA, 1, GPIO_AF_4);

    // Configure the UART

    /* USART CR1 Configuration -*/
    /* Set CR1 */
    USART4->CR1 = 
        USART_CR1_RE |  // recieve enable
        USART_CR1_TE;   // send enable
    // No parity
    // No interrupts generated

    /* USART CR2 Configuration -*/
    USART4->CR2 = 0;
    // Set 1 stop it, default


    /* USART CR3 Configuration -*/  
    USART4->CR3 = 0;
    // No hardware flow control, default

    /* USART BRR Configuration -*/
    /* Write to USART BRR */
    USART4->BRR = (uint16_t)((48000000+(baud-1))/baud);

    // enable
    USART4->CR1 |= USART_CR1_UE;

    return 0;    
}

int usart_getc (void)
{
    while ((USART4->ISR & USART_ISR_RXNE) == 0);
    return  USART4->RDR & 0xff;
}

int usart_available(void)
{
    return (USART4->ISR & USART_ISR_RXNE) != 0;
}

int usart_putc(int c)
{
  while ((USART4->ISR & USART_ISR_TXE) == 0);
  USART4->TDR = (uint16_t)(c&0xFF);
  return 0;
}

#else

/* NOTES:
 * - Configures PA9(TX) and PA10(RX) as USART1
 */

int usart_init(uint32_t baud)
{
    /* Enable the peripheral clock of GPIOA */
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    /* Enable clock for USART1 */
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN; 

    // Configure TX pin
    GPIO_CONFIGURE_ALTERNATE_FUNCTION(GPIOA, 9, GPIO_AF_1);
    // Configure RX pin
    GPIO_CONFIGURE_ALTERNATE_FUNCTION(GPIOA, 10, GPIO_AF_1);

    // Configure the UART

    /* USART CR1 Configuration -*/
    /* Set CR1 */
    USART1->CR1 = 
        USART_CR1_RE |  // recieve enable
        USART_CR1_TE;   // send enable
    // No parity
    // No interrupts generated

    /* USART CR2 Configuration -*/
    USART1->CR2 = 0;
    // Set 1 stop it, default


    /* USART CR3 Configuration -*/  
    USART1->CR3 = 0;
    // No hardware flow control, default

    /* USART BRR Configuration -*/
    /* Write to USART BRR */
    USART1->BRR = (uint16_t)((48000000+(baud-1))/baud);

    // enable
    USART1->CR1 |= USART_CR1_UE;

    return 0;    
}

int usart_getc (void)
{
    while ((USART1->ISR & USART_ISR_RXNE) == 0);
    return  USART1->RDR & 0xff;
}

int usart_available(void)
{
    return (USART1->ISR & USART_ISR_RXNE) != 0;
}

int usart_putc(int c)
{
  while ((USART1->ISR & USART_ISR_TXE) == 0);
  USART1->TDR = (uint16_t)(c&0xFF);
  return 0;
}


#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

const char digits[]="0123456789abcdef";

static void _process_uint(unsigned u)
{
    unsigned d,c;

    if (u==0) {
        usart_putc('0');
        return;
    }

    d=1000000000; // 32 bits means max uint = 4,294,967,295
    while (d>u) d/=10;
    while (d)
    {
        c = u/d;
        usart_putc(digits[c]);
        u %= d;
        d /= 10;
    }
    return;
}

static void _process_string(const char *s)
{
    while (*s) usart_putc(*s++);
}

static void _process_hex(unsigned u)
{
    unsigned su,c;

    if (u==0) 
    {
        usart_putc('0');
        usart_putc('0');
        return;
    }         
    su = 24;
    while ((u>>su)==0) su-=8;
    su+=4;
    for (;;) {
        c = (u>>su)&0xF;
        usart_putc(digits[c]);
        if (su==0) break;
        su-=4;
    }
}

int usart_vprintf(const char *format, va_list ap)
{   
    int i;
    unsigned u;
    const char *s;

    while (*format) {
        if (*format=='%')
        {
            format++;
            switch (*format) {
                case '%':
                    format++;
                    usart_putc(*format++);
                    break;
                case 'i':
                    format++;
                    i = va_arg(ap, int);
                    if (i<0)
                    {
                        usart_putc('-');
                        i = -i;
                    }
                    _process_uint((unsigned)i);
                    break;
                case 'u':
                    format++;
                    u = va_arg(ap, unsigned);
                    _process_uint(u);
                    break;
                case 'x':
                case 'X':
                    format++;
                    u = va_arg(ap, unsigned);
                    _process_hex(u);
                    break;
                case 's':
                    format++;
                    s = va_arg(ap, const char *);
                    _process_string(s);    
                    break;
                case 'c':
                    format++;
                    i = va_arg(ap, int);
                    usart_putc(i);
                    break;
                case 'p':
                    format++;
                    _process_string("0x");
                    u = va_arg(ap, unsigned);
                    _process_hex(u);
                    break;
                default:
                    format++;
                    _process_string("<?format?>");
                    return -1;
            }
        }
        else
        {
            if (*format=='\n')
                usart_putc('\r');
            usart_putc(*format++);
        }
    }

    return 0;
}


int usart_printf(const char *format, ...)
{
    va_list ap;
    int retval;

    va_start(ap, format);
    retval = usart_vprintf(format, ap);
    va_end(ap);
    return retval;
}

int usart_debug_printf(const char *format, ...)
{
    if (usart_debug_enable)
    {
        va_list ap;
        int retval;

        va_start(ap, format);
        retval = usart_vprintf(format, ap);
        va_end(ap);
        return retval;
    }
    return 0;
}


