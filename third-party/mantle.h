#ifndef MANTLE_H
#define MANTLE_H

#include "mantle_crc.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#define GSC_PROTOCOL_VERSION 1u
#define GSC_MAGIC_STRING     "GSRP" // guelph space research protocol!
#define GSC_MAX_PACKET_SIZE  128u
#define GSC_COMMAND_SIZE     1u
#define GSC_CRC_SIZE         2u
#define GSC_HEADER_SIZE      12u
#define GSC_PACKET_OVERHEAD  (GSC_HEADER_SIZE + GSC_CRC_SIZE + GSC_COMMAND_SIZE)
#define GSC_MAX_PAYLOAD_SIZE (GSC_MAX_PACKET_SIZE - GSC_PACKET_OVERHEAD)

// Identity ID which must be set at build time by the user!
#ifndef GSC_IDENTITY
#define GSC_IDENTITY     0xFFu
#endif
#ifndef GSC_BROADCAST_ID
#define GSC_BROADCAST_ID 0xFFu
#endif

// Packed packet header structure.
typedef struct __attribute__((packed)) {
    uint8_t magic[4];   // "GSRP" magic string. Offset 0
    uint32_t packet_id; // Auto-incremented big-endianne packet ID. Offset 4
    uint8_t version;    // Protocol version. Offset 8
    uint8_t source_id;  // Recipient ID. Offset 9
    uint8_t dest_id;    // Destination ID. Offset 10
    uint8_t body_len;   // Length of packet body that is being sent. Offset 11
} gsc_header;
static_assert(sizeof(gsc_header) == 12, "Packet header is not packed to 64 bits!");

// Satellite commands to send in packets. 
typedef enum __attribute__((packed)) {
    GSC_REQUEST_HEARTBEAT = 0x01,
    GSC_REQUEST_STATE_SYSTEM = 0x02,
    GSC_REQUEST_STATE_FLIGHT = 0x03,

    GSC_REQUEST_GPS = 0x10,
    GSC_REQUEST_ENVIRONMENT = 0x11,
    GSC_REQUEST_MOTION = 0x12,

    // below are used when uplink can be established.
    GSC_RAW_REQUEST = 0xF0,
    GSC_RAW_DATA = 0xF1,
    GSC_RAW_DATA_RESPONSE = 0xF2,
    GSC_ASSIGN_SEQUENCE = 0xF3,
    GSC_ACK_GO_AHEAD = 0xFE,
    GSC_ACK_FIN = 0xFF
} satellite_commands;
static_assert(sizeof(satellite_commands) == 1, "Satellite commands size has changed and/or does not equal 1 (byte).");

// Full MANTLE packet structure.
typedef struct __attribute__((packed)) {
    gsc_header header;                      // 12-byte packed packet header. Offset 0
    satellite_commands cmd;                 // 1-byte satellite command. Offset 12 
    uint8_t body[GSC_MAX_PAYLOAD_SIZE];     // Raw packet data. Offset 13
    uint16_t checksum;                      // 2-byte big-endianne CRC hash. Offset 13 + GSC_MAX_PAYLOAD_SIZE
} gsc_packet;
static_assert(sizeof(gsc_packet) <= GSC_MAX_PACKET_SIZE, "Packet size is too large to be transmitted!");

/**
  * @brief  Create a new empty packet with a specified destination ID and satellite command.
  * @param  p Pointer to a packet to be filled
  * @param  dest Destination ID value
  * @param  cmd Satellite command to pack into the packet body (first byte)
  * @param  body Packet body to be sent
  * @param  body_len Length of the packet body to be sent. Will be clamped to GSC_MAX_PAYLOAD_SIZE if too large!
  * @return Whether or not the packet was created successfuly. False if packet pointer is NULL, cmd = 0, or body is too big to fit in the packet structure.
  */
bool gsc_new_packet(gsc_packet *p, const uint8_t dest, satellite_commands cmd, uint8_t *body, uint32_t body_len);

/**
  * @brief Encode packet data into a byte buffer.
  * @param p Packet to encode
  * @param output Buffer to output data to
  * @param output_len Length of the output buffer
  * @return Number of bytes written to the output buffer
  */
size_t gsc_packet_encode(gsc_packet *p, uint8_t *output, size_t output_len);

/**
  * @brief Decode packet byte stream into separate variables through `memcpy`ing them. NULL pointers will result in variables being ignored.
  * @param stream Packet byte stream to decode
  * @param packet_id Pointer to a uint32_t to store the packet ID, returned as little-endianne
  * @param source_id Pointer to a uint8_t to store the packet source ID
  * @param dest_id Pointer to a uint8_t to store the dest ID
  * @param magic Pointer to a buffer to store the packet magic string
  * @param body Pointer to a buffer to store the packet body
  * @param body_len Length of the buffer to store the packet body
  * @param cmd Pointer to a uint8_t to store the satellite command
  * @param checksum Pointer to a uint16_t to store the checksum, returned as little-endianne
  */
void gsc_packet_decode(void *stream, uint32_t *packet_id, uint8_t *source_id, uint8_t *dest_id, uint8_t *magic, uint8_t *body, uint16_t body_len, uint8_t *cmd, uint16_t *checksum);

#endif // MANTLE_H
