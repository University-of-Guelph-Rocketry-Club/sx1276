#include "chip_main.h"

/**
  * @brief SX1276 entry point, called from Core/Src/main.c. Validates the SX1276 ID then configures the chip for LoRa mode and CRC generation, then halts.
  */
SPI_Status sx1276_entry()
{
    // Init chip context and variables for SPI TX.
    Radio_Chip_Ctx ctx = {};
    uint8_t address_byte = 0;
    SPI_Status ok = 0;
  
    // Get version ID from the chip then go into LoRa + Standby mode. If ID is bad, exit immediately.
    uint8_t version = 0xcc;
    if (ok = sx1276_read(&hspi2, REG_VERSION, &version, SPI_MAX_TX_TIMEOUT, SPI_MAX_RETRY_DELAY); ok != SPI_OK) return ok;

    // Check version ID (0x12 is default)
    if (version == 0x00 || version == 0xFF) {
        printf("Bad version ID: %#02x\n", version);
        return SPI_BAD_ID;
    } else {
        // Green LED on.
        chip.id = version;
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
    }

    // Get the initial op mode value
    uint8_t op_mode = 0;
    if (ok = sx1276_read(&hspi2, REG_OPMODE, &op_mode, SPI_MAX_TX_TIMEOUT, SPI_MAX_RETRY_DELAY); ok != SPI_OK) return ok;
    printf("Got op mode: %#08x\n", op_mode);

    // Go into LoRa mode by first entering SLEEP mode then setting the 7th bit (MSB) of the RegOpMode register (0x01). See Section 4.4 of the datasheet.
    uint8_t sleep_data   = SET_OPMODE(op_mode, OPMODE_SLEEP); // Last 3 bits are 000 to go into SLEEP mode.
    uint8_t lora_data    = sleep_data; USER_SET_BIT(lora_data, LORAMODE_BIT); // Set MSB to 1 to go into LoRa mode.
    uint8_t standby_data = SET_OPMODE(lora_data, OPMODE_STDBY); // Go back into STDBY (001) mode.
    
    // Set the address for RegOpMode with write permissions.
    address_byte = REG_OPMODE;
    uint8_t sleep_tx[2]  = {address_byte, sleep_data};
    uint8_t lora_tx[2]   = {address_byte, lora_data};
    uint8_t stdby_tx[2]  = {address_byte, standby_data};

    // Go into SLEEP mode first to *then* go into LoRa mode.
    if (ok = sx1276_write(&&hspi2, sleep_tx, SPI_MAX_TX_TIMEOUT, SPI_MAX_RETRY_DELAY); ok != SPI_OK) return ok;
    if (ok = sx1276_write(&&hspi2, lora_tx, SPI_MAX_TX_TIMEOUT, SPI_MAX_RETRY_DELAY); ok != SPI_OK) return ok; 
  
    // Set op mode to standby again, but now with LoRa on. Save the mode too.
    if (ok = sx1276_write(&&hspi2, stdby_tx, SPI_MAX_TX_TIMEOUT, SPI_MAX_RETRY_DELAY); ok != SPI_OK) return ok; 
    printf("In LoRa mode, standing by: op mode: %#08x\n", standby_data);
    ctx.mode = standby_data;
    
    // Configure TX FIFO pointer to point to base (0).
    address_byte = REG_FIFOTXBASEADDR;
    uint8_t fifoptraddr_set_tx[2] = {address_byte, 0x00};
    if (ok = sx1276_write(&hspi2, fifoaddrptr_set_tx, SPI_MAX_TX_TIMEOUT, SPI_MAX_RETRY_DELAY); ok != SPI_OK) return ok;

    // Configure RX FIFO pointer to point to base (0).
    address_byte = REG_FIFORXBASEADDR;
    fifoptraddr_set_tx[0] = address_byte;
    if (ok = sx1276_write(&hspi2, fifoaddrptr_set_tx, SPI_MAX_TX_TIMEOUT, SPI_MAX_RETRY_DELAY); ok != SPI_OK) return ok;

    //
    // NOTE: Frequency is already set at 434MHz by default, so we won't even change it.
    //

    // Finished, halt.
    while (1) {}
}

/**
  * @brief  Transmit data over SPI to the SX1276.
  * @param  hspi pointer to a SPI module configuration structure
  * @param  to_send Pointer to the data to send over SPI
  * @param  spi_tx_timeout Transaction timeout
  * @param  spi_delay_timeout Transaction replay delay, in case of errors
  * @retval SPI transaction status
  */
SPI_Status sx1276_write(SPI_HandleTypeDef *hspi, const uint8_t *to_send, uint32_t spi_tx_timeout, uint32_t spi_retry_delay)
{
    // Set write permissions to address byte.
    SET_WRITEABLE(to_send[0]);
    HAL_StatusTypeDef res = 0;
    uint8_t retries = 0;
    while (retries != SPI_MAX_RETRIES) {
        // Always sending the address and value to write.
        res = HAL_SPI_Transmit(hspi, to_send, 2, spi_tx_timeout);
        if (res != HAL_OK) {
            HAL_Delay(spi_delay_timeout);
        } else {
            return SPI_OK;
        }
        retries += 1;
    }
    return SPI_STATUS_MAP[res];
}

/**
  * @brief  Transmit address byte and receieve data over SPI to the SX1276.
  * @param  hspi pointer to a SPI module configuration structure
  * @param  reg Register number to read from over SPI
  * @param  to_recv Pointer to storage for received data over SPI
  * @param  spi_tx_timeout Transaction timeout
  * @param  spi_delay_timeout Transaction replay delay, in case of errors
  * @retval SPI transaction status
  */
SPI_Status sx1276_read(SPI_HandleTypeDef *hspi, const uint8_t reg, uint8_t *to_recv, uint32_t spi_tx_timeout, uint32_t spi_retru_delay)
{
    // Set read permissions to address byte.
    SET_READABLE(*to_send);
    HAL_StatusTypeDef res = 0;
    uint8_t retries = 0;
    while (retries != SPI_MAX_RETRIES) {
        // Always sending the address getting some byte value back.
        res = HAL_SPI_TransmitReceive(hspi, &reg, to_recv, 1, spi_tx_timeout);
        if (res != HAL_OK) {
            HAL_Delay(spi_delay_timeout);
        } else {
            return SPI_OK;
        }
        retries += 1;
    }
    return SPI_STATUS_MAP[res];
}

/**
 * @brief  Transmit a LoRa packet over the air.
 * @param  ctx Chip context
 * @param  p   Packet to transmit
 * @return Packet transmission status
 */
Packet_Status sx1276_transmit_packet(Radio_Chip_Ctx ctx)
{
    // NOTE: We always set the transmit/receieve FIFO pointers to the base (0) to use the full 256 bytes of the buffer space.

    // Construct packet.
    // TODO: Set recipient ID, packet size, packet command, and the payload itself (probably depending on the command).
    packet p = {};
    uint8_t recipient_id = 0;
    // TODO: Change the satellite command here when needed!
    gsc_packet_create_standalone_request(&p, &recipient_id, GSC_RAW_DATA);

    // Get packet bytes and extract payload length from 15th bit.
    // TODO: Ensure that the packet is actually a packed structure with no padding otherwise this won't work!!!!!!!!
    uint8_t packet_bytes = (*uint8_t)p;
    uint8_t payload_length = packet_bytes[15];
    
transmit_retry:
    // Write data to FIFO buffer over SPI
    // 1. Set RegPayloadLength to be the number of bytes to be sent.
    uint8_t address_byte = REG_PAYLOADLENGTH;
    uint8_t payload_length_tx[2] = { address_byte, payload_length };
    if (sx1276_write(&hspi2, payload_length_tx, SPI_MAX_TX_TIMEOUT, SPI_MAX_RETRY_DELAY) != SPI_OK) {
        goto transmit_retry;
    }

    // 2. Set RegFifoAddrPtr to 0, then write RegPayloadLength bytes to RegFifo.
    address_byte = REG_FIFOADDRPTR;
    uint8_t fifoaddrptr_set_tx[2] = {address_byte, 0x00};
    if (sx1276_write(&hspi2, fifoaddrptr_set_tx, SPI_MAX_TX_TIMEOUT, SPI_MAX_RETRY_DELAY) != SPI_OK) {
        goto transmit_retry;
    }
    address_byte = REG_FIFO;
    for (int i = 0; i < packet_length; i++) {
        // NOTE: address_byte SHOULD be automatically incremented, per Section 4.1.2.3.
        uint8_t payload_write_tx = {address_byte, packet_bytes[i]};
        if (sx1276_write(&hspi2, payload_write_tx, SPI_MAX_TX_TIMEOUT, SPI_MAX_RETRY_DELAY) != SPI_OK) {
            goto transmit_retry;
        }
    }

    // 3. Go into TX mode
    address_byte = REG_OPMODE;
    ctx->mode = SET_OPMODE(ctx->mode, OPMODE_TX);
    uint8_t tx_mode_tx[2] = {address_byte, ctx.mode};
    sx1276_write(&hspi2, tx_mode_tx, SPI_MAX_TX_TIMEOUT, SPI_MAX_RETRY_DELAY);

    // 4. Wait until IRQ TxDone before exiting
    SPI_Status txdone_ok;
    while (!sx1276_is_txdone()) {}
    if (txdone_ok == SPI_TIMEOUT) return PACKET_TIMEOUT;

    // Now in standby mode once TX is done.
    ctx->mode = SET_OPMODE(ctx->mode, OPMODE_STDBY);
    return PACKET_OK;
}

/**
 * @brief  Transmit a LoRa packet over the air.
 * @param  ctx Chip context
 * @return Packet reception status
 */
Packet_Status sx1276_receive_packet(Radio_Chip_Ctx *ctx)
{
    // NOTE: We always set the transmit/receieve FIFO pointers to the base (0) to use the full 256 bytes of the buffer space.
    uint8_t address_byte = 0;

    // NOTE: After receiving the packet, we are put into standby mode. Steps are shown in Section 4.1.6:
    // 1. (Assuming single request) Go into Rx Single mode
    address_byte = REG_OPMODE;
    ctx->mode = SET_OPMODE(ctx->mode, OPMODE_RXSINGLE);
    uint8_t rxsingle_mode_tx[2] = {address_byte, ctx.mode};
    sx1276_write(&hspi2, rxsingle_mode_tx, SPI_MAX_TX_TIMEOUT, SPI_MAX_RETRY_DELAY);

    // 2. Wait for RxDone or RxTimeout IRQ
    SPI_Status rxdone_ok;
    while (!sx1276_is_rxdone_or_rxtimeout(ctx, &rxdone_ok)) {}
    if (rxdone_ok == SPI_TIMEOUT) return PACKET_TIMEOUT;

receive_retry:
    // 3. Set RegFifoAddrPtr to RegFifoRxCurrentAddr, then read RegFifo over SPI, RegRxNbBytes times (number of bytes received).
    uint8_t packet_size = 0, rx_currentaddr = 0;
    SPI_Status curaddr_ok = sx1276_read(&hspi2, REG_FIFORXCURADDR, &rx_currentaddr, SPI_MAX_TX_TIMEOUT, SPI_MAX_RETRY_DELAY);
    SPI_Status packet_size_ok = sx1276_read(&hspi2, REG_RXNBBYTES, &packet_size, SPI_MAX_TX_TIMEOUT, SPI_MAX_RETRY_DELAY);
    if (packet_size_ok != SPI_OK || curaddr_ok != SPI_OK)
        goto receive_retry;
    }

fifo_read_retry:
    // 4. Read Rx Data in FIFO buffer to local buffer then cast to packet.
    uint8_t fifo_data[256] = {0};
    for (int i = 0; i < USER_ARRAY_LEN(fifo_data); i++) {
        if (sx1276_read(&hspi2, REG_FIFO, fifo_data[i], SPI_MAX_TX_TIMEOUT, SPI_MAX_RETRY_DELAY) != SPI_OK) {
            goto fifo_read_retry;
        }
    }
    packet *p = (*packet)fifo_data;

    // Get little endian values for the CRC and packet ID.
    // TODO: Check compiled code to see what instructions get generated? Probably 'REV' or maybe 'RBIT'?
    uint64_t packet_id_be = p->header.packet_id;
    uint64_t packet_id = __builtin_bswap64(packet_id_be);
    uint16_t checksum_be = p->checksum;
    uint16_t checksum = __builtin_bswap16(checksum_be);
    if (!sx1276_is_crc_valid(checksum)) return PACKET_CRC_INVALID;

    // TODO: Do other stuff with the packet data? Switch case of satellite commands?

    // Save mode and return ok.
    ctx->mode = SET_OPMODE(ctx->mode, OPMODE_STDBY);
    return PACKET_OK;
}

/**
  * @brief  Check validity of CRC hash on packet reception through the PayloadCrcError Irq flag (Section 4.1.1.6).
  * @param  checksum CRC hash of the received packet
  * @return Whether the CRC hash is valid or not
  */
bool sx1276_is_crc_valid(uint16_t checksum)
{
    // TODO: Check CRC hash inside of packed payload through MANTLE protocol
}

/**
  * @brief  Checks if the TxDone interrupt is tripped.
  * @param  ctx Chip context
  * @param  read_status SPI TX status for acquiring the RegIrqFlags register data
  * @return True if interrupt is set and false otherwise. 
  */
bool sx1276_is_tx_done(Radio_Chip_Ctx ctx, SPI_Status *read_status);
{
    uint8_t address_byte = REG_IRQFLAGS, irqflags = 0, retries = 0;
    if (sx1276_read(&hspi2, REG_IRQFLAGS, &irqflags, SPI_MAX_TX_TIMEOUT, SPI_MAX_RETRY_DELAY) != SPI_OK) {
        retries += 1;
        if (retries == SPI_MAX_RETRIES) {
            *read_status = SPI_TIMEOUT;
            // NOTE: This is a "fake" true return.
            // We return 'true' to exit the spin "lock" of waiting on this IRQ clear operatoin.
            return true;
        } else {
            goto irq_retry;
        }
    }
    *read_status = SPI_OK;

    uint8_t txdone = USER_GET_BIT(irqflags, TXDONE_BIT);
    if (txdone) {
        // Clear the IRQ then return.
        USER_SET_BIT(irqflags, TXDONE_BIT);
        uint8_t irqset_tx[2] = {address_byte, irqflags};
        sx1276_write(&hspi2, irqset_tx, SPI_MAX_TX_TIMEOUT, SPI_MAX_RETRY_DELAY);
        return true;
    }
}

/**
  * @brief  Checks if the RxDone or RxTimeout interrupt is tripped.
  * @param  ctx Chip context
  * @param  read_status SPI TX status for acquiring the RegIrqFlags register data
  * @return True if either interrupt is set and false otherwise.
  */
bool sx1276_is_rxdone_or_rxtimeout(Radio_Chip_Ctx ctx, SPI_Status *read_status)
{
    uint8_t address_byte = REG_IRQFLAGS, irqflags = 0, retries = 0;
irq_retry:
    if (sx1276_read(&hspi2, REG_IRQFLAGS, &irqflags, SPI_MAX_TX_TIMEOUT, SPI_MAX_RETRY_DELAY) != SPI_OK) {
        retries += 1;
        if (retries == SPI_MAX_RETRIES) {
            *read_status = SPI_TIMEOUT;
            // NOTE: This is a "fake" true return.
            // We return 'true' to exit the spin "lock" of waiting on this IRQ clear operatoin.
            return true;
        } else {
            goto irq_retry;
        }
    }
    *read_status = SPI_OK;

    uint8_t rxdone = USER_GET_BIT(irqflags, RXDONE_BIT);
    uint8_t rxtimeout = USER_GET_BIT(irqflags, RXTIMEOUT_BIT);
    if (rxdone || rxtimeout) {
        // Clear the both IRQs then return.
        USER_SET_BIT(irqflags, RXTIMEOUT_BIT);
        USER_SET_BIT(irqflags, RXDONE_BIT);
        uint8_t irqset_tx[2] = {address_byte, irqflags};
        sx1276_write(&hspi2, irqset_tx, SPI_MAX_TX_TIMEOUT, SPI_MAX_RETRY_DELAY);
        return true;
    }
}

/**
  * @brief Reset the SX1276. See Section 5.2.2 for more details.
  */
void sx1276_reset()
{
    // Pull Pin 7 low for 100us, then release, then delay for 5ms.
    HAL_GPIO_WritePin(CHIP_NRESET_GPIO_Port, CHIP_NRESET_Pin, GPIO_PIN_RESET);
    HAL_Delay(1); // Measured in milliseconds and we can't supply a floating point value.
    HAL_GPIO_WritePin(CHIP_NRESET_GPIO_Port, CHIP_NRESET_Pin, GPIO_PIN_SET);
    HAL_Delay(5);
}
