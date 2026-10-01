/**
 **************************************************************************************************
 * @file   crcdriver.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   25/03/2020
 *
 * @brief  CRC Driver
 *
 **************************************************************************************************
 * @attention                      (c) Ripcord Designs Limited 2022
 **************************************************************************************************
 */

#ifndef CRCDRIVER_H
#define CRCDRIVER_H

#include "main.h"
#include <stdbool.h>


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
/* API */
void     CrcDriverInitialise          (void);
uint16_t CrcDriverCalculateCrc16      (uint8_t * pBuffer, uint32_t length);
uint32_t CrcDriverCalculateCrc32      (uint8_t * pBuffer, uint32_t length);
void     CrcDriverInitCrc32Multi      (void);
uint32_t CrcDriverCalculateCrc32Multi (uint8_t * pBuffer, uint32_t length);
uint32_t CrcDriverFinishCrc32Multi    (uint32_t crc);

#endif // CRCDRIVER_H
