/**
 **************************************************************************************************
 * @file   serialram.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  Driver for the serial pSRAM chip
 *
 **************************************************************************************************
 * @attention                   (c) Ripcord Designs Limited 2019
 **************************************************************************************************
 */
#include "global.h"

/*
 **************************************************************************************************
 *                                       PRIVATE DEFINES
 **************************************************************************************************
 */
/* SPI bus settings for this device */
#define SERIAL_RAM_SPI_MODE               0
#define SERIAL_RAM_SPI_BITRATE            8000000

/* SRAM test build switch. Uncomment to test entire RAM address range */
//#define PSRAM_FULL_TEST

/* SPI transaction timeout */
#define SPI_TIMEOUT_MS          500

/* Serial RAM Instruction Set */
#define PSRAM_INSTR_READ       0x03
#define PSRAM_INSTR_WRITE      0x02
#define PSRAM_INSTR_EDIO       0x3B
#define PSRAM_INSTR_EQIO       0x38
#define PSRAM_INSTR_RSTIO      0xFF
#define PSRAM_INSTR_RDMR       0x05
#define PSRAM_INSTR_WRMR       0x01

/* Mode Register Options */
#define PSRAM_MODE_BYTE        0x00
#define PSRAM_MODE_PAGE        0x80
#define PSRAM_MODE_SEQUENTIAL  0x40

/* self test data block size */
#define TEST_DATA_SIZE_BYTES    128


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
static spihandle_t SerialRamSpiHandle;


/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static bool              SerialRamSetMode       (uint8_t mode);
static bool              SerialRamTest          (void);


/*
 **************************************************************************************************
 *                                   PUBLIC SECTION / INTERFACE
 **************************************************************************************************
 */
/**
 * @brief     Initialise the serial pSRAM module
 * @param     SpiBus :  the SPI bus to use
 * @param     ChipSelect: the chip select to use
 * @return    none
 */
void SerialRamInit (spibus_e SpiBus, spichipselect_t ChipSelect)
{
  /* Configure the SPI bus & chip select for this device */
  SerialRamSpiHandle.Bus = SpiBus;
  SerialRamSpiHandle.ChipSelect = ChipSelect;
  SerialRamSpiHandle.Mode = SERIAL_RAM_SPI_MODE;
  SerialRamSpiHandle.BitRate = SERIAL_RAM_SPI_BITRATE;
  SpiInitialise(&SerialRamSpiHandle);

#if SERIAL_RAM_TYPE==SERIAL_RAM_MICROCHIP
  /* Set the mode to sequential reads / writes */
  SerialRamSetMode(PSRAM_MODE_SEQUENTIAL);
#endif
  /* Carry out Wake-Up Self-Test if required */
  PostWakeUpSelfTest(SRAM);
}




/**
 * @brief     Read data from the serial pSRAM
 * @param     pBuffer : pointer to the buffer to copy the data into
 * @param     size    : number of bytes to read
 * @return    bool    : true on success, false on failure
 */
bool SerialRamRead (uint32_t address, uint8_t *pBuffer, int size)
{
  bool retval = false;
  uint8_t writeBuffer[4];

  if ((address + size) < SRAM_SIZE_BYTES)
  {
    /* Set up the read instruction and memory address */
    writeBuffer[0] = PSRAM_INSTR_READ;
    writeBuffer[1] = ((address >> 16) & 0xFF);
    writeBuffer[2] = ((address >> 8) & 0xFF);
    writeBuffer[3] = ((address >> 0) & 0xFF);

    /* make the transfer */
    SerialRamSpiHandle.TransferType = SPI_TRANSFER_TXRX;
    retval = SpiTransfer(&SerialRamSpiHandle, writeBuffer, 4, pBuffer, size);
  }
  return (retval);
}


/**
 * @brief     Write data to the pSRAM
 * @param     pBuffer : pointer to the buffer to copy the data from
 * @param     size    : number of bytes to write
 * @return    bool    : true on success, false on failure
 */
bool SerialRamWrite (uint32_t address, uint8_t *pBuffer, int size)
{
  bool retval = false;
  uint8_t writeBuffer[4];

  if ((address + size) < SRAM_SIZE_BYTES)
  {
    /* Set up the write instruction and memory address */
    writeBuffer[0] = PSRAM_INSTR_WRITE;
    writeBuffer[1] = ((address >> 16) & 0xFF);
    writeBuffer[2] = ((address >> 8) & 0xFF);
    writeBuffer[3] = ((address >> 0) & 0xFF);

    /* make the transfer */
    SerialRamSpiHandle.TransferType = SPI_TRANSFER_TXTX;
    retval = SpiTransfer(&SerialRamSpiHandle, writeBuffer, 4, pBuffer, size);
  }
  return (retval);
}


/**
 * @brief     Serial RAM Wake Up Self Test Routine
 * @param     none
 * @return    true - on success
 *            false - on failure
 */
bool SerialRamWUST(void)
{
  return SerialRamTest();
}


/*
 **************************************************************************************************
 *                                        PRIVATE SECTION
 **************************************************************************************************
 */

/**
 * @brief     Write to the MODE register
 * @param     mode - the new mode to write to the register.  Can be Byte,
 *            Page or Sequential mode
 * @return    HAL_OK on success else a HAL error
 */
static bool SerialRamSetMode (uint8_t mode)
{
  bool retval = false;
  uint8_t writeBuffer[2];

  /* Set up the mode command */
  writeBuffer[0] = PSRAM_INSTR_WRMR;
  writeBuffer[1] = mode;

  /* make the transfer */
  SerialRamSpiHandle.TransferType = SPI_TRANSFER_TX;
  retval = SpiTransfer(&SerialRamSpiHandle, writeBuffer, 2, NULL, 0);
  return (retval);
}


/**
 * @brief     Test the entire SRAM device can be written and read
 * @param     none
 * @return    true - on success
 *            false - on failure
 */
static bool SerialRamTest (void)
{
  uint8_t ramWriteData[TEST_DATA_SIZE_BYTES];
  uint8_t ramReadData[TEST_DATA_SIZE_BYTES];

#ifdef PSRAM_FULL_TEST
  int StartAddress = 0;
  int EndAddress = SRAM_SIZE_BYTES;
#else
  int StartAddress = SERIAL_RAM_SELF_TEST_AREA_START;
  int EndAddress = StartAddress + TEST_DATA_SIZE_BYTES;
#endif

  /* initialise the test data */
  for (int i = 0; i < TEST_DATA_SIZE_BYTES; i++)
  {
    ramWriteData[i] = (uint8_t) i;
  }

  /* run the test one block at a time */
  int CurrentAddress = StartAddress;
  while ((CurrentAddress + TEST_DATA_SIZE_BYTES) <= EndAddress)
  {
    /* write then read back a block of data */
    SerialRamWrite(CurrentAddress, &ramWriteData[0], TEST_DATA_SIZE_BYTES);
    memset(ramReadData, 0, TEST_DATA_SIZE_BYTES);
    SerialRamRead(CurrentAddress, &ramReadData[0], TEST_DATA_SIZE_BYTES);

    /* Check that the data matches */
    for (int i = 0; i < TEST_DATA_SIZE_BYTES; i++)
    {
      if (ramWriteData[i] != ramReadData[i])
      {
        return false;
      }
    }
    CurrentAddress += TEST_DATA_SIZE_BYTES;
  }

  return true;
}


/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       Clear an address range to zero
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int SerialRamConsoleClear (int ArgCount, char **pArguments)
{
  int Address;
  int Length;
  int WriteSize;
  uint8_t Buffer[16];

  if (2 == ArgCount)
  {
    /* get the date and time from the command line */
    sscanf(pArguments[0],  "%d", &Address);
    sscanf(pArguments[1],  "%d", &Length);

    /* clear the buffer */
    memset(Buffer, 0, 16);

    /* read the SPI RAM and display the data in hex format */
    while (Length)
    {
      if (Length > 16)
      {
        WriteSize = 16;
      }
      else
      {
        WriteSize = Length;
      }
      SerialRamWrite(Address, Buffer, WriteSize);
      Length = Length - WriteSize;
      Address = Address + WriteSize;
    }
  }

  /* indicate success */
  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       Read an address range
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int SerialRamConsoleRead (int ArgCount, char **pArguments)
{
  int Address;
  int Length;
  int ReadSize;
  uint8_t Buffer[16];

  if (2 == ArgCount)
  {
    /* get the date and time from the command line */
    sscanf(pArguments[0],  "%d", &Address);
    sscanf(pArguments[1],  "%d", &Length);

    /* read the SPI RAM and display the data in hex format */
    while (Length)
    {
      if (Length > 16)
      {
        ReadSize = 16;
      }
      else
      {
        ReadSize = Length;
      }
      Length = Length - ReadSize;
      if (SerialRamRead(Address, Buffer, ReadSize))
      {
        for (int i = 0; i < ReadSize; i++)
        {
          ConsolePrintf(" %02x", Buffer[i]);
        }
        ConsolePrintf("    ");
        for (int i = 0; i < ReadSize; i++)
        {
          if ((Buffer[i] < 32) || (Buffer[i] > 126))
          {
            Buffer[i] = ' ';
          }
          ConsolePrintf("%c", Buffer[i]);
        }
        ConsolePrintf("\n\r");
      }
      Address = Address + ReadSize;
    }
    /* indicate success */
    return (CONSOLE_ERROR_OK);
  }
  return (CONSOLE_ARG_NO);
}


/**
 * @brief       Perform a self test of the RAM
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int SerialRamConsoleTest (int ArgCount, char **pArguments)
{
  ConsolePrintf("RAM test...");

  /*
   * Perform a self test of the RAM
   * NOTE: To avoid upsetting the system, this uses a "test" address range that
   *       is not used in normal operation
   */
  if (SerialRamTest())
  {
    ConsolePrintf("PASSED\n\r");
  }
  else
  {
    ConsolePrintf("FAILED\n\r");
  }

  /* indicate success */
  return (CONSOLE_ERROR_OK);
}


