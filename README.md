# SX1276 Radio Chip Driver
Radio chip driver for the UoG Rocketry club.

Note that no uplink is implemented/available so transmission is done on the satellite/rocket and reception on the ground station *only*.

# Project Structure
```
`- chip_main.c, chip_main.h     Main driver implementation
`- Makefile                     Builds the driver .bin file
`- proto/                       MANTLE radio protocol implementation; custom packet structure featuring CRC for verification
```

# Building and Flashing
```sh
make
st-flash write driver.bin 0x8000000
```

# Contributing
Feel free to submit PRs to the driver. Do not submit PRs to the protocol since it is vendored source code and is managed elsewhere (See txrx repository).
