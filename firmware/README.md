# Notes about flashing the firmware to the PiMaster

Driver can be flashed with the following process:

1) Convert elf firmware to bin

```
arm-none-eabi-objcopy -O binary driver.elf driver.bin
```

2) Upload firmware through SWD connector using st-flash

```
st-flash write driver.bin 0x8000000
``` 

We use the open-source utility [stlink][1] to flash firmware on the STM32F0 with a cheap "ebay-sourced" 3 dollar ST-LINK V2 clone. 

[1]: https://github.com/texane/stlink

