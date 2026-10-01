/**
 **************************************************************************************************
 * @file   i2c.c
 *
 * @author Ripcord Designs
 *
 * @brief  This module provides an abstraction layer for the GD32 on-chip I2C interface.
 *         It shares a common Ripcord API with I2C modules for other chips, so higher software
 *         layers can be easily moved between different MCU's.
 *
 * @notes  Master mode transfers only at present
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
/* Number of I2C busses on this chip */
#define NO_OF_I2C_BUSSES 2

/* bus type */
typedef struct
{
  /* Instance of the I2C Port */
  uint32_t          Instance;

  /* each bus is locked while transfers are in progress */
  SemaphoreHandle_t BusMutex;

  /* flag to indicate whether the bus has been initialised */
  bool              Initialized;
} i2c_bus_t;



/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static void I2cReadBytes  (uint32_t I2Cn, uint8_t SlaveAddress, uint8_t * pBuffer, uint16_t NumberOfBytes);
static void I2cWriteBytes (uint32_t I2Cn, uint8_t SlaveAddress, uint8_t * pBuffer, uint16_t NumberOfBytes);


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
/* I2C Busses  */
static i2c_bus_t I2c[NO_OF_I2C_BUSSES];


/*
 **************************************************************************************************
 *                                   PUBLIC SECTION / INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       Initialise the I2C bus.  Must be performed before the bus is used.
 * @param[in]   pI2cHandle : I2C handle to configure
 * @return      true on success, false on failure
 */
bool I2cInitialise (i2cbus_e I2cBus, uint32_t BitRate)
{
  static bool FirstRun = true;
  i2c_bus_t * pBus = &I2c[I2cBus];

  /* The first time through we need to do some basic initialisation */
  if (FirstRun)
  {
    for (int bus = 0; bus < NO_OF_I2C_BUSSES; bus++)
    {
      I2c[bus].Initialized = false;
      I2c[bus].Instance = 0;
    }
    FirstRun = false;
  }

  /* Initialise this specific bus */
  if (false == pBus->Initialized)
  {
    pBus->BusMutex = xSemaphoreCreateBinary();
    xSemaphoreGive(pBus->BusMutex);

    /* Configure up the I2C instance for this bus */
    switch (I2cBus)
    {
#ifdef CS_USE_I2C0
    case I2C_BUS_0:
    	pBus->Instance = I2C0;
        rcu_periph_clock_enable(RCU_I2C0);

        /* Set up GPIO pins for this port */
        rcu_periph_clock_enable(I2C0_PORT_CLK);
        gpio_init(I2C0_SCL_PORT, GPIO_MODE_AF_OD, GPIO_OSPEED_50MHZ, I2C0_SCL_PIN);
        gpio_init(I2C0_SDA_PORT, GPIO_MODE_AF_OD, GPIO_OSPEED_50MHZ, I2C0_SDA_PIN);
        rcu_periph_clock_enable(RCU_I2C0);
        gpio_pin_remap_config(GPIO_I2C0_REMAP, ENABLE);
    	break;
#endif
#ifdef CS_USE_I2C1
    case I2C_BUS_1:
    	pBus->Instance = I2C1;
        rcu_periph_clock_enable(RCU_I2C1);

        /* Set up GPIO pins for this port */
        rcu_periph_clock_enable(I2C1_PORT_CLK);
        gpio_init(I2C1_SCL_PORT, GPIO_MODE_AF_OD, GPIO_OSPEED_50MHZ, I2C1_SCL_PIN);
        gpio_init(I2C1_SDA_PORT, GPIO_MODE_AF_OD, GPIO_OSPEED_50MHZ, I2C1_SDA_PIN);
        gpio_pin_remap_config(GPIO_I2C1_REMAP, ENABLE);
      break;
#endif
    default:
    	pBus->Instance = 0;
    	break;
    }

    if (pBus->Instance)
    {
      rcu_periph_clock_enable(RCU_AF);

      /* configure I2C clock */
      i2c_clock_config(pBus->Instance, BitRate, I2C_DTCY_2);

      /* configure I2C address */
      i2c_mode_addr_config(pBus->Instance, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, 0xFF);
//      i2c_mode_addr_config(pBus->Instance, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, 0x70);

      /* enable I2C */
      i2c_enable(pBus->Instance);

      /* enable acknowledge */
      i2c_ack_config(pBus->Instance, I2C_ACK_ENABLE);

      pBus->Initialized = true;
    }
  }
  return (pBus->Initialized);
}


/**
 * @brief       De-initialise one I2C bus.
 * @param[in]   I2cBus : the I2C bus to de-init
 * @return      true on success, false on failure
 */
bool I2cDeInitialise (i2cbus_e I2cBus)
{
  i2c_bus_t * pBus = &I2c[I2cBus];

  /* deinit the I2C Bus instance */
  i2c_deinit(pBus->Instance);

  /* delete the semaphores */
  vSemaphoreDelete(pBus->BusMutex);

  /* Reset the driver parameters */
  pBus->Initialized = false;
  pBus->Instance = 0;

  return true;

}


/**
 * @brief       Transmit data over the I2C bus
 * @param[in]   I2cBus      : The I2C bus to use
 * @param[in]   ChipAddress : I2C address of the target chip
 * @param[in]   pTxData     : pointer to the data to send
 * @param[in]   Size        : size of the data to send
 * @return      true on success, false on failure
 */
bool I2cTransmit (i2cbus_e I2cBus, uint8_t ChipAddress, uint8_t * pTxData, uint16_t Size)
{
  bool Status = true;
  i2c_bus_t * pBus = &I2c[I2cBus];

  if (pBus->Initialized && pBus->Instance)
  {
    /* Lock the I2C bus */
    Status = (pdTRUE == xSemaphoreTake(pBus->BusMutex, portMAX_DELAY));

    if (Status)
    {
      I2cWriteBytes(pBus->Instance, ChipAddress, pTxData, Size);

      /* Unlock the I2C bus */
      xSemaphoreGive(pBus->BusMutex);
    }
  }

  return Status;
}


/**
 * @brief       Receive data over the I2C bus
 * @param[in]   I2cBus      : The I2C bus to use
 * @param[in]   ChipAddress : I2C address of the target chip
 * @param[in]   pRxData     : pointer to the buffer for the received data
 * @param[in]   read_address: internal address or register to start reading
 * @param[in]   Size        : size of the data to receive
 * @return      true on success, false on failure
 */
bool I2cReceive (i2cbus_e I2cBus, uint8_t ChipAddress, uint8_t * pRxData, uint16_t Size)
{
  bool Status = false;
  i2c_bus_t * pBus = &I2c[I2cBus];

  if (pBus->Initialized && pBus->Instance)
  {
    /* Lock the I2C bus */
    Status = (pdTRUE == xSemaphoreTake(pBus->BusMutex, portMAX_DELAY));

    if (Status)
    {
      I2cReadBytes(pBus->Instance, ChipAddress, pRxData, Size);

      /* Unlock the I2C bus */
      xSemaphoreGive(pBus->BusMutex);
    }
  }
  return Status;
}


/**
* @brief        read a bytes from I2C bus from the read_address by write one byte and read back number of byte
*
* @warning      i2c port need to be set up first, tuned for the Gas Gauge port.
*
* @param[in]    I2C_REG: I2C port register (0-2)
* @param[in]    bus_address: I2C slave device address, 7-bit < 1,
* @param[in]    p_buffer: pointer to the buffer that receives the data read from the i2c bus
* @param[in]    read_address: internal address or register to start reading
* @param[in]    number_of_byte: number of bytes to reads
* @return       true on success, false on failure
*/
bool I2c_read_GG_bytes (i2cbus_e I2cBus, uint8_t bus_address, uint8_t* p_buffer, uint8_t read_address, uint16_t number_of_byte)
{
  bool Status = false;
  i2c_bus_t * pBus = &I2c[I2cBus];

  if (pBus->Initialized && pBus->Instance)
  {
    /* Lock the I2C bus */
    uint32_t I2Cn = pBus->Instance;
    Status = (pdTRUE == xSemaphoreTake(pBus->BusMutex, portMAX_DELAY));
    if (Status)
    {
      /* i2c read begin */

      /* wait until I2C bus is idle */
      while(i2c_flag_get(I2Cn, I2C_FLAG_I2CBSY));
      if(2 == number_of_byte)
      {
        i2c_ackpos_config(I2Cn,I2C_ACKPOS_NEXT);
      }
      /* send a start condition to I2C bus */
      i2c_start_on_bus(I2Cn);
      /* wait until SBSEND bit is set */
      while(!i2c_flag_get(I2Cn, I2C_FLAG_SBSEND));
      /* send slave address to I2C bus */
      i2c_master_addressing(I2Cn, bus_address, I2C_TRANSMITTER);
       /* wait until ADDSEND bit is set */
      while(!i2c_flag_get(I2Cn, I2C_FLAG_ADDSEND));
      /* clear the ADDSEND bit */
      i2c_flag_clear(I2Cn,I2C_FLAG_ADDSEND);
      /* wait until the transmit data buffer is empty */
      while(SET != i2c_flag_get( I2Cn , I2C_FLAG_TBE));
      /* enable I2Cn*/
      i2c_enable(I2Cn);
       /* send the first byte, or start address to read */
      i2c_data_transmit(I2Cn, read_address);
      /* wait until BTC bit is set */
      while(!i2c_flag_get(I2Cn, I2C_FLAG_BTC));
      /* send a start condition to I2C bus */
      i2c_start_on_bus(I2Cn);
      /* wait until SBSEND bit is set */
      while(!i2c_flag_get(I2Cn, I2C_FLAG_SBSEND));
      /* send slave address to I2C bus */
      i2c_master_addressing(I2Cn, bus_address, I2C_RECEIVER);
      if(number_of_byte < 3)
      {
        i2c_ack_config(I2Cn,I2C_ACK_DISABLE);       /* disable acknowledge */
      }
      /* wait until ADDSEND bit is set */
      while(!i2c_flag_get(I2Cn, I2C_FLAG_ADDSEND));
      /* clear the ADDSEND bit */
      i2c_flag_clear(I2Cn,I2C_FLAG_ADDSEND);
      if(1 == number_of_byte)
      {
        i2c_stop_on_bus(I2Cn);          /* send a stop condition to I2C bus */
      }
      /* while there is data to be read */
      while(number_of_byte)
      {
        if(3 == number_of_byte)
        {
          while(!i2c_flag_get(I2Cn, I2C_FLAG_BTC));         /* wait until BTC bit is set */
          /* disable acknowledge */
          i2c_ack_config(I2Cn,I2C_ACK_DISABLE);
        }
        if(2 == number_of_byte)
        {
          while(!i2c_flag_get(I2Cn, I2C_FLAG_BTC));         /* wait until BTC bit is set */
          /* send a stop condition to I2C bus */
          i2c_stop_on_bus(I2Cn);
        }
        /* wait until the RBNE bit is set and clear it */
        if(i2c_flag_get(I2Cn, I2C_FLAG_RBNE))
        {
          *p_buffer = i2c_data_receive(I2Cn);       /* read a byte from device */
          p_buffer++; /* point to the next location where the byte read will be saved */
          number_of_byte--; /* decrement the read bytes counter */
        }
      }
      while(I2C_CTL0(I2Cn)&0x0200);  /* wait until the stop condition is finished */
      /* enable acknowledge */
      i2c_ack_config(I2Cn,I2C_ACK_ENABLE);
      i2c_ackpos_config(I2Cn,I2C_ACKPOS_CURRENT);
      /* i2c read end */

     /* Unlock the I2C bus */
      xSemaphoreGive(pBus->BusMutex);
    }
  }

  return (Status);
}


/*
 **************************************************************************************************
 *                                         PRIVATE SECTION
 **************************************************************************************************
 */


/**
* @brief        read a bytes from I2C bus from the read_address by write one byte and read back number of byte
*
* @param[in]    I2C_REG: I2C port register (0-2)
* @param[in]    SlaveAddress : I2C slave device address, 7-bit < 1,
* @param[in]    pBuffer      : pointer to the buffer that receives the data read from the i2c bus
* @param[in]    NumberOfBytes: number of bytes to reads
* @return       {none}
*/

static void I2cReadBytes (uint32_t I2Cn, uint8_t SlaveAddress, uint8_t* pBuffer, uint16_t NumberOfBytes)
{
  /* wait until I2C bus is idle */
  while (i2c_flag_get(I2Cn, I2C_FLAG_I2CBSY));

  /* send a start condition to I2C bus */
  i2c_start_on_bus(I2Cn);

  /* wait until SBSEND bit is set */
  while (!i2c_flag_get(I2Cn, I2C_FLAG_SBSEND));

  /* send slave address to I2C bus */
  i2c_master_addressing(I2Cn, SlaveAddress, I2C_RECEIVER);

  if (NumberOfBytes < 3)
  {
    i2c_ack_config(I2Cn, I2C_ACK_DISABLE);       /* disable acknowledge */
  }
  /* wait until ADDSEND bit is set */
  while (!i2c_flag_get(I2Cn, I2C_FLAG_ADDSEND));

  /* clear the ADDSEND bit */
  i2c_flag_clear(I2Cn,I2C_FLAG_ADDSEND);

  if (1 == NumberOfBytes)
  {
    i2c_stop_on_bus(I2Cn);          /* send a stop condition to I2C bus */
  }

  /* while there is data to be read */
  while (NumberOfBytes)
  {
    if (3 == NumberOfBytes)
    {
      while (!i2c_flag_get(I2Cn, I2C_FLAG_BTC));         /* wait until BTC bit is set */
      /* disable acknowledge */
      i2c_ack_config(I2Cn,I2C_ACK_DISABLE);
    }
    if (2 == NumberOfBytes)
    {
      while (!i2c_flag_get(I2Cn, I2C_FLAG_BTC));         /* wait until BTC bit is set */
      /* send a stop condition to I2C bus */
      i2c_stop_on_bus(I2Cn);
    }
    /* wait until the RBNE bit is set and clear it */
    if (i2c_flag_get(I2Cn, I2C_FLAG_RBNE))
    {
      *pBuffer = i2c_data_receive(I2Cn);       /* read a byte from device */
      pBuffer++; /* point to the next location where the byte read will be saved */
      NumberOfBytes--; /* decrement the read bytes counter */
    }
  }
  while (I2C_CTL0(I2Cn)&0x0200);  /* wait until the stop condition is finished */

  /* enable acknowledge */
  i2c_ack_config(I2Cn,I2C_ACK_ENABLE);
  i2c_ackpos_config(I2Cn,I2C_ACKPOS_CURRENT);
}


/**
* @brief        write more than one byte to the i2c device with a single write cycle
*
* @param[in]    I2C_REG      : I2C port register (0-2)
* @param[in]    SlaveAddress : I2C slave device address, 7-bit < 1,
* @param[in]    p_buffer     : pointer to the buffer containing the data to be written to i2c device
* @param[in]    NumberOfBytes: number of bytes to write
* @return       {none}
*/
static void I2cWriteBytes (uint32_t I2Cn, uint8_t SlaveAddress, uint8_t * pBuffer, uint16_t NumberOfBytes)
{
  /* wait until I2C bus is idle */
  while (i2c_flag_get(I2Cn, I2C_FLAG_I2CBSY));

  /* send a start condition to I2C bus */
  i2c_start_on_bus(I2Cn);

  /* wait until SBSEND bit is set */
  while (!i2c_flag_get(I2Cn, I2C_FLAG_SBSEND));

  /* send slave address to I2C bus, wait until ADDSEND bit is set and clear it*/
  i2c_master_addressing(I2Cn, SlaveAddress, I2C_TRANSMITTER);

  /* wait until ADDSEND bit is set */
  while (!i2c_flag_get(I2Cn, I2C_FLAG_ADDSEND));

  /* clear the ADDSEND bit */
  i2c_flag_clear(I2Cn,I2C_FLAG_ADDSEND);

  /* wait until the transmit data buffer is empty */
  while (SET != i2c_flag_get(I2Cn, I2C_FLAG_TBE));

  /* while there is data to be written */
  while (NumberOfBytes--)
  {
    i2c_data_transmit(I2Cn, *pBuffer);
    pBuffer++;

    /* wait until BTC bit is set */
    while (!i2c_flag_get(I2Cn, I2C_FLAG_BTC));
  }

  /* send a stop condition to I2C bus */
  i2c_stop_on_bus(I2Cn);

  /* wait until the stop condition is finished */
  while(I2C_CTL0(I2Cn)&0x0200);
}


/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */

/**
 * @brief       Probe the I2C bus and display a list of active addresses on the bus
 * @param[in]   argCount    : The number of arguments passed from the console
 * @param[in]   pArguments  : The list of arguments passed from the console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int I2cConsoleProbe (int ArgCount, char **pArguments)
{
  return(CONSOLE_ERROR_OK);
}


/**
 * @brief       Display I2C statistics
 * @param[in]   argCount    : The number of arguments passed from the console
 * @param[in]   pArguments  : The list of arguments passed from the console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int I2cConsoleStats (int ArgCount, char **pArguments)
{
  /* print title row */
  ConsolePrintf("I2C#    Instance  TxCount   RxCount   ErrorCount\r");

  /* print the status data */
#if 0
  for (int I2cIndex = 0; I2cIndex < NO_OF_I2C_BUSSES; I2cIndex++)
  {
    i2c_bus_t * pBus = &I2c[I2cIndex];
    ConsolePrintf("%d     %d     %d\n\r",
                 I2cIndex,
                 pBus->Instance,
				 I2c[I2cIndex].Error.TxCount,
				 I2c[I2cIndex].Error.RxCount,
				 I2c[I2cIndex].Error.ErrorCount
                 );
  }
#endif
  return(CONSOLE_ERROR_OK);
}


/**
 * @brief       Reset all I2C statistics back to zero
 * @param[in]   argCount    : The number of arguments passed from the console
 * @param[in]   pArguments  : The list of arguments passed from the console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int I2cConsoleClear (int ArgCount, char **pArguments)
{
  /* Reset the statistics */
  for (int I2cIndex = 0; I2cIndex < NO_OF_I2C_BUSSES; I2cIndex++)
  {
//    I2c[I2cIndex].Error.TxCount = 0;
//    I2c[I2cIndex].Error.RxCount = 0;
//    I2c[I2cIndex].Error.ErrorCount = 0;
  }

  /* indicate success */
  return (CONSOLE_ERROR_OK);
}


