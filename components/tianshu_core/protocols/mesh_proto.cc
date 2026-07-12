#include "mesh_proto.h"
#include <string.h>

uint8_t proto_calculate_checksum(const uint8_t* data, size_t len) {
    uint8_t checksum = 0;
    for (size_t i = 0; i < len; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

int proto_pack(proto_cmd_t cmd, const uint8_t* data, size_t len, uint8_t* packet, size_t* packet_len) {
    if (!packet || !packet_len || len > PROTO_MAX_DATA_LEN) {
        return PROTO_ERROR_INVALID_LENGTH;
    }
    
    proto_packet_t* pkt = (proto_packet_t*)packet;
    pkt->cmd = cmd;
    pkt->length = len;
    
    if (data && len > 0) {
        memcpy(pkt->data, data, len);
    }
    
    pkt->checksum = proto_calculate_checksum(packet, PROTO_HEADER_LEN - 1 + len);
    *packet_len = PROTO_HEADER_LEN + len;
    
    return PROTO_ERROR_NONE;
}

int proto_unpack(const uint8_t* packet, size_t packet_len, proto_cmd_t* cmd, uint8_t* data, size_t* data_len) {
    if (!packet || !cmd || !data || !data_len) {
        return PROTO_ERROR_INVALID_HEADER;
    }
    
    if (packet_len < PROTO_HEADER_LEN) {
        return PROTO_ERROR_INVALID_LENGTH;
    }
    
    proto_packet_t* pkt = (proto_packet_t*)packet;
    uint8_t expected_checksum = proto_calculate_checksum(packet, PROTO_HEADER_LEN - 1 + pkt->length);
    
    if (pkt->checksum != expected_checksum) {
        return PROTO_ERROR_CHECKSUM;
    }
    
    *cmd = (proto_cmd_t)pkt->cmd;
    *data_len = pkt->length;
    
    if (pkt->length > 0 && data) {
        memcpy(data, pkt->data, pkt->length);
    }
    
    return PROTO_ERROR_NONE;
}