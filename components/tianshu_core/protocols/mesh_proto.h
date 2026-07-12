#ifndef MESH_PROTO_H
#define MESH_PROTO_H

#include <stdint.h>
#include <stddef.h>

#define PROTO_HEADER_LEN 4
#define PROTO_MAX_DATA_LEN 256
#define PROTO_MAX_PACKET_LEN (PROTO_HEADER_LEN + PROTO_MAX_DATA_LEN)

typedef enum {
    PROTO_CMD_HELLO = 0x01,
    PROTO_CMD_PING = 0x02,
    PROTO_CMD_PONG = 0x03,
    PROTO_CMD_ELECTION = 0x04,
    PROTO_CMD_ELECTION_RESULT = 0x05,
    PROTO_CMD_DATA = 0x06,
    PROTO_CMD_STATUS = 0x07,
    PROTO_CMD_CONFIG = 0x08,
    PROTO_CMD_ACK = 0xFF
} proto_cmd_t;

typedef enum {
    PROTO_ERROR_NONE = 0,
    PROTO_ERROR_INVALID_HEADER = -1,
    PROTO_ERROR_INVALID_CMD = -2,
    PROTO_ERROR_INVALID_LENGTH = -3,
    PROTO_ERROR_CHECKSUM = -4,
    PROTO_ERROR_TIMEOUT = -5
} proto_error_t;

typedef struct {
    uint8_t cmd;
    uint16_t length;
    uint8_t checksum;
    uint8_t data[];
} proto_packet_t;

uint8_t proto_calculate_checksum(const uint8_t* data, size_t len);
int proto_pack(proto_cmd_t cmd, const uint8_t* data, size_t len, uint8_t* packet, size_t* packet_len);
int proto_unpack(const uint8_t* packet, size_t packet_len, proto_cmd_t* cmd, uint8_t* data, size_t* data_len);

#endif