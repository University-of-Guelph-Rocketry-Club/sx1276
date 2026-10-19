#ifndef TXRX_PROTO_H
#define TXRX_PROTO_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// FIXME: Get rid of this so we can compile the driver!!!!!!!!!!!
#include "body/heartbeat.h"

#define GSC_PROTOCOL_VERSION 1u
#define GSC_MAGIC_STRING "GSRP" // guelph space research protocol!
#define GSC_PROTOCOL_MAX_PACKET_SIZE 128u
#define GSC_HEADER_SIZE (4u + 1u + 1u + 1u + 8u + 1u + 1u)
#define GSC_COMMAND_SIZE 1u
#define GSC_PACKET_CRC_SIZE 2u

#define GSC_PACKET_OVERHEAD \
    (GSC_HEADER_SIZE + GSC_COMMAND_SIZE + GSC_PACKET_CRC_SIZE)

#define GSC_MAX_PAYLOAD_SIZE \
    (GSC_PROTOCOL_MAX_PACKET_SIZE - GSC_PACKET_OVERHEAD)

#define PACKET_MAX_PAYLOAD GSC_MAX_PAYLOAD_SIZE

// Define YOUR identity here. 0xFF is a placeholder, change in production.
#define GSC_IDENTITY 0xFFu
#define GSC_BROADCAST_ID 0xFFu

typedef struct {
    unsigned int id : 4;
    unsigned int position : 4;
} sequenceIdentifier;

typedef struct {
    uint8_t version;
    uint8_t source_id;
    uint8_t dest_id; // One-byte recipient; GSC_BROADCAST_ID addresses everyone.
    uint64_t packet_id;
    uint8_t body_size;
    sequenceIdentifier sequence_id;
} gsc_header;

typedef enum {
    GSC_REQUEST_HEARTBEAT = 0x01,
    GSC_REQUEST_STATE_SYSTEM = 0x02,
    GSC_REQUEST_STATE_FLIGHT = 0x03,

    GSC_REQUEST_GPS = 0x10,
    GSC_REQUEST_ENVIRONMENT = 0x11,
    // below are used when uplink can be established.
    GSC_RAW_REQUEST = 0xF0,
    GSC_RAW_DATA = 0xF1,
    GSC_RAW_DATA_RESPONSE = 0xF2,
    GSC_ACK_GO_AHEAD = 0xFE,
    GSC_ACK_FIN = 0xFF
} satellite_commands;

typedef union {
    uint8_t raw[GSC_MAX_PAYLOAD_SIZE];   // reserves the size, gives byte access
    heartbeatBody heartbeat;
} packet_body;

typedef struct {
    gsc_header header;
    satellite_commands cmd;
    uint16_t checksum; // 16 bit CRC checksum
    packet_body body;
} packet;

/* Create an empty packet with the protocol version, local identity, and recipient set. */
packet gsc_packet_init_sloppy(uint8_t recipient);

bool gsc_packet_create_standalone_request(packet *source, const uint8_t* recipient, satellite_commands cmd);
bool gsc_packet_create_standalone_response(packet *source, const uint8_t* recipient, satellite_commands cmd, packet_body* body, size_t body_size);


/*
 * Encode into a separate buffer; input and output must not overlap.
 * Returns bytes written, or zero for invalid arguments or insufficient space.
 * On failure, output is unchanged. The CRC is calculated, not read from input.
 */
size_t gsc_packet_encode_sloppy(const packet *input, uint8_t *output, size_t output_capacity);

#endif // TXRX_PROTO_H
