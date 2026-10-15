# SX1276 Radio Chip Driver
Radio chip driver for the UoG Rocketry club.

# Info
- Pin 7 (NRESET) can be used to reset the chip; pull low for 100us (microseconds) then wait 5ms after the reset before any new SPI traffic can be ingested/handled/sent.
- Using custom CRC implementation that is packed into the payload data.

# Tasks
- Determine what timeout we should set for transactions (Changing LoRaRxTimeout mainly, changing symbol period might be possible)
- Get the chip ID or something over SPI, on an stm32 nucleo board
        - Determine if we should be using LoRa or FSK/OOK modem, ask questy??
        - Determine what sort of input/output we are getting? Are we just sending data over the chip over the air? Just constantly send data over radio and hope it gets received?
    - See notes above, shouldn't be that hard (just read in the register and do some serial print idk)

# Building and Flashing
```sh
make
st-flash write driver.bin
```
