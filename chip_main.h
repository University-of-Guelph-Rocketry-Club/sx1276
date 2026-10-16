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
const uint32_t SPI_MAX_TX_TIMEOUT  = 10; // 10ms delay for TX timeout
const uint32_t SPI_MAX_RETRY_DELAY = 10; // 10ms TX retry delay

extern SPI_HandleTypeDef hspi2;

// General SX1276 data.
// TODO: Send this around to functions that need this data.
typedef struct {
    uint8_t id;
} Radio_Chip_Ctx;

// General status codes for SX1276 SPI transactions.
typedef enum {
    SPI_OK,
    SPI_TIMEOUT,
    SPI_GENERAL_ERROR,
    SPI_BAD_ID
} SPI_Status;

// General status codes for SX1276 RF transactions.
typedef enum {
    PACKET_OK,
    PACKET_TIMEOUT,
    PACKET_CRC_INVALID,
    PACKET_BAD_HEADER,
} Packet_RX_Status;

/**
  * @brief SX1276 entry point, called from Core/Src/main.c. Validates the SX1276 ID then configures the chip for LoRa mode and CRC generation, then halts.
  */
SPI_Status sx1276_entry();

/**
  * @brief  Transmit data over SPI to the SX1276.
  * @param  hspi pointer to a SPI module configuration structure
  * @param  to_send Pointer to the data to send over SPI
  * @param  length Number of data elements to be sent
  * @param  spi_tx_timeout Transaction timeout
  * @param  spi_delay_timeout Transaction replay delay, in case of errors
  * @retval SPI transaction status
  */
SPI_Status sx1276_transmit(SPI_HandleTypeDef *hspi, const uint8_t *to_send, uint16_t size, uint32_t spi_tx_timeout, uint32_t spi_retry_delay);

/**
  * @brief  Transmit address byte and receieve data over SPI to the SX1276.
  * @param  hspi pointer to a SPI module configuration structure
  * @param  to_send Pointer to the data to send over SPI
  * @param  to_recv Pointer to storage for received data over SPI
  * @param  length Number of data elements to be sent/received
  * @param  spi_tx_timeout Transaction timeout
  * @param  spi_delay_timeout Transaction replay delay, in case of errors
  * @retval SPI transaction status
  */
SPI_Status sx1276_transmit_receieve(SPI_HandleTypeDef *hspi, const uint8_t *to_send, uint8_t *to_recv, uint16_t length, uint32_t spi_tx_timeout, uint32_t spi_retry_delay);

/**
 * @brief Transmit a LoRa packet 'p' over the air.
 * @param p Packet to transmit
 */
void sx1276_transmit_packet(packet p);

/**
 * @brief  Transmit a LoRa packet 'p' over the air.
 * @param  p Out pointer for the received packet
 * @return Packet reception status
 */
Packet_RX_Status sx1276_receive_packet(packet *p)

/**
  * @brief  Check validity of CRC hash on packet reception through the PayloadCrcError Irq flag (Section 4.1.1.6).
  * @param  crc CRC hash of the received packet
  * @return Whether the CRC hash is valid or not
  */
bool sx1276_is_last_packet_valid(uint16_t crc);

/**
  * @brief Reset the SX1276. See Section 5.2.2 for more details.
  */
void sx1276_reset();
