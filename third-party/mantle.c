#include "mantle.h"
#include "mantle_crc.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global packet ID to increment with each newly created packet.
uint32_t packet_id = 0x67;

bool gsc_new_packet(gsc_packet *p, const uint8_t dest, satellite_commands cmd, uint8_t *body, uint32_t body_len) {

    // If no packet or command is provided.
    if (p == NULL || cmd == 0)
        return false; 

    // Clamp body length to GSC_MAX_PAYLOAD_SIZE.
    if (body_len > GSC_MAX_PAYLOAD_SIZE)
        body_len = GSC_MAX_PAYLOAD_SIZE;

    // Fill in the packet header.
    memcpy(p->header.magic, GSC_MAGIC_STRING, 4);
    p->header.packet_id = __builtin_bswap32(packet_id); packet_id++;
    p->header.version   = GSC_PROTOCOL_VERSION;
    p->header.source_id = GSC_IDENTITY;
    p->header.dest_id   = dest;
    p->header.body_len  = body_len;

    // Fill in remaining data.
    p->cmd = cmd;
    memcpy(p->body, body, body_len);
    p->checksum = gen_crc16(body, body_len);
    
    return true;
}

size_t gsc_packet_encode(gsc_packet *input, uint8_t *output, size_t output_len)
{
    // Ensure non-NULL pointers and appropriate output size.
    if (input == NULL || output == NULL || output_len < (GSC_PACKET_OVERHEAD + input->header.body_len)) {
        return 0;
    }

    // Offset is the index of the next free byte in output.
    size_t offset = 0;

    // The magic string contains four characters, without the string terminator.
    memcpy(output, GSC_MAGIC_STRING, 4);
    offset += 4;

    // Dump the packet header.
    memcpy(output + offset, &input->header.packet_id, 4); offset += 4; 
    output[offset++] = input->header.version;
    output[offset++] = input->header.source_id;
    output[offset++] = input->header.dest_id;
    output[offset++] = input->header.body_len;

    // Dump remaining packet data then return.
    output[offset++] = (uint8_t)input->cmd;
    memcpy(output + offset, input->body, input->header.body_len);
    offset += input->header.body_len;
    memcpy(output + offset, &input->checksum, 2);
    offset += 2;
    return offset;
}

void memcpy_or_return(void *dst, void *src, size_t n)
{
    if (dst != NULL) memcpy(dst, src, n);
}

void gsc_packet_decode(void *stream, uint32_t *packet_id, uint8_t *source_id, uint8_t *dest_id, uint8_t *magic, uint8_t *body, uint16_t body_len, uint8_t *cmd, uint16_t *checksum)
{
    //
    // Copy operations are done instead of assignment (loads) since these risk unaligned loads which may cause access faults or at best performance hits.
    // Memcpy gets around this by just accessing single bytes at a time for us and copying them into variable of appropriate sizes from the right packet stream offsets.
    // See mantle.h for offset information in the header and packet structures.
    //
    
    memcpy_or_return(packet_id, stream + 4, 4);
    *packet_id = __builtin_bswap32(*packet_id);
    memcpy_or_return(magic, stream, 4);
    memcpy_or_return(source_id, stream + 9, 1);
    memcpy_or_return(dest_id, stream + 10, 1);
    memcpy_or_return(cmd, stream + 12, 1);

    // Determine which of the packet body sizes or user buffer sizes to use for copying the packet body (prefer smallest size).
    uint8_t packet_body_len = 0;
    memcpy_or_return(&packet_body_len, stream + 11, 1);
    uint8_t len = packet_body_len;
    if (packet_body_len > body_len) len = body_len;
    memcpy_or_return(body, stream + 13, len);
    memcpy_or_return(checksum, stream + 13 + len, 2);
}
