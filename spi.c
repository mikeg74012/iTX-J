/**
 **************************************************************************************************
 * @file   spi.c
 *
 * @author Ripcord Designs
 *
 * @brief  This module provides an abstraction layer for the GD32 on-chip SPI interface.
 *         It shares a common Ripcord API with SPI modules for other chips, so higher software
 *         layers can be easily moved between different MCU's.
 *
 **************************************************************************************************
 * @attention                  (c) Ripcord Designs Limited 2022
 **************************************************************************************************
 */
#include "global.h"


/*
 **************************************************************************************************
 *                                     PUBLIC VARIABLES
 **************************************************************************************************
 */


/*
 **************************************************************************************************
 *                                       PRIVATE DEFINES
 **************************************************************************************************
 */

/* SPI transaction timeout */
#define SPI_TIMEOUT_MS          500


/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static uint8_t SpiWriteByte           (unsigned long base, uint8_t byte);


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */


/*
 **************************************************************************************************
 *                                   PUBLIC SECTION / INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       Initialise the SPI module.  Must be performed at start up.
 * @param[in]   pSpiHandle : SPI handle to configure
 * @return      true on success, false on failure
 */
bool SpiInitialise (unsigned long base)
{
    /* set up the  SPI hardware settings */

    if (base == ACCEL_SPI)
    {
        set_gpio_high (SPI_CS_SRAM);
        set_gpio_output (SPI_CS_SRAM);
    }
    else
    {
        set_gpio_high (SPI_CS_IMU);
        set_gpio_output (SPI_CS_IMU);
    }

    switch (base)
    {
        case SPI0:
            or_register (RCU, RCU_APB2EN, (1 << 12));
            
            set_gpio_alternate (SPI0_MISO);
            set_gpio_alternate (SPI0_MOSI);
            set_gpio_alternate (SPI0_SCK);
                
            set_gpio_high_speed (SPI0_MISO);
            set_gpio_high_speed (SPI0_MOSI);
            set_gpio_high_speed (SPI0_SCK);
            
            /* Remap SPI0 to the correct I/O pins and disable JTAG so we can reuse those pins*/
            
            write_register (AFIO, AFIO_PCF0, (read_register (AFIO, AFIO_PCF0) & ~(7 << 24)) | (1 << 0) | (2 << 24));
            
            break;
            
        case SPI1:
            or_register (RCU, RCU_APB1EN, (1 << 14));
            
            set_gpio_alternate (SPI1_MISO);
            set_gpio_alternate (SPI1_MOSI);
            set_gpio_alternate (SPI1_SCK);
                
            set_gpio_high_speed (SPI1_MISO);
            set_gpio_high_speed (SPI1_MOSI);
            set_gpio_high_speed (SPI1_SCK);
            
            break;
    }

    or_register (RCU, RCU_APB2EN, 1 << 0);  /* Enable AF */

    /* TODO make sure SPI is in the correct mode here.  I think it is */

    /* Set PCLK/4 */
    
    or_register (base, SPI_CTL0, (1 << 3));
    
    /* SPI enable */
    
    or_register (base, SPI_CTL0, (1 << 6));
    
    return true;
}




/**
 * @brief       Send and / or receive data over the SPI bus.
 * @notes       Caller MUST set the transfer type in the SpiHandle (TX, TXRX or TXTX)
 *
 * @param[in]   pSpiHandle : SPI handle to configure
 * @param[in]   pBuf1 : The transmit data buffer
 * @param[in]   Len1  : The transmit data length
 * @param[in]   pBuf2 : The receive OR second transmit data buffer (depends on transfer type TXRX or TXTX)
 * @param[in]   Len2  : The receive OR second transmit data length (depends on transfer type TXRX or TXTX)
 * @return      true on success, false on failure
 */
bool SpiTransfer (unsigned long base, uint8_t * pBuf1, int Len1, uint8_t  * pBuf2, int Len2, spitransfer_e TransferType)
{
  bool StatusOK = true;

    /* Activate the Chip Select */
  
    if (base == ACCEL_SPI)
    {
        set_gpio_low (SPI_CS_IMU);
    }
    else
    {
        set_gpio_low (SPI_CS_SRAM);
    }
    
    /* Perform the SPI transmit */
    if ((NULL != pBuf1) && (Len1))
    {
	  for (int i = 0; i < Len1; i++)
	  {
		SpiWriteByte(base, *pBuf1);
		pBuf1++;
	  }
    }

    /* Perform a second SPI transmit if requested */
    if (SPI_TRANSFER_TXTX == TransferType)
    {
      if ((NULL != pBuf2) && (Len2))
      {
        for (int i = 0; i < Len2; i++)
        {
          SpiWriteByte(base, *pBuf2);
          pBuf2++;
        }
      }
    }

    /* Perform a SPI receive if requested */
    if (SPI_TRANSFER_TXRX == TransferType)
    {
      if ((NULL != pBuf2) && (Len2))
      {
        for (int i = 0; i < Len2; i++)
        {
          *pBuf2 = SpiWriteByte(base, 0);
          pBuf2++;
        }
      }
    }
  
    /* Deactivate Chip Select */
    
    if (base == ACCEL_SPI)
    {
        set_gpio_high (SPI_CS_IMU);
    }
    else
    {
        set_gpio_high(SPI_CS_SRAM);
    }
  
  return (StatusOK);
}





/**
* @brief        send a byte through the SPI interface and return the byte received from the SPI bus
*
* @warning      SPI port is ready and CS is active
*
* @param[in]    SPI_REG: SPI port register
* @param[in]    byte: byte to send
* @return       {the value of the received byte}
*/
static uint8_t SpiWriteByte (unsigned long base, uint8_t byte)
{
  /* loop while data register in not empty */
    
    while (!(read_register (base, SPI_STAT) & (1 << 1)))
    {
    }
    
  /* send byte through the SPI peripheral */

    write_register (base, SPI_DATA, byte);
    
  /* wait to receive a byte */
    
    while (!(read_register (base, SPI_STAT) & (1 << 0)))
    {
    }

  /* return the byte read from the SPI bus */
  return(read_register (base, SPI_DATA));
}






