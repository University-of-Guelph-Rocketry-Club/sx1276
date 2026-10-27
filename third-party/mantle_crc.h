#ifndef MANTLE_CRC_H
#define MANTLE_CRC_H

#include <stddef.h>
#include <stdint.h>

#define CRC16 0x8005

/**
  * @brief Compute 16-bit CRC hash given some data.
  * @param data Buffer of the data to hash
  * @param size Length of the data buffer
  * @return 16-bit CRC hash of the data
  */
uint16_t gen_crc16(const uint8_t *data, uint16_t size);

#endif // MANTLE_CRC_H
