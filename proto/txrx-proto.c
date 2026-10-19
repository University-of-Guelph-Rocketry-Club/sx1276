#include "txrx-proto.h"

#include "crc.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint64_t packet_id = 0x67;

bool gsc_packet_create_standalone_request(packet *source, const uint8_t* recipient, satellite_commands cmd) {

    /* absolutely make sure nothing is fucked */
    if ( // something is invalid!
        source == NULL ||
        recipient == NULL ||
        cmd == 0
    ){ return false; } 

    /* fill the header! */
    source->header.version = GSC_PROTOCOL_VERSION;

    source->header.source_id = GSC_IDENTITY; // move this to board definition!
    source->header.dest_id = *recipient;

    // standalone packets come with no sequence and no expectation for follow up. indicate this.
    source->header.packet_id = packet_id; packet_id++;
    source->header.sequence_id.position = 0x0u;
    source->header.sequence_id.id = 0x0u;

    source->cmd = cmd;
    source->header.body_size = 0;
    
    // checksum is produced with the bytes at 0x0000, not truncated. 
    source->checksum = 0x0000u; // will be overwritten
    
    return true;

}

bool gsc_packet_create_standalone_response(packet *source, const uint8_t* recipient, satellite_commands cmd, packet_body* body, size_t body_size) {

    /* absolutely make sure nothing is fucked */
    if ( // something is invalid!
        source == NULL ||
        recipient == NULL ||
        cmd == 0 ||
        body == NULL
    ){ return false; } 

    /* The union has no length of its own, so the caller states how many bytes
       of body are real. sizeof(*body) is the size of the container and would
       always exceed the payload limit. */
    if ( // body exceeds size!
        body_size > GSC_MAX_PAYLOAD_SIZE
    ){ return false; } 

    /* fill the header! */
    source->header.version = GSC_PROTOCOL_VERSION;

    source->header.source_id = GSC_IDENTITY; // move this to board definition!
    source->header.dest_id = *recipient;

    // standalone packets come with no sequence and no expectation for follow up. indicate this.
    source->header.packet_id = packet_id; packet_id++;
    source->header.sequence_id.position = 0x0u;
    source->header.sequence_id.id = 0x0u;

    source->cmd = cmd;
    source->body = *body;
    source->header.body_size = (uint8_t)body_size;
    
    // checksum is produced with the bytes at 0x0000, not truncated. 
    source->checksum = 0x0000u; // will be overwritten

    return true;
}

size_t gsc_packet_encode_sloppy(const packet *input, uint8_t *output, size_t output_capacity)
{
    /* Check everything that can fail before writing to the output buffer. */
    if (input == NULL || output == NULL) {
        return 0;
    }

    if (input->header.body_size > GSC_MAX_PAYLOAD_SIZE ||
        (unsigned int)input->cmd > UINT8_MAX) {
        return 0;
    }

    size_t packet_size = GSC_PACKET_OVERHEAD + input->header.body_size;
    if (output_capacity < packet_size) {
        return 0;
    }

    /* Offset is the index of the next free byte in output. */
    size_t offset = 0;

    /* The magic string contains four characters, without the string terminator. */
    memcpy(output, GSC_MAGIC_STRING, 4);
    offset += 4;

    output[offset] = input->header.version;
    offset += 1;

    output[offset] = input->header.source_id;
    offset += 1;

    output[offset] = input->header.dest_id;
    offset += 1;

    /*
     * A 64-bit packet ID occupies eight bytes. Write its highest byte first.
     * Shifts of 56, 48, ..., 0 select those bytes from left to right.
     * For example, 0x0102030405060708 becomes 01 02 03 04 05 06 07 08.
     */
    for (int shift_bits = 56; shift_bits >= 0; shift_bits -= 8) {
        uint64_t shifted_id = input->header.packet_id >> shift_bits;
        uint8_t next_byte = (uint8_t)(shifted_id & 0xFFu);

        output[offset] = next_byte;
        offset += 1;
    }

    output[offset] = input->header.body_size;
    offset += 1;

    /* Pack the sequence ID into the upper half, and its position into the lower half. */
    uint8_t sequence_byte = (uint8_t)(input->header.sequence_id.id << 4);
    sequence_byte |= (uint8_t)input->header.sequence_id.position;
    output[offset] = sequence_byte;
    offset += 1;

    output[offset] = (uint8_t)input->cmd;
    offset += 1;

    /* Copy only the populated payload bytes, not the entire reserved array. */
    memcpy(output + offset, input->body.raw, input->header.body_size);
    offset += input->header.body_size;

    /* Cover all preceding bytes with the CRC, then append its high and low bytes. */
    offset = GSC_PROTOCOL_MAX_PACKET_SIZE-2;
    uint16_t crc = gen_crc16(output, offset);
    output[offset] = (uint8_t)(crc >> 8);
    offset += 1;

    output[offset] = (uint8_t)(crc & 0xFFu);
    offset += 1;

    return offset;
}
