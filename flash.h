/**
 **************************************************************************************************
 * @file   flash.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  On-chip FLASH memory module for GD32.  Used to erase and write to the on-chip FLASH.
 *         It shares a common Ripcord API with SPI modules for other chips, so higher software
 *         layers can be easily moved between different MCU's.
 *
 **************************************************************************************************
 * @attention                      (c) Ripcord Designs Limited 2022
 **************************************************************************************************
 */
#ifndef _FLASH_H_
#define _FLASH_H_


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
#define FLASH_PAGE_SIZE     0x800


/*
 **************************************************************************************************
 *                                        PUBLIC PROTOTYPES
 **************************************************************************************************
 */
/* API */
bool     FlashInit       (void);
bool     FlashErase      (uint32_t PageAddress, uint32_t NoOfPages);
bool     FlashWrite      (uint32_t Address, uint8_t * pData, uint32_t Length);

#endif /* _FLASH_H_ */
