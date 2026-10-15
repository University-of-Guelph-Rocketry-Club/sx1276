#include "chip_main.h"

// SX1276 entry point, called from Core/Src/main.c. Validates the SX1276 ID then configures the chip for LoRa mode and CRC generation, then halts.
Spi_Status chip_entry()
{
    //
    // Get version ID from the chip then go into LoRa + Standby mode. If ID is bad, exit immediately.
    //
  
    uint8_t address_byte = REG_VERSION;
    SET_READABLE(address_byte, 7); // Set wnr to READ (0)
    uint8_t version = 0xcc;
    HAL_StatusTypeDef ok = HAL_SPI_TransmitReceive(&hspi2, &address_byte, &version, 1*sizeof(uint8_t), SPI_MAX_TIMEOUT);
    if (version == 0x00 || version == 0xFF) {
        printf("Bad version ID: %#02x\n", version);
        return SPI_BAD_ID;
    } else {
        // Green LED on.
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
    }

    // Get the initial op mode value
    address_byte = REG_OPMODE;
    SET_READABLE(address_byte); // Set wnr to READ (0)
    uint8_t op_mode = 0;
    while (1) {
        ok = HAL_SPI_TransmitReceive(&hspi2, &address_byte, &op_mode, 1*sizeof(uint8_t), SPI_MAX_TIMEOUT);
        if (ok == HAL_OK) break;
        HAL_Delay(SPI_TX_DELAY);
    }
    printf("Got op mode: %#08x\n", op_mode);

    // Go into LoRa mode by first entering SLEEP mode then setting the 7th bit (MSB) of the RegOpMode register (0x01). See Section 4.4 of the datasheet.
    uint8_t sleep_data   = op_mode & 0b11111000;   // Last 3 bits are 000 to go into SLEEP mode.
    uint8_t lora_data    = sleep_data | (1<<7);    // Set MSB to 1 to go into LoRa mode.
    uint8_t standby_data = lora_data & 0b11111001; // Go back into STDBY (001) mode.
    
    // Set the address for RegOpMode with write permissions.
    address_byte = REG_OPMODE;
    SET_WRITEABLE(address_byte);
    uint8_t sleep_tx[2]  = {address_byte, sleep_data}
    uint8_t lora_tx[2]   = {address_byte, lora_data};
    uint8_t stdby_tx[2]  = {address_byte, standby_data};

    // Go into SLEEP mode first to *then* go into LoRa mode.
    ok = HAL_SPI_Transmit(&hspi2, sleep_tx, 2*sizeof(uint8_t), SPI_MAX_TIMEOUT); 
    if (ok != HAL_OK) return SPI_GENERAL_ERROR;
    ok = HAL_SPI_Transmit(&hspi2, lora_tx, 2*sizeof(uint8_t), SPI_MAX_TIMEOUT); 
    if (ok != HAL_OK) return SPI_GENERAL_ERROR;
  
    // Set op mode to standby again, but now with LoRa on.
    ok = HAL_SPI_Transmit(&hspi2, stdby_tx, 2*sizeof(uint8_t), SPI_MAX_TIMEOUT); 
    if (ok != HAL_OK) return SPI_GENERAL_ERROR;
    printf("In LoRa mode, standing by: op mode: %#08x\n", standby_data);

    //
    // NOTE: Frequency is already set at 434MHz by default, so we won't even change it.
    //

    // Finished, halt.
    while (1) {}
}

// Transmit a LoRa packet 'p' over the wire.
void chip_transmit_packet(LoRa_Packet p)
{
    // NOTE: After sending the packet, we are put into standby mode.
    // Steps (Section 4.1.6):
    // 1. Tx Init
    // 2. Write data to FIFO buffer over SPI
    //  2.1. Set FifoPtrAddr to FifoTxPtrBase
    //  2.2. Write PayloadLength bytes to RegFifo
    // 3. Go into TX mode
    // 4. Wait until IRQ TxDone
}

// Receive a LoRa packet from another radio over the wire.
LoRa_Packet chip_receive_packet(LoRa_Packet p)
{
    // NOTE: After receiving the packet, we are put into standby mode.
    // Steps (Section 4.1.6):
    // 1. Rx Init
    //  1.1. Set FifoAddrPtr to FifoRxBaseAddr
    // 2. (Assuming single request) Go into Rx Single mode
    // 3. Wait for RxDone or RxTimeout IRQ
    //  3.1. If RxDone, check CRC (PayloadCrcError IRQ)
    //   3.1.1. If no error, then read Rx Data in FIFO buffer
    //    3.1.1.1. Ensure that ValidHeader, PayloadCrcError, RxDone, and RxTimeout in the RegIrqFlags register are all lowered.
    //    3.1.1.2. If no errors, then set RegFifoAddrPtr to RegFifoRxCurrentAddr, then read RegFifo over SPI, RegRxNbBytes times (number of bytes received).
    //    - NOTE: Use a 256-byte long buffer and copy the data over. TODO: Maybe add this reception buffer to the chip config struct?
    //  3.2. If RxTimeout, clear the interrupt by writing '1' to the RegIrqFlags register (0x12, 7th bit) then exit.
}

// Check validity of CRC hash on packet reception through the PayloadCrcError Irq flag (Section 4.1.1.6).
bool chip_is_last_packet_valid()
{
    // TODO: Check CRC hash inside of packed payload through MANTLE protocol
}

// Reset the SX1276. See Section 5.2.2 for more details.
void chip_reset()
{
    // Pull Pin 7 low for 100us, then release, then delay for 5ms.
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
    HAL_Delay(1); // Measured in milliseconds and we can't supply a floating point value.
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
    HAL_Delay(5);
}
