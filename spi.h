/**
 **************************************************************************************************
 * @file   spi.h
 *
 * @author Ripcord Designs
 *
 * @brief  This module provides an abstraction layer for the on-chip SPI interface.
 *         It shares a common Ripcord API with SPI modules for other chips, so higher software
 *         layers can be easily moved between different MCU's.
 *
 **************************************************************************************************
 * @attention                  (c) Ripcord Designs Limited 2022
 **************************************************************************************************
 */
#ifndef _SPI_H_
#define _SPI_H_


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
/*
 * SPI Busses available on this chip.  The application layer
 * must use one of these bus definitions when initializing the port
 */
typedef enum
{
  SPI_BUS_0 = 0,
  SPI_BUS_1,
  SPI_BUS_2,
} spibus_e;


typedef enum
{
  SPI_MODE_0,
  SPI_MODE_1,
  SPI_MODE_2,
  SPI_MODE_3
} spimode_e;


typedef enum
{
  SPI_TRANSFER_TX,
  SPI_TRANSFER_TXRX,
  SPI_TRANSFER_TXTX
} spitransfer_e;


typedef struct
{
  uint32_t      Port;
  uint16_t      Pin;
} spichipselect_t;


/* chip select type */
typedef struct
{
  spibus_e        Bus;
  spichipselect_t ChipSelect;
  spimode_e       Mode;
  uint32_t        BitRate;
  spitransfer_e   TransferType;
} spihandle_t;


/*
 **************************************************************************************************
 *                                        GLOBAL VARIABLES
 **************************************************************************************************
 */


/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */
/* API */
bool   SpiInitialise   (unsigned long base);
bool SpiTransfer (unsigned long base, uint8_t * pBuf1, int Len1, uint8_t  * pBuf2, int Len2, spitransfer_e TransferType);

/* Console Commands */
int    SpiConsoleStats (int ArgCount, char **pArguments);
int    SpiConsoleClear (int ArgCount, char **pArguments);

#endif /* _SPI_H_ */
