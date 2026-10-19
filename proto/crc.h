#ifndef CRC_H
#define CRC_H

#include <stddef.h>
#include <stdint.h>

#define CRC16 0x8005
uint16_t gen_crc16(const uint8_t *data, uint16_t size);

#endif // CRC_H