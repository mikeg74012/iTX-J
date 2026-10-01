/**
 **************************************************************************************************
 * @file   serialram.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   2019-12-10
 *
 * @brief  Driver for the serial pSRAM chip
 *
 **************************************************************************************************
 * @attention                   (c) Ripcord Designs Limited 2019
 **************************************************************************************************
 */
#ifndef SERIALRAM_H
#define SERIALRAM_H

#include "main.h"
#include <stdbool.h>

/*
 **************************************************************************************************
 *                                       PUBLIC DEFINES
 **************************************************************************************************
 */
#define SRAM_SIZE_BYTES    0x00800000 // 64 Mbits


/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */
/* API */
void  SerialRamInit   (spibus_e SpiBus, spichipselect_t ChipSelect);
bool  SerialRamRead   (uint32_t address, uint8_t *pBuffer, int size);
bool  SerialRamWrite  (uint32_t address, uint8_t *pBuffer, int size);
bool  SerialRamWUST   (void);

/* Console Commands */
int   SerialRamConsoleClear (int ArgCount, char **pArguments);
int   SerialRamConsoleRead  (int ArgCount, char **pArguments);
int   SerialRamConsoleTest  (int ArgCount, char **pArguments);

#endif /* SERIALRAM_H */
