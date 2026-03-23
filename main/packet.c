/************************************************************************
 * Filename    : packet.c
 * Created on  : 2023
 * Author      : insoo.kum@hdel.co.kr
 * Description :
 * Copyright(s): HYUNDAI ELEVATOR CO,.LTD
 ************************************************************************/
#include "packet.h"

#include <stdlib.h>
#include <string.h>

void ProductPacket_Init(PRODUCT_PACKET *packet, pf_send_packet pfSendPacket, pf_packet_parsing pfParsing) {
    memset(packet, 0x00, sizeof(PRODUCT_PACKET));
    packet->pf_send = pfSendPacket;
    packet->pf_parsing = pfParsing;
}

void ProductPacket_ParserPacket(PRODUCT_PACKET *packet, uint8_t rData) {
    switch (packet->Status) {
        case PRODUCT_STATUS_STX_U:
            if (rData == PRODUCT_STX_UPPER) {
                packet->Status = PRODUCT_STATUS_STX_L;
            }
            break;
        case PRODUCT_STATUS_STX_L:
            if (rData == PRODUCT_STX_LOWER) {
                packet->Status = PRODUCT_STATUS_LEN;
            } else {
                packet->Status = PRODUCT_STATUS_STX_L;
            }
            break;
        case PRODUCT_STATUS_LEN:
            packet->Length = rData;
            memset(packet->rBuff, 0, sizeof(packet->rBuff));
            packet->rCount = 0;
            packet->Crc = 0;
            packet->Status = PRODUCT_STATUS_DATA;
            break;
        case PRODUCT_STATUS_DATA:
            packet->rBuff[packet->rCount++] = rData;
            packet->Crc ^= rData;
            if (packet->rCount >= packet->Length) {
                packet->Status = PRODUCT_STATUS_CRC;
            }
            break;
        case PRODUCT_STATUS_CRC:
            if (packet->Crc == rData) {
                packet->Status = PRODUCT_STATUS_ETX;
            } else {
                packet->Status = PRODUCT_STATUS_STX_U;
            }
            break;
        case PRODUCT_STATUS_ETX:
            if (rData == PRODUCT_ETX) {
                packet->pf_parsing();
            }
            packet->Status = PRODUCT_STATUS_STX_U;
            break;
        default:
            packet->Status = PRODUCT_STATUS_STX_U;
            break;
    }
}

void ProductPacket_PacketSendByUART(PRODUCT_PACKET *packet, uint16_t cmd, uint8_t *buff, uint8_t size) {
    uint8_t crc = 0;
    uint16_t total_len = (uint16_t)(size + 7);
    uint8_t *packet_buff = (uint8_t *)malloc(total_len);
    if (!packet_buff) {
        return;
    }

    packet_buff[0] = PRODUCT_STX_UPPER;
    packet_buff[1] = PRODUCT_STX_LOWER;
    packet_buff[2] = (uint8_t)(size + 2);
    packet_buff[3] = (uint8_t)(cmd >> 8);
    packet_buff[4] = (uint8_t)(cmd & 0xFF);

    crc = (uint8_t)(cmd >> 8);
    crc ^= (uint8_t)(cmd & 0xFF);
    for (uint16_t i = 0; i < size; i++) {
        packet_buff[5 + i] = buff[i];
        crc ^= buff[i];
    }
    packet_buff[5 + size] = crc;
    packet_buff[6 + size] = PRODUCT_ETX;

    packet->pf_send(packet_buff, total_len);
    free(packet_buff);
}
