/**
 **************************************************************************************************
 * @file   acceldriver.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   20/11/2019
 *
 * @brief  Accelerometer driver for ST LSM6DSL
 *
 **************************************************************************************************
 */

#include "global.h"



/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */
/* SPI bus settings for this device */
#define LSM6DSL_SPI_MODE         0
#define LSM6DSL_SPI_BITRATE      8000000

/* Commands */
#define SPI_WRITE_CMD            0x00
#define SPI_READ_CMD             0x80
#define SPI_TIMEOUT              5000 /* 5 seconds */

/* Registers */
#define LSM6DSL_FIFO_CTRL1       0x06
#define LSM6DSL_FIFO_CTRL2       0x07
#define LSM6DSL_FIFO_CTRL3       0x08
#define LSM6DSL_FIFO_CTRL4       0x09
#define LSM6DSL_FIFO_CTRL5       0x0A
#define LSM6DSL_WHO_AM_I         0x0F
#define LSM6DSL_CTRL1_XL         0x10
#define LSM6DSL_CTRL2_G          0x11
#define LSM6DSL_CTRL3_C          0x12
#define LSM6DSL_CTRL6_C          0x15   /* XL_HM_MODE */
#define LSM6DSL_CTRL7_G          0x16   /* G_HM_MODE */
#define LSM6DSL_WAKE_UP_SRC      0x1B
#define LSM6DSL_OUTX_L_XL        0x28
#define LSM6DSL_OUTX_H_XL        0x29
#define LSM6DSL_OUTY_L_XL        0x2A
#define LSM6DSL_OUTY_H_XL        0x2B
#define LSM6DSL_OUTZ_L_XL        0x2C
#define LSM6DSL_OUTZ_H_XL        0x2D
#define LSM6DSL_FIFO_STATUS1     0x3A
#define LSM6DSL_FIFO_STATUS2     0x3B
#define LSM6DSL_FIFO_STATUS3     0x3C
#define LSM6DSL_FIFO_STATUS4     0x3D
#define LSM6DSL_FIFO_DATA_L      0x3E
#define LSM6DSL_FIFO_DATA_H      0x3F
#define LSM6DSL_FUNC_SRC1        0x53
#define LSM6DSL_TAP_CFG          0x58
#define LSM6DSL_WAKE_UP_THS      0x5B
#define LSM6DSL_WAKE_UP_DUR      0x5C
#define LSM6DSL_MD1_CFG          0x5E
#define LSM6DSL_MD2_CFG          0x5F

/* Bit fields in registers */
#define ODR_XL_52HZ              0x30 // Accelerometer update rate 52Hz, low power when G_HM_MODE=1
#define ODR_XL_104HZ             0x40 // Accelerometer update rate 104Hz
#define XL_FS_2G                 0x00 // Accelerometer fullscale +- 2g
#define XL_FS_4G                 0x08 // Accelerometer fullscale +- 4g
#define XL_FS_8G                 0x0C // Accelerometer fullscale +- 8g
#define XL_FS_16G                0x04 // Accelerometer fullscale +- 16g
#define XL_HM_MODE_DISABLE       0x10 // disable Accelerometer high-performance operating mode
#define XL_HM_MODE_ENABLE        0x00 // enable Accelerometer high-performance operating mode
#define ODR_G_0HZ                0x00 // Gyro update rate 0Hz, power down
#define ODR_G_52HZ               0x30 // Gyro update rate 52Hz, low power when G_HM_MODE=1
#define ODR_G_104HZ              0x40 // Gyro update rate 104Hz
#define G_FS_2000DPS             0x0C // Gyro fullscale +- 2000dps
#define CTRL3_C_BDU              0x40 // Block Data Update flag
#define CTRL3_C_IF_INC           0x04 // Register Address Auto-Increment flag
#define CTRL3_C_SW_RESET         0x01 // SW Reset flag
#define NO_DEC_G                 0x01 // No FIFO decimation for gyro data
#define NO_DEC_XL                0x08 // No FIFO decimation for accel data
#define ODR_FIFO_104HZ           0x20 // FIFO update rate 104Hz
#define FIFO_MODE_BYPASS         0x00 // FIFO mode = bypass
#define FIFO_MODE_CONTINUOUS     0x06 // FIFO mode = continuous
#define FIFO_STATUS_DIFF_MASK    0x07 // High 3 bits of unread words in 3 ls bits
#define FIFO_STATUS_OR_MASK      0x40 // Bit indicating FIFO Overrun
#define FIFO_STATUS_PATTERN_MASK 0x03 // High 2 bits of FIFO pattern is 2 ls bits
#define FIFO_STOP_ON_FTH         0x80 // Stop filling FIFO when threshold set in
                                      // FIFO_CTRL1 and FIFO_CTRL_2 is reached
//#define WAKE_UP_THRESHOLD        0x01 // Wake-up Threshold: 1 = FS_XL/64
#define WAKE_UP_THRESHOLD        0x02 // Wake-up Threshold: 1 = FS_XL/64 (0.031g)
#define WAKE_UP_THRESHOLD_2      0x06 // Wake-up Threshold: 1 = FS_XL/64 (0.100g)
#define WAKE_UP_DURATION         0x40 // Wake-up Threshold: 2 ODR_time
#define INT_WU                   0x20 // INT = Wake-up
#define INT_ENABLE               0x80 // Enable interrupt
#define INT_LATCHING             0x01 // Interrupt latches until the cause register is read

#define MM_WHO_AM_I_BYTE         0x6A // "Who am I" identifier of the LSM6DSL

/* Set size of FIFO to be 4092 bytes (2046 words), which is a multiple of
 * 12 bytes. This ensures that if the FIFO fills up we preserve the sample
 * pattern Gx,Gy,Gz,XLx,XLy,XLz with no extra bytes left over */
#define FIFO_SIZE_WORDS          2046

/* delay required after changing FIFO settings to prevent spurious entries */
#define FIFO_RESET_DELAY_MS      15

/* operation mode */
#define HM_MODE_DISABLE          1
#define HM_MODE_ENABLE           0

/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
static spihandle_t SpiHandle;

static uint8_t spiReadBuffer[SPI_BUFFER_SIZE];  // Is this really needed?


/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static bool   drvSpiReadReg                   (uint8_t registerNumber, uint8_t *pData);
static bool   drvSpiWriteReg                  (uint8_t registerNumber, uint8_t data);
static bool   drvSpiReadMultibyte             (uint8_t registerNumber, uint8_t *pData, uint16_t length);
static void   AccelDriverSetOperationMode     (uint8_t mode);
static bool   AccelDriverClearMotionFlags     (void);

/* Global var (temporary use) */
bool AccelWakeUp = false;


/*
 **************************************************************************************************
 *                                   PUBLIC SECTION / INTERFACE
 **************************************************************************************************
 */
/**
 * @brief     Initialise the Accelerometer
 * @param     None
 * @return    true on success, false on failure
 */
bool AccelDriverInit (spibus_e SpiBus, spichipselect_t ChipSelect)
{
  /* Configure the SPI bus & chip select for this device */
  SpiHandle.Bus = SpiBus;
  SpiHandle.ChipSelect = ChipSelect;
  SpiHandle.Mode = LSM6DSL_SPI_MODE;
  SpiHandle.BitRate = LSM6DSL_SPI_BITRATE;
  bool status = SpiInitialise(&SpiHandle);

  if (status)
  {
    /* Now set up the Accelerometer itself */
    uint8_t readByte;
    /* Read "Who am I" from accelerometer. */
    drvSpiReadReg(LSM6DSL_WHO_AM_I, &readByte);

    /* Carry a SW reset of the accelerometer */
    drvSpiWriteReg(LSM6DSL_CTRL3_C, CTRL3_C_SW_RESET);

    /* Wait until the reset has taken effect */
    sleep(2);

    /*
     * Ensure that the BDU flag is set in the CTRL3_C register
     * This will ensure that the LSB and MSB bytes of an accel
     * or gyro value cannot be updated whilst one has been read
     * but the other not.
     * Also set the IF_INC register address auto-increment flag.
     */
    drvSpiWriteReg(LSM6DSL_CTRL3_C, CTRL3_C_BDU | CTRL3_C_IF_INC);

    /* Set size of FIFO */
    drvSpiWriteReg(LSM6DSL_FIFO_CTRL1, FIFO_SIZE_WORDS & 0xFF);
    drvSpiWriteReg(LSM6DSL_FIFO_CTRL2, FIFO_SIZE_WORDS >> 8);

    /* Set decimation rate of FIFO */
    drvSpiWriteReg(LSM6DSL_FIFO_CTRL3, NO_DEC_G | NO_DEC_XL);

    /* Set FIFO to use size specified in CTRL1 and CTRL2 */
    drvSpiWriteReg(LSM6DSL_FIFO_CTRL4, FIFO_STOP_ON_FTH);

    /* Set update rate of acceleration data to  104 Hz and
     * fullscale to be +/- 16g */
    drvSpiWriteReg(LSM6DSL_CTRL1_XL, ODR_XL_104HZ | XL_FS_16G);

    /* Set update rate of gyro data to be 104 Hz and
     * fullscale to be +/- 2000dps */
    drvSpiWriteReg(LSM6DSL_CTRL2_G, ODR_G_104HZ | G_FS_2000DPS);

    /* Set update rate and mode of FIFO */
    drvSpiWriteReg(LSM6DSL_FIFO_CTRL5, ODR_FIFO_104HZ | FIFO_MODE_CONTINUOUS);

    /* Configure wake-up interrupt */
    AccelDriverWakeupThresholdConfig();

  }

  /* Set up the IMU interrupt pin */

  set_gpio_input (IMU_INT2);

  /* interrupt init */
  
  /* Set EXTI 7 to be rising edge interrupt on PB7, priority 5 */
  
  /* Set rising edge trigger */
  
  or_register (EXTI, EXTI_RTEN, (1 << 7));
  
  /* Disable falling edge trigger just in case */
  
  and_register (EXTI, EXTI_FTEN, ~(1 << 7));
  
  /* Set EXTI7 to use PB7 */
  
  write_register (AFIO, AFIO_EXTISS1, (read_register (AFIO, AFIO_EXTISS1) & ~(0xf << 12)) | (1 << 12));
  
  
  InterruptConfig.irqPrePriority = 5;
  InterruptConfig.irqSubPriority = 0;
  GpioConfigInterrupt(ACCEL_INT2_PORT, ACCEL_INT2_PIN, &InterruptConfig);

  /* start with the wake up interrupt disabled */
  AccelDriverDisableWakeUpInterrupt();

  return status;
}


/**
 * @brief     Get the X-acceleration
 * @param     None
 * @return    The X-acceleration in g/1000 (mg)
 */
int16_t AccelDriverGetAccelerationX (void)
{
  uint8_t readByte;
  uint16_t rawAccel;
  /* Read the lower and upper bytes */
  drvSpiReadReg(LSM6DSL_OUTX_L_XL, &readByte );
  rawAccel = readByte;
  drvSpiReadReg(LSM6DSL_OUTX_H_XL, &readByte );
  rawAccel |= (((uint16_t)readByte)<<8);
  /* Convert to signed value, then scale to range to units
   * of G/1000 in range -16g to +16g. Use U32 for
   * intermediate value to avoid overflow */
  uint32_t intermediateValue = ((int32_t)(int16_t)rawAccel)*1000;
  return (int16_t)(intermediateValue/2048);
}


/**
 * @brief     Get the Y-acceleration
 * @param     None
 * @return    The Y-acceleration in g/1000 (mg)
 */
int16_t AccelDriverGetAccelerationY (void)
{
  uint8_t readByte;
  uint16_t rawAccel;
  /* Read the lower and upper bytes */
  drvSpiReadReg(LSM6DSL_OUTY_L_XL, &readByte );
  rawAccel = readByte;
  drvSpiReadReg(LSM6DSL_OUTY_H_XL, &readByte );
  rawAccel |= (((uint16_t)readByte)<<8);
  /* Convert to signed value, then scale to range to units
   * of g/1000 in range -16g to +16g. Use U32 for
   * intermediate value to avoid overflow */
  uint32_t intermediateValue = ((int32_t)(int16_t)rawAccel)*1000;
  return (int16_t)(intermediateValue/2048);
}


/**
 * @brief     Get the Y-acceleration
 * @param     None
 * @return    The Y-acceleration in g/1000 (mg)
 */
int16_t AccelDriverGetAccelerationZ (void)
{
  uint8_t readByte;
  uint16_t rawAccel;
  /* Read the lower and upper bytes */
  drvSpiReadReg(LSM6DSL_OUTZ_L_XL, &readByte );
  rawAccel = readByte;
  drvSpiReadReg(LSM6DSL_OUTZ_H_XL, &readByte );
  rawAccel |= (((uint16_t)readByte)<<8);
  /* Convert to signed value, then scale to range to units
   * of g/1000 in range -16g to +16g. Use U32 for
   * intermediate value to avoid overflow */
  uint32_t intermediateValue = ((int32_t)(int16_t)rawAccel)*1000;
  return (int16_t)(intermediateValue/2048);
}


/**
 * @brief     Get the FIFO status
 * @param     Pointer to integer to hold the number of 16-bit
 *            samples available in the FIFO
 * @return    true if no error found, false otherwise
 */
bool AccelDriverGetFifoStatus (uint16_t* pSamplesAvailable)
{
  bool success = true;
  uint8_t lowByte = 0;
  uint8_t highByte = 0;
  drvSpiReadReg(LSM6DSL_FIFO_STATUS1, &lowByte);
  drvSpiReadReg(LSM6DSL_FIFO_STATUS2, &highByte);
  *pSamplesAvailable = (((uint16_t)(highByte & FIFO_STATUS_DIFF_MASK)) << 8) | (uint16_t)lowByte;

  /* Check for FIFO overrun */
  if ((*pSamplesAvailable == 0) && ((highByte & FIFO_STATUS_OR_MASK) != 0))
  {
    /* Overrun has occurred - attempt to catchup though some data will have been lost */
    *pSamplesAvailable = 999; // attempt to retrieve a large amount to recover situation
  }

  /* Check that the next sample that we will read from the FIFO is at the start of the
   * 6-sample dataset (Gx,Gy,Gx,XLx,XLy,XLz) that we will read from the FIFO. It is highly
   * unlikely to not to be, but if this error occurs we must take remedial action
   * or we will interpret the FIFO data incorrectly.
   */
  drvSpiReadReg(LSM6DSL_FIFO_STATUS3, &lowByte);
  drvSpiReadReg(LSM6DSL_FIFO_STATUS4, &highByte);
  uint16_t fifoPattern = (((uint16_t)(highByte & FIFO_STATUS_PATTERN_MASK)) << 8) | (uint16_t)lowByte;
  if (fifoPattern != 0)
  {
    success = false;
  }
  return success;
}


/**
 * @brief     Get a low-byte/high-byte pair data out of the FIFO
 * @param     Pointer to buffer to hold data
 * @return    Number of bytes read from FIFO
 */
uint8_t AccelDriverGetFifoDataPair (int16_t* pData)
{
  uint8_t bytesRead = 0;
  uint8_t dataPair[2];

  /*
   * Register address auto-increment will mean that the first byte is read
   * from the FIFO low-byte register and the second is read from the high-byte
   * register.
   */
  if (drvSpiReadMultibyte(LSM6DSL_FIFO_DATA_L, &dataPair[0], 2))
  {
    uint16_t tempU16 = (((uint16_t)dataPair[1])<<8) | dataPair[0];
    *pData = (int16_t)tempU16;
    bytesRead = 2;
  }
  else
  {
    *pData = 0;
  }
  return bytesRead;
}


/**
 * @brief     Reset the FIFO
 * @param     None
 * @return    None
 */
void AccelDriverResetFifo (void)
{
  /* Set FIFO into Bypass mode */
  drvSpiWriteReg(LSM6DSL_FIFO_CTRL5, ODR_FIFO_104HZ | FIFO_MODE_BYPASS);
  /* Set FIFO back into Continuous mode */
  drvSpiWriteReg(LSM6DSL_FIFO_CTRL5, ODR_FIFO_104HZ | FIFO_MODE_CONTINUOUS);
}


/**
 * @brief     Check the IMU's WHOAMI byte
 * @param     None
 * @return    Whether the WHOAMI byte was as expected
 */
bool AccelDriverCheckWhoAmI (void)
{
  uint8_t readByte;
  bool success = false;
  /* Read "Who am I" from accelerometer. */
  if (drvSpiReadReg(LSM6DSL_WHO_AM_I, &readByte))
  {
    if (readByte == MM_WHO_AM_I_BYTE)
    {
      success = true;
    }
  }
  return success;
}


/**
 * @brief     Enable the wake up interrupt from the IMU
 * @param     None
 * @return    None
 */
void AccelDriverEnableWakeUpInterrupt (void)
{
  /* clear the Wake Up flags in the IMU to make sure we don't immediately interrupt */
  AccelDriverClearMotionFlags();

  /* Enable the interrupt */
#ifdef GD32F30X_HD
  or_register (EXTI, EXTI_INTEN, (1 << 7));
  write_register (EXTI, EXTI_PD, (1 << 7));
#else
  GpioEnableInterrupt(ACCEL_INT2_PORT, ACCEL_INT2_PIN);
#endif
}


/**
 * @brief     Disable the wake up interrupt from the IMU
 * @param     None
 * @return    None
 */
void AccelDriverDisableWakeUpInterrupt (void)
{
#ifdef GD32F30X_HD
  write_register (EXTI, EXTI_PD, (1 << 7));
  and_register (EXTI, EXTI_INTEN, ~(1 << 7));
#else
  GpioDisableInterrupt(ACCEL_INT2_PORT, ACCEL_INT2_PIN);
#endif
}


/**
 * @brief     Configure the accelerometer for device sleep
 *            - 2g range for wake up sensitivity
 *            - Interrupt enabled
 * @param     None
 * @return    None
 */
void AccelDriverSleepConfig (void)
{
  /* set FIFO to BYPASS mode so we dont buffer data while we sleep (and at the wrong range) */
  drvSpiWriteReg(LSM6DSL_FIFO_CTRL5, ODR_FIFO_104HZ | FIFO_MODE_BYPASS);

  /* set the accelerometer range to +/-2g, to allow sensitive wake up */
  drvSpiWriteReg(LSM6DSL_CTRL1_XL, ODR_XL_52HZ | XL_FS_2G);

  /* set Gyro off,  full-scale +/- 2000dps */
  drvSpiWriteReg(LSM6DSL_CTRL2_G, ODR_G_0HZ | G_FS_2000DPS);

  /* disable high-performance operating mode */
  AccelDriverSetOperationMode(HM_MODE_DISABLE);

  /* enable the wake up interrupt */
  AccelDriverEnableWakeUpInterrupt();
}


/**
 * @brief     Configure the accelerometer for normal operation
 *            - 16g range for impact capture
 *            - Interrupt disabled
 * @param     None
 * @return    None
 */
void AccelDriverActiveConfig (void)
{
  /* disable the interrupt */
  AccelDriverDisableWakeUpInterrupt();

  /* set the accelerometer range to +/-16g, for normal operation */
  drvSpiWriteReg(LSM6DSL_CTRL1_XL, ODR_XL_104HZ | XL_FS_16G);

  /* set Gyro back for normal operation */
  drvSpiWriteReg(LSM6DSL_CTRL2_G, ODR_G_104HZ | G_FS_2000DPS);

  /* enable high-performance operating mode */
  AccelDriverSetOperationMode(HM_MODE_ENABLE);

  /*
   * A short delay is required to allow the sensitivity setting to take effect before we re-enable the FIFO
   * otherwise we get a couple of spurious readings in the FIFO
   */
  sleep(FIFO_RESET_DELAY_MS);

  /* reset the FIFO and set back to continuous mode */
  AccelDriverResetFifo();
}


/**
 * @brief     Configure the accelerometer Sleeping Threshold
 *            for the wake-up interrupt
 *            Config Store have the Algo selection
 *            
 * @param     None
 * @return    None
 */
void AccelDriverWakeupThresholdConfig (void)
{
    /* Check which Algo to use, the Accel Threshold value is different */
    if (ConfigGetSOJUseNewAlgo())
    {
      drvSpiWriteReg(LSM6DSL_WAKE_UP_THS, WAKE_UP_THRESHOLD_2);
    }
    else
    {
      drvSpiWriteReg(LSM6DSL_WAKE_UP_THS, WAKE_UP_THRESHOLD);
    }
    drvSpiWriteReg(LSM6DSL_WAKE_UP_DUR, WAKE_UP_DURATION);  // duration
    drvSpiWriteReg(LSM6DSL_MD2_CFG, INT_WU);                // int2 = wake-up
    drvSpiWriteReg(LSM6DSL_TAP_CFG, INT_ENABLE | INT_LATCHING);  // interrupt enabled and set to latching
}


/**
 ******************************************************************************
 *                              PRIVATE FUNCTIONS
 ******************************************************************************
 */

/**
 * @brief     Set accelerometer performance operating mode
 * @param     mode  : 0 default high mode, 1 to disable high-performance operating mode
 * @return    None
 */
static void AccelDriverSetOperationMode (uint8_t mode)
{
  uint8_t readByte;
  drvSpiReadReg(LSM6DSL_CTRL6_C, &readByte);

  if (mode == HM_MODE_DISABLE)
  {
    drvSpiWriteReg(LSM6DSL_CTRL6_C, (readByte | XL_HM_MODE_DISABLE)); /* set to disable HM mode */
  }
  else
  {
    drvSpiWriteReg(LSM6DSL_CTRL6_C, (readByte | XL_HM_MODE_ENABLE));  /* back to default */
  }
}


/**
 * @brief     Clear the wake up interrupt flags in the IMU
 * @param     None
 * @return    true = success, false = error
 */
static bool AccelDriverClearMotionFlags (void)
{
  uint8_t readByte;
  bool success = false;

  /* Read function interrupt source from accelerometer. */
  if (drvSpiReadReg(LSM6DSL_WAKE_UP_SRC, &readByte))
  {
    success = true;
  }
  return success;
}



/**
 * @brief     Read an 8-bit register. Uses blocking mode.
 * @param     The register address
 * @param     Pointer to a byte which will hold the value read
 * @return    true on success, false on failure
 */
static bool drvSpiReadReg (uint8_t registerNumber, uint8_t *pData)
{
  bool retval;
  uint8_t writeData[2];

  writeData[0] = SPI_READ_CMD | registerNumber;
  writeData[1] = 0x00;

  /* make the transfer */
  SpiHandle.TransferType = SPI_TRANSFER_TXRX;
  retval = SpiTransfer(&SpiHandle, writeData, 1, pData, 1);

  return retval;
}



/**
 * @brief     Write an 8-bit register. Uses blocking mode.
 * @param     The register address
 * @param     Pointer to the byte to be written
 * @return    true on success, false on failure
 */
static bool drvSpiWriteReg (uint8_t registerNumber, uint8_t dataByte)
{
  bool retval;
  uint8_t writeData[2];

  writeData[0] = SPI_WRITE_CMD | registerNumber;
  writeData[1] = dataByte;

  SpiHandle.TransferType = SPI_TRANSFER_TX;
  retval = SpiTransfer(&SpiHandle, writeData, 2, NULL, 0);

  return retval;
}


/**
 * @brief     Read a block of data. Uses blocking mode.
 * @param     The register address
 * @param     Pointer to a buffer which will hold the values read
 * @return    true on success, false on failure
 */
static bool drvSpiReadMultibyte (uint8_t registerNumber, uint8_t *pData, uint16_t length)
{
  bool retval = false;

  if (length <= SPI_MAX_DATA_LENGTH)
  {
    /* set up the read command and read buffer */
    uint8_t writeBuffer[1];
    writeBuffer[0] = SPI_READ_CMD | registerNumber;
    memset(spiReadBuffer, 0, sizeof(spiReadBuffer));

    /* make the transfer */
    SpiHandle.TransferType = SPI_TRANSFER_TXRX;
    retval = SpiTransfer(&SpiHandle, writeBuffer, 1, spiReadBuffer, length);

    if (retval)
    {
      /* Copy data out of buffer */
      memcpy(pData, &spiReadBuffer, length);  // Is this really needed?  Could we read directly into pData?
    }
  }

  return retval;
}


/**
 * @brief  EXTI line detection callback.
 * @param  None
 * @retval None
 */
void AccelDriverCallback (void)
{
  AccelWakeUp = true;
}



/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       Read and display the value of a accelerometer register
 * @param[in]   argCount    : The number of arguments passed from the CLI
 * @param[in]   pArguments  : The list of arguments passed from the CLI
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int AccelDriverConsolePeek (int argCount, char **pArguments)
{
  int registerAddress;
  uint8_t data = 0;

  /* check for the correct number of arguments */
  if (1 == argCount)
  {
    /* get the address */
    sscanf(pArguments[0],  "%x", &registerAddress);

    /* check for valid address */
    if ((registerAddress < 0) || (registerAddress > 0x75))
    {
      return (CONSOLE_ARG_RANGE);
    }

    /* Read the register */
    if (drvSpiReadReg((uint8_t)registerAddress, &data))
    {
      ConsolePrintf("\nAddr 0x%02x = 0x%02x\n\r", registerAddress, (int)data);
    }
    else
    {
      ConsolePrintf("Error reading data\n\r");
    }

    return(CONSOLE_ERROR_OK);
   }

  return(CONSOLE_ARG_NO);
}


/**
 * @brief       Write the given data value into a accelerometer register
 * @param[in]   argCount    : The number of arguments passed from the CLI
 * @param[in]   pArguments  : The list of arguments passed from the CLI
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int AccelDriverConsolePoke (int argCount, char **pArguments)
{
  int registerAddress;
  int data;

  /* check for correct number of arguments */
  if (2 == argCount)
  {
    /* get the address and data */
    sscanf(pArguments[0],  "%x", &registerAddress);
    sscanf(pArguments[1],  "%x", &data);

    /* check for valid address */
    if ((registerAddress < 0) || (registerAddress > 0x75))
    {
      return (CONSOLE_ARG_RANGE);
    }

    /* Write the register */
    if (drvSpiWriteReg((uint8_t)registerAddress, (uint8_t)data))
    {
      /* print the status data */
      ConsolePrintf("\nAddr 0x%02x = 0x%02x\n\r", registerAddress, data);
    }
    else
    {
      ConsolePrintf("Error writing data\n\r");
    }
    return (CONSOLE_ERROR_OK);
   }

   return (CONSOLE_ARG_NO);
}


/**
 * @brief       Read and display multiple data from the accelerometer
 * @param[in]   argCount    : The number of arguments passed from the CLI
 * @param[in]   pArguments  : The list of arguments passed from the CLI
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int AccelDriverConsoleRead (int argCount, char **pArguments)
{
  int registerAddress;
  int ReadLength;
  uint8_t data[16];

  /* check for the correct number of arguments */
  if (2 == argCount)
  {
    /* get the address and length */
    sscanf(pArguments[0],  "%x", &registerAddress);
    sscanf(pArguments[1],  "%x", &ReadLength);

    /* check for valid address */
    if ((registerAddress < 0) || (registerAddress > 0x75))
    {
      return (CONSOLE_ARG_RANGE);
    }

    /* check for valid length */
    if ((ReadLength < 0) || (ReadLength > 16))
    {
      return (CONSOLE_ARG_RANGE);
    }

    /* Read the data */
    if (drvSpiReadMultibyte ((uint8_t)registerAddress, data, ReadLength))
    {
      ConsolePrintf("\nAddr 0x%02x =", registerAddress);
      for (int i = 0; i < ReadLength; i++)
      {
        ConsolePrintf(" 0x%02x", (int)data[i]);
      }
      ConsolePrintf("\n\r");
    }
    else
    {
      ConsolePrintf("Error reading data\n\r");
    }

    return(CONSOLE_ERROR_OK);
   }

  return(CONSOLE_ARG_NO);
}

