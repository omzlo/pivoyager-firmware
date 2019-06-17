TEMPLATEROOT = ..

# compilation flags for gdb

CFLAGS  = -O3 -g -Wall
ASFLAGS = -g 

# object files

OBJS=  $(STARTUP) main.o
OBJS+= system.o systick.o gpio.o usart.o i2c_slave.o rtc.o adc.o time_conv.o 
# rtc.o 

# include common make file

include $(TEMPLATEROOT)/Makefile.common

# Override default for flash 
flash:	$(BIN)
				st-flash write $(BIN) 0x8002000
