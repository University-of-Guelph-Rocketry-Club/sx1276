#include "Core/Inc/main.h"
#include <stdbool.h>

// See Section 4.4 of the datasheet for all register addresses.
#define REG_VERSION      (0x42)
#define REG_OPMODE       (0x01)
#define REG_MODEMCONFIG2 (0x1E)

// User macros/constants
#define USER_SET_BIT(byte, nBit) (byte |= (1 << nBit))
#define USER_CLEAR_BIT(byte, nBit) (byte &= ~(1 << nBit))
#define USER_ARRAY_LEN(array) (sizeof(array) / sizeof(array[0]))
#define SET_READABLE(address_byte) USER_CLEAR_BIT(address_byte, 7)
#define SET_WRITEABLE(address_byte) USER_SET_BIT(address_byte, 7)
const int SPI_MAX_TIMEOUT = 10; // 10ms delay

// General SX1276 data.
typedef struct {
    SPI_HandleTypeDef *spi;
    HAL_StatusTypeDef ok;
    uint8_t op_mode;
} Radio_Chip_Config;

// An explicit (default) LoRa packet header.
typedef struct {
    uint8_t payload_length;
    uint8_t forward_error_correction_code_rate; // maximum value of 4/8
    uint16_t crc_hash; // Optional, set "through the bit RxPayloadCrcOn in the register RegModemConfig1" (Section 4.1.1.6).
} LoRa_Explicit_Header;

// A LoRa packet.
// FIXME: This format is not correct! It will be whatever we end up sending over the wire through our MANTLE (https://github.com/University-of-Guelph-Rocketry-Club/MANTLE) protocol.
typedef struct {
    uint8_t preamble[6];
    LoRa_Explicit_Header header;
    uint8_t payload[256]; // FIFO Data Buffer (Section 4.1.2.3) is 256 bytes long and contains our payload, so this will be the max for now.
} LoRa_Packet;

// General status codes for SX1276 SPI transactions.
typedef enum {
    SPI_OK,
    SPI_TIMEOUT,
    SPI_GENERAL_ERROR,
    SPI_BAD_ID
} Spi_Status;

// SX1276 entry point, called from Core/Src/main.c. Validates the SX1276 ID then configures the chip for LoRa mode and CRC generation, then halts.
Spi_Status chip_entry();

// Transmit a LoRa packet 'p' over the wire.
void chip_transmit_packet(LoRa_Packet p);

// Receive a LoRa packet from another radio over the wire.
LoRa_Packet chip_send_packet(LoRa_Packet p);

// Check validity of CRC hash on packet reception through the PayloadCrcError Irq flag (Section 4.1.1.6).
bool chip_is_last_packet_valid();

// Reset the SX1276. See Section 5.2.2 for more details.
void chip_reset();
