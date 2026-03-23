/*
 * nxp_partdesc.h
 *
 *  Created on: 2025. 1. 17.
 *      Author: 82102
 */

#ifndef APP_ESP32_MAIN_OPERATION_FIRMWARE_UPDATE_TARGET_NXP_ISP_NXP_PARTDESC_H_
#define APP_ESP32_MAIN_OPERATION_FIRMWARE_UPDATE_TARGET_NXP_ISP_NXP_PARTDESC_H_

#include "lpcisp.h"


void ReportPartInfo(int level, partinfo_t *p);
int GetPartInfo(int fd, lpcispcfg_t *cfg, partinfo_t *p);
void DumpPartList(FILE *fp);


#if 0
// device configuration is held as a structure to allow low level functions to be thread-safe
// and not share any persistent variables.
typedef struct
{
    int
        resetPin,
        ispPin,
        echo;
    unsigned char
        lineTermination;
}lpcispcfg_t;

typedef struct
{
    unsigned int
        bank,       // bank number
        base,       // base address
        size;       // size in bytes
}sectormap_t;

typedef struct
{
    // these are definitions which are set by the part table in partdesc.c
    unsigned int
        id[2],              // some parts have more than one ID.  set the second to ~0 if unused.
        id1;                // some parts have a second ID word.  set to ~0 if unused.
    const char
        *name;
    const int
        numSectors;
    const sectormap_t
        *sectorMap;
    const int
        numBanks,
        ramSize,
        flashBlockSize,     // number of bytes to write at once.  depends on available RAM.
        flashBlockRAMBase;  // start address in RAM of buffer to use for writing to flash
    const unsigned char
        flags;
    // these are variables which are read from the target device and filled in
    unsigned int
        uid[4];
    unsigned char
        bootMajor,
        bootMinor;
    int
        idIdx;
}partinfo_t;

// b0   = 1 if data is uuencoded, 0 if not
// b2:1 = expected line termination (00=any, 01=CR, 10=LF, 11=CRLF)
// b3   = 1 if device has a UID, 0 if not
// b4   = 1 if device remaps the first 64 bytes (vectors) in ISP mode
// b5   = 1 if device remaps the first 256 bytes (vectors) in ISP mode
// b6   = 1 if device cannot read the first word of flash (LPC546xx, possibly others)

#define UUENCODE        (1<<0)  // set if part expects data to be uuencoded and checksummed
#define TERM_ANY        (0<<1)  // default, no flags set
#define TERM_CR         (1<<1)
#define TERM_LF         (2<<1)
#define TERM_CRLF       (3<<1)
#define TERM_MASK       (3<<1)
#define HAS_UID         (1<<3)  // device has a UID, supports UID command
#define VECT_REMAP64    (1<<4)  // set true if device remaps the first 64 bytes (vectors) in ISP mode, thus making that section un-verifiable
#define VECT_REMAP256   (1<<5)  // set true if device remaps the first 256 bytes (vectors) in ISP mode, thus making that section un-verifiable
#define SKIP_0          (1<<6)  // when reading skip first word of flash
#endif

#endif /* APP_ESP32_MAIN_OPERATION_FIRMWARE_UPDATE_TARGET_NXP_ISP_NXP_PARTDESC_H_ */
