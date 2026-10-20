#include "Core/Inc/main.h"
#include "proto/txrx-proto.h"
#include "proto/checksum.h"
#include <assert.h>
#include <stdbool.h>

// See Section 4.4 of the datasheet for all register addresses and op mode constants.
#define REG_VERSION        (0x42)
#define REG_OPMODE         (0x01)
#define REG_MODEMCONFIG2   (0x1E)
#define REG_RXNBBYTES      (0x13)
#define REG_FIFORXCURADDR  (0x10)
#define REG_FIFOADDRPTR    (0x0D)
#define REG_FIFOTXBASEADDR (0x0E)
#define REG_FIFORXBASEADDR (0x0F)
#define REG_FIFO           (0x00)
#define REG_PAYLOADLENGTH  (0x22)
#define REG_IRQFLAGS       (0x12)
#define OPMODE_TX          (0b11111011)
#define OPMODE_STDBY       (0b11111001)
#define OPMODE_SLEEP       (0b11111000)
#define OPMODE_RXSINGLE    (0b11111110)
#define TXDONE_BIT         3
#define RXTIMEOUT_BIT      7
#define RXDONE_BIT         6
#define LORAMODE_BIT       7

// User macros/constants
#define USER_SET_BIT(byte, nBit) (byte |= (1 << nBit))
#define USER_GET_BIT(byte, nBit) ((byte >> nBit) & 0b00000001)
#define USER_CLEAR_BIT(byte, nBit) (byte &= ~(1 << nBit))
#define USER_ARRAY_LEN(array) (sizeof(array) / sizeof(array[0]))
// Clears the op mode bits then sets the new ones.
#define SET_OPMODE(op_mode, new_op_mode) ((op_mode & 0xF8) | (new_op_mode & 0x07))
// For changing the MSB (wnr bit) of the address byte to enable R/W permissions.
// - MSB = 0 for read
// - MSB = 1 for write
#define SET_READABLE(address_byte) USER_CLEAR_BIT(address_byte, 7)
#define SET_WRITEABLE(address_byte) USER_SET_BIT(address_byte, 7)
const uint8_t SPI_MAX_TX_TIMEOUT  = 10; // 10ms delay for TX timeout
const uint8_t SPI_MAX_RETRY_DELAY = 10; // 10ms TX retry delay
const uint8_t SPI_MAX_RETRIES     = 3;  // Max amount of TX retries.

// SPI handle initialized through the STM32 generated code.
extern SPI_HandleTypeDef hspi2;

// General SX1276 data.
typedef struct {
    uint8_t id;   // Version and recipient ID (for when transmitting packets)
    uint8_t mode; // Operation mode
} Radio_Chip_Ctx;

// General status codes for SX1276 SPI transactions.
// Same as the HAL ones but includes a BAD_ID code for debugging purposes.
typedef enum {
    SPI_OK,
    SPI_ERROR,
    SPI_TIMEOUT,
    SPI_BUSY,
    SPI_BAD_ID,
    SPI_STATUS_COUNT,
} SPI_Status;

// Compatibility between "custom" statuses and the HAL default ones.
SPI_Status SPI_STATUS_MAP[SPI_STATUS_COUNT - 1] = {
    [HAL_OK]      = SPI_OK,
    [HAL_ERROR]   = SPI_ERROR,
    [HAL_BUSY]    = SPI_BUSY,
    [HAL_TIMEOUT] = SPI_TIMEOUT,
};
static_assert(USER_ARRAY_LEN(SPI_STATUS_MAP) == 4, "Number of mapped HAL => SPI_Status codes has changed!");

// General status codes for SX1276 RF transactions/receptions.
typedef enum {
    PACKET_OK,
    PACKET_TIMEOUT,
    PACKET_CRC_INVALID,
    PACKET_STATUS_COUNT,
} Packet_Status;

/**
  * @brief SX1276 entry point, called from Core/Src/main.c. Validates the SX1276 ID then configures the chip for LoRa mode and CRC generation, then halts.
  */
SPI_Status sx1276_entry();

/**
  * @brief  Transmit address byte and receieve data over SPI to the SX1276.
  * @param  hspi pointer to a SPI module configuration structure
  * @param  reg Register number to read from over SPI
  * @param  to_recv Pointer to storage for received data over SPI
  * @param  spi_tx_timeout Transaction timeout
  * @param  spi_delay_timeout Transaction replay delay, in case of errors
  * @retval SPI transaction status
  */
SPI_Status sx1276_read(SPI_HandleTypeDef *hspi, const uint8_t reg, uint8_t *to_recv, uint32_t spi_tx_timeout, uint32_t spi_retru_delay);

/**
  * @brief  Transmit data over SPI to the SX1276.
  * @param  hspi pointer to a SPI module configuration structure
  * @param  to_send Pointer to the data to send over SPI
  * @param  spi_tx_timeout Transaction timeout
  * @param  spi_delay_timeout Transaction replay delay, in case of errors
  * @retval SPI transaction status
  */
SPI_Status sx1276_write(SPI_HandleTypeDef *hspi, const uint8_t *to_send, uint32_t spi_tx_timeout, uint32_t spi_retry_delay);

/**
 * @brief  Transmit a LoRa packet over the air.
 * @param  ctx Chip context
 * @param  p   Packet to transmit
 * @return Packet transmission status
 */
Packet_Status sx1276_transmit_packet(Radio_Chip_Ctx *ctx);

/**
 * @brief  Transmit a LoRa packet over the air.
 * @param  ctx Chip context
 * @return Packet reception status
 */
Packet_Status sx1276_receive_packet(Radio_Chip_Ctx *ctx);

/**
  * @brief  Check validity of CRC hash on packet reception through the PayloadCrcError Irq flag (Section 4.1.1.6).
  * @param  checksum CRC hash of the received packet
  * @return Whether the CRC hash is valid or not
  */
bool sx1276_is_crc_valid(uint16_t checksum);

/**
  * @brief  Checks if the TxDone interrupt is tripped.
  * @param  ctx Chip context
  * @param  read_status SPI TX status for acquiring the RegIrqFlags register data
  * @return True if interrupt is set and false otherwise. 
  */
bool sx1276_is_txdone(Radio_Chip_Ctx *ctx, SPI_Status *read_status);

/**
  * @brief  Checks if the RxDone or RxTimeout interrupt is tripped.
  * @param  ctx Chip context
  * @param  read_status SPI TX status for acquiring the RegIrqFlags register data
  * @return True if either interrupt is set and false otherwise.
  */
bool sx1276_is_rxdone_or_rxtimeout(Radio_Chip_Ctx *ctx, SPI_Status *read_status);

/**
  * @brief Reset the SX1276. See Section 5.2.2 for more details.
  */
void sx1276_reset();
