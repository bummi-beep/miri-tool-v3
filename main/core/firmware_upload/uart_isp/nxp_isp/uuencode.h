/*
 * nxp_partdesc.h
 *
 *  Created on: 2025. 1. 17.
 *      Author: 82102
 */

#ifndef ISP_NXP_UUENCODE_H_
#define ISP_NXP_UUENCODE_H_


void uuencode(const unsigned char *src, unsigned char *dest, unsigned int nmemb);
int uudecode(const char *src, unsigned char *dest);

#endif /*ISP_NXP_UUENCODE_H_ */
