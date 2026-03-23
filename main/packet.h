/************************************************************************
 * Filename    : packet.h
 * Created on  : 2023
 * Author      : insoo.kum@hdel.co.kr
 * Description :
 * Copyright(s): HYUNDAI ELEVATOR CO,.LTD
 ************************************************************************/
#ifndef __PACKET_H__
#define __PACKET_H__

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define PROTOCOL_ERROR                0
#define PROTOCOL_SUCCESS              1

#define CONVERT_2BYTES_UINT(a,b)      ((a) << 8) | (b)
#define CONVERT_UINT_2BYTES(a,b,c)    {b = a >> 8; c = a & 0xFF;}
#define SWAP_UINT16(x)                (((x) >> 8) | ((x) << 8))
#define SWAP_UINT32(x)                (((x) >> 24) | (((x) & 0x00FF0000) >> 8) | (((x) & 0x0000FF00) << 8) | ((x) << 24))

#define PRODUCT_STX_UPPER             0xFF
#define PRODUCT_STX_LOWER             0xFE
#define PRODUCT_ETX                   0x03

#define PRODUCT_COMM_BUFF_SIZE        200

enum {
    PRODUCT_STATUS_STX_U = 0,
    PRODUCT_STATUS_STX_L,
    PRODUCT_STATUS_LEN,
    PRODUCT_STATUS_DATA,
    PRODUCT_STATUS_CRC,
    PRODUCT_STATUS_ETX,
    PRODUCT_STATUS_MAX,
};

typedef void (*pf_send_packet)(uint8_t *pbuff, uint16_t size);
typedef void (*pf_packet_parsing)(void);

#pragma pack(push, 1)
typedef struct __PRODUCT_PACKET {
    uint8_t rBuff[PRODUCT_COMM_BUFF_SIZE];
    uint8_t Status;
    uint8_t Length;
    uint8_t rCount;
    uint8_t Crc;
    pf_send_packet pf_send;
    pf_packet_parsing pf_parsing;
    void (*packetParser)(uint8_t data);
    void (*write_packet)(uint16_t cmd, uint8_t *buff, uint8_t size);
} PRODUCT_PACKET;
#pragma pack(pop)

void ProductPacket_Init(PRODUCT_PACKET *packet, pf_send_packet pfSendPacket, pf_packet_parsing pfParsing);
void ProductPacket_ParserPacket(PRODUCT_PACKET *packet, uint8_t rData);
void ProductPacket_PacketSendByUART(PRODUCT_PACKET *packet, uint16_t cmd, uint8_t *buff, uint8_t size);

#endif
