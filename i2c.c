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

    /* Configure up the I2C instance for this bus */
    switch (I2cBus)
    {
        case I2C_BUS_0:
            pBus->Instance = I2C0;
            
            or_register (RCU, RCU_APB1EN, (1 << 21));

            /* Set up GPIO pins for this port */
            
            set_gpio_alternate (I2C0_SDA);
            set_gpio_alternate (I2C0_SCL);
            
            or_register (AFIO, AFIO_PCF0, (1 << 1));    /* Remap I2C0 onto PB8 and PB9 */
            
            break;
            
        default:
            pBus->Instance = 0;
            break;
    }

    if (pBus->Instance)
    {
        /* configure I2C clock */
        
        /* Always set 400000 bits per second and ignore the BitRate parameter */
        
        write_register (pBus->Instance, I2C_CKCFG, (1 << 15) | (APB1_SPEED / 400000));
        write_register (pBus->Instance, I2C_CTL1, APB1_SPEED / 1000000);

        /* Enable I2C */
        
        
        /* configure I2C address */
        
        write_register (pBus->Instance, I2C_CKCFG, 0xff);

        /* enable I2C */
        or_register (pBus->Instance, I2C_CTL0, (1 << 0));
        
        /* enable acknowledge */
        or_register (pBus->Instance, I2C_CTL0, (1 << 10));
        
        
        pBus->Initialized = true;
    }
  }
  return (pBus->Initialized);
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
    I2cWriteBytes(pBus->Instance, ChipAddress, pTxData, Size);

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
      I2cReadBytes(pBus->Instance, ChipAddress, pRxData, Size);

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
    uint32_t I2Cn = pBus->Instance;
      /* i2c read begin */

      /* wait until I2C bus is idle */
      while(read_register (I2Cn, I2C_STAT1) & (1 << 1));    /* Wait while I2CBSY is 1 */
      
      if(2 == number_of_byte)
      {
          or_register (I2Cn, I2C_CTL0, (1 << 11));          /* Set POAP */
      }
      /* send a start condition to I2C bus */
      
      or_register (I2Cn, I2C_CTL0, (1 << 8));           /* START */

      /* wait until SBSEND bit is set */

      while(!(read_register (I2Cn, I2C_STAT0) & (1 << 0)));

      /* send slave address to I2C bus */
      
      write_register (I2Cn, I2C_DATA, bus_address & ~1);
      
       /* wait until ADDSEND bit is set */
      while(!(read_register (I2Cn, I2C_STAT0) & (1 << 1)));
      
      /* clear the ADDSEND bit */

      read_register (I2Cn, I2C_STAT0);
      read_register (I2Cn, I2C_STAT1);
            
/* wait until the transmit data buffer is empty */
      
      while(!(read_register (I2Cn, I2C_STAT0) & (1 << 7)));    /* Wait until I2C_FLAG_TBE is set */
      
      /* Surely it's aleady enabled? */
      
      /* enable I2Cn*/
      
      or_register (I2Cn, I2C_CTL0, (1 << 0));
      
       /* send the first byte, or start address to read */
      
      write_register (I2Cn, I2C_DATA, read_address);

      /* wait until BTC bit is set */

      while(!(read_register (I2Cn, I2C_STAT0) & (1 << 2)));
      
      /* send a start condition to I2C bus */
      
      or_register (I2Cn, I2C_CTL0, (1 << 8));           /* START */

      /* wait until SBSEND bit is set */

      while(!(read_register (I2Cn, I2C_STAT0) & (1 << 0)));

      /* send slave address to I2C bus */

      write_register (I2Cn, I2C_DATA, bus_address | 1);

      if(number_of_byte < 3)
      {
        and_register (pBus->Instance, I2C_CTL0, ~(1 << 10));        /* disable acknowledge */
      }
      
      /* wait until ADDSEND bit is set */

      while(!(read_register (I2Cn, I2C_STAT0) & (1 << 1)));
      
      /* clear the ADDSEND bit */

      read_register (I2Cn, I2C_STAT0);
      read_register (I2Cn, I2C_STAT1);
            
      if(1 == number_of_byte)
      {
        or_register (I2Cn, I2C_CTL0, (1 << 9));           /* STOP */
      }
      
      /* while there is data to be read */
      while(number_of_byte)
      {
        if(3 == number_of_byte)
        {
            /* wait until BTC bit is set */

            while(!(read_register (I2Cn, I2C_STAT0) & (1 << 2)));
            
          /* disable acknowledge */

            and_register (pBus->Instance, I2C_CTL0, ~(1 << 10));        /* disable acknowledge */
        }
        if(2 == number_of_byte)
        {
            /* wait until BTC bit is set */

            while(!(read_register (I2Cn, I2C_STAT0) & (1 << 2)));
            
          /* send a stop condition to I2C bus */
            
            or_register (I2Cn, I2C_CTL0, (1 << 9));           /* STOP */
        }
        /* wait until the RBNE bit is set and clear it */
        if(read_register (I2Cn, I2C_STAT0) & (1 << 6))
        {
          *p_buffer = read_register (I2Cn, I2C_DATA);       /* read a byte from device */
          p_buffer++; /* point to the next location where the byte read will be saved */
          number_of_byte--; /* decrement the read bytes counter */
        }
      }
      while(read_register (I2Cn, I2C_CTL0) & 0x0200);  /* wait until the stop condition is finished */
      
    /* enable acknowledge */
    or_register (pBus->Instance, I2C_CTL0, (1 << 10));        
    and_register (I2Cn, I2C_CTL0, ~(1 << 11));          /* Clear POAP */
    /* i2c read end */
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
    
    while(read_register (I2Cn, I2C_STAT1) & (1 << 1));    /* Wait while I2CBSY is 1 */
  
    /* send a start condition to I2C bus */
    
    or_register (I2Cn, I2C_CTL0, (1 << 8));           /* START */

    /* wait until SBSEND bit is set */

    while(!(read_register (I2Cn, I2C_STAT0) & (1 << 0)));

    /* send slave address to I2C bus */
    
    write_register (I2Cn, I2C_DATA, SlaveAddress | 1);

    if (NumberOfBytes < 3)
    {
        and_register (I2Cn, I2C_CTL0, ~(1 << 10));        /* disable acknowledge */
    }
    
    /* wait until ADDSEND bit is set */

    while(!(read_register (I2Cn, I2C_STAT0) & (1 << 1)));

    /* clear the ADDSEND bit */
  
    read_register (I2Cn, I2C_STAT0);
    read_register (I2Cn, I2C_STAT1);

    if (1 == NumberOfBytes)
    {
        or_register (I2Cn, I2C_CTL0, (1 << 9));           /* STOP */
    }

    /* while there is data to be read */
    while (NumberOfBytes)
    {
        if (3 == NumberOfBytes)
        {
            /* wait until BTC bit is set */
          
            while(!(read_register (I2Cn, I2C_STAT0) & (1 << 2)));
            
            /* disable acknowledge */
            
            and_register (I2Cn, I2C_CTL0, ~(1 << 10));        /* disable acknowledge */
        }
        
        if (2 == NumberOfBytes)
        {
            /* wait until BTC bit is set */
          
            while(!(read_register (I2Cn, I2C_STAT0) & (1 << 2)));
            
            /* send a stop condition to I2C bus */
            
            or_register (I2Cn, I2C_CTL0, (1 << 9));           /* STOP */
        }
    
        /* wait until the RBNE bit is set and clear it */
        if(read_register (I2Cn, I2C_STAT0) & (1 << 6))
        {
            *pBuffer = read_register (I2Cn, I2C_DATA);       /* read a byte from device */
            pBuffer++; /* point to the next location where the byte read will be saved */
            NumberOfBytes--; /* decrement the read bytes counter */
        }
    } while(read_register (I2Cn, I2C_CTL0) & 0x0200);  /* wait until the stop condition is finished */

  /* enable acknowledge */
    or_register (I2Cn, I2C_CTL0, (1 << 10));        
    and_register (I2Cn, I2C_CTL0, ~(1 << 11));          /* Clear POAP */
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
    while(read_register (I2Cn, I2C_STAT1) & (1 << 1));    /* Wait while I2CBSY is 1 */
  
    /* send a start condition to I2C bus */
    or_register (I2Cn, I2C_CTL0, (1 << 8));           /* START */

    /* wait until SBSEND bit is set */

    while(!(read_register (I2Cn, I2C_STAT0) & (1 << 0)));

    /* send slave address to I2C bus, wait until ADDSEND bit is set and clear it*/
  
    write_register (I2Cn, I2C_DATA, SlaveAddress & ~1);

    /* wait until ADDSEND bit is set */
    
    while(!(read_register (I2Cn, I2C_STAT0) & (1 << 1)));

    /* clear the ADDSEND bit */
  
    read_register (I2Cn, I2C_STAT0);
    read_register (I2Cn, I2C_STAT1);

    /* wait until the transmit data buffer is empty */
  
    while (!(read_register (I2Cn, I2C_STAT0) & (1 << 7)))
        

    /* while there is data to be written */
        
    while (NumberOfBytes--)
    {
        write_register (I2Cn, I2C_DATA, *pBuffer);
        pBuffer++;

        /* wait until BTC bit is set */
        while(!(read_register (I2Cn, I2C_STAT0) & (1 << 2)));
    }

    /* send a stop condition to I2C bus */
    or_register (I2Cn, I2C_CTL0, (1 << 9));           /* STOP */

    /* wait until the stop condition is finished */
    while(read_register (I2Cn, I2C_CTL0) & 0x0200);  /* wait until the stop condition is finished */
  
}



