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
static bool    SpiConfigureChipSelect (spihandle_t * pSpiHandle);
static uint8_t SpiWriteByte           (uint32_t SPI_REG, uint8_t byte);
static uint8_t SpiReadByte            (uint32_t SPI_REG);


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
    switch (base)
    {
        case SPI0:
            or_register (RCU, RCU_APB2EN, (1 << 12));
            
            set_gpio_a
            gpio_init(SPI0_SCK_PORT,  GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, SPI0_SCK_PIN);
            gpio_init(SPI0_MOSI_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, SPI0_MOSI_PIN);
            gpio_init(SPI0_MISO_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, SPI0_MISO_PIN);
            gpio_pin_remap_config(GPIO_SPI0_REMAP, ENABLE);

            /* Remap SWO pin to GPIO so we can use it as SPI CLK */
            gpio_pin_remap_config(GPIO_SWJ_SWDPENABLE_REMAP, ENABLE); /* for the SWO PB3 remap */
            
            break;
            
        case SPI1:
            rcu_periph_clock_enable(RCU_SPI1);
            rcu_periph_clock_enable(RCU_GPIOB); // TODO - fix this.  Need to use the Chip Support GPIO module instead
            gpio_init(SPI1_SCK_PORT,  GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, SPI1_SCK_PIN);
            gpio_init(SPI1_MOSI_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, SPI1_MOSI_PIN);
            gpio_init(SPI1_MISO_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, SPI1_MISO_PIN);
            
            break;
    }

    if (pBus->SpiInstance)
    {
      rcu_periph_clock_enable(RCU_AF);

      /* store the basic bus configuration */
      pBus->SpiConfig.device_mode = SPI_MASTER;
      pBus->SpiConfig.trans_mode = SPI_TRANSMODE_FULLDUPLEX;
      pBus->SpiConfig.frame_size = SPI_FRAMESIZE_8BIT;
      pBus->SpiConfig.nss = SPI_NSS_SOFT;
      pBus->SpiConfig.endian = SPI_ENDIAN_MSB;

      /* Other settings for this bus */
      spi_crc_off(pBus->SpiInstance);   /* crc off */
      spi_ti_mode_disable(pBus->SpiInstance);
      spi_nssp_mode_disable(pBus->SpiInstance);

      /* initialise Chip Select GPIO pins */
      gpio_init(pSpiHandle->ChipSelect.Port , GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, pSpiHandle->ChipSelect.Pin);
      gpio_bit_set(pSpiHandle->ChipSelect.Port, pSpiHandle->ChipSelect.Pin);

      spi_enable(pBus->SpiInstance);

      pBus->Initialized = true;
    }
  }
  return true;
}


/**
 * @brief       De-initialise one SPI bus.
 * @param[in]   pSpiHandle : SPI handle to de-init
 * @return      true on success, false on failure
 */
bool SpiDeInitialise (spihandle_t * pSpiHandle)
{
  spi_bus_t *pBus = &Spi[pSpiHandle->Bus];

  /* disable and de-initialise the SPI bus */
  spi_disable(pBus->SpiInstance);
  spi_i2s_deinit(pBus->SpiInstance);

  /* mark bus as not initialised */
  pBus->Initialized = false;

  /* no feedback from GD32 HW layer, so always return true */
  return (true);
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
bool SpiTransfer (spihandle_t * pSpiHandle, uint8_t * pBuf1, int Len1, uint8_t  * pBuf2, int Len2)
{
  bool StatusOK = true;

  /* Lock the SPI bus */
  if (taskSCHEDULER_NOT_STARTED != xTaskGetSchedulerState())
  {
    StatusOK = (pdTRUE == xSemaphoreTake(Spi[pSpiHandle->Bus].SpiBusMutex, portMAX_DELAY));
  }

  if (StatusOK)
  {
    /* Configure the bus for this chip select (mode and bitrate) */
    StatusOK = SpiConfigureChipSelect(pSpiHandle);

    /* Activate the Chip Select */
    gpio_bit_write(pSpiHandle->ChipSelect.Port, pSpiHandle->ChipSelect.Pin, RESET);

    /* Perform the SPI transmit */
    if (StatusOK && (NULL != pBuf1) && (Len1))
    {
	  for (int i = 0; i < Len1; i++)
	  {
		SpiWriteByte(Spi[pSpiHandle->Bus].SpiInstance, *pBuf1);
		pBuf1++;
	  }
    }

    /* Perform a second SPI transmit if requested */
    if (StatusOK && (SPI_TRANSFER_TXTX == pSpiHandle->TransferType))
    {
      if ((NULL != pBuf2) && (Len2))
      {
        for (int i = 0; i < Len2; i++)
        {
          SpiWriteByte(Spi[pSpiHandle->Bus].SpiInstance, *pBuf2);
          pBuf2++;
        }
      }
    }

    /* Perform a SPI receive if requested */
    if (StatusOK && (SPI_TRANSFER_TXRX == pSpiHandle->TransferType))
    {
      if ((NULL != pBuf2) && (Len2))
      {
        for (int i = 0; i < Len2; i++)
        {
          *pBuf2 = SpiReadByte(Spi[pSpiHandle->Bus].SpiInstance);
          pBuf2++;
        }
      }
    }
  
    /* Deactivate Chip Select */
    gpio_bit_write(pSpiHandle->ChipSelect.Port, pSpiHandle->ChipSelect.Pin, SET);
  }
  
  /* Unlock the SPI bus */
  if (taskSCHEDULER_NOT_STARTED != xTaskGetSchedulerState())
  {
    xSemaphoreGive(Spi[pSpiHandle->Bus].SpiBusMutex);
  }
  
  return (StatusOK);
}




/*
 **************************************************************************************************
 *                                         PRIVATE SECTION
 **************************************************************************************************
 */
/**
 * @brief       Configure a specific SPI chip select for bus mode and bit rate.
 * @note        Must be Performed before every SPI transfer, because each chip select can have different
 *              requirements (SPI mode + bit rate)
 * @param[in]   TBC
 * @return      TBC
 */
static bool SpiConfigureChipSelect (spihandle_t * pSpiHandle)
{
  spi_bus_t *pBus = &Spi[pSpiHandle->Bus];

  /* Set bit rate */
  pBus->SpiConfig.prescale = SPI_PSC_4;  // Currently fixed.  Should be calculated in future
  
  /* set mode */
  switch (pSpiHandle->Mode)
  {
  case SPI_MODE_0:
	pBus->SpiConfig.clock_polarity_phase = SPI_CK_PL_LOW_PH_1EDGE;
    break;
  case SPI_MODE_1:
	pBus->SpiConfig.clock_polarity_phase = SPI_CK_PL_LOW_PH_2EDGE;
    break;
  case SPI_MODE_2:
	pBus->SpiConfig.clock_polarity_phase = SPI_CK_PL_HIGH_PH_1EDGE;
    break;
  case SPI_MODE_3:
	pBus->SpiConfig.clock_polarity_phase = SPI_CK_PL_HIGH_PH_2EDGE;
    break;
  }

  /* initialise the bus */
  spi_init(pBus->SpiInstance, &pBus->SpiConfig);

  /* no feedback from the spi-driver on GD32, so always return true */
  return true;
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
static uint8_t SpiWriteByte (uint32_t SPI_REG, uint8_t byte)
{
  /* loop while data register in not empty */
  while (RESET == spi_i2s_flag_get(SPI_REG, SPI_FLAG_TBE));

  /* send byte through the SPI peripheral */
  spi_i2s_data_transmit(SPI_REG, byte);
  while (spi_i2s_flag_get(SPI_REG, SPI_FLAG_TRANS));

  /* wait to receive a byte */
  while (RESET == spi_i2s_flag_get(SPI_REG,SPI_FLAG_RBNE));

  /* return the byte read from the SPI bus */
  return(spi_i2s_data_receive(SPI_REG));
}


/**
* @brief        read a byte from SPI interface and return the byte received from the SPI bus
*
* @warning      call when SPI port is ready and CS is active
*
* @param[in]    SPI_REG: SPI port register
* @return       {the value of the received byte}
*/
#if 0
static uint8_t SpiReadByte (uint32_t SPI_REG)
{
  uint8_t data;
  
  /* wait to receive a byte */
  while (RESET == spi_i2s_flag_get(SPI_REG, SPI_FLAG_RBNE));

  /* read a byte from the SPI bus */
  data = spi_i2s_data_receive(SPI_REG);
  while (spi_i2s_flag_get(SPI_REG, SPI_FLAG_TRANS));

  /* return the byte read from the SPI bus */
  return data;
}
#else
static uint8_t SpiReadByte (uint32_t SPI_REG)
{
  /* loop while data register in not empty */
  while (RESET == spi_i2s_flag_get(SPI_REG, SPI_FLAG_TBE));

  /* send dummy byte through the SPI peripheral */
  spi_i2s_data_transmit(SPI_REG, 0);
  while (spi_i2s_flag_get(SPI_REG, SPI_FLAG_TRANS));

  /* wait to receive a byte */
  while (RESET == spi_i2s_flag_get(SPI_REG,SPI_FLAG_RBNE));

  /* return the byte read from the SPI bus */
  return(spi_i2s_data_receive(SPI_REG));
}
#endif




