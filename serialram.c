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
void SerialRamInit (void)
{
  /* Configure the SPI bus & chip select for this device */
//  SerialRamSpiHandle.BitRate = SERIAL_RAM_SPI_BITRATE;
  SpiInitialise(PSRAM_SPI);

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
    retval = SpiTransfer(PSRAM_SPI, writeBuffer, 4, pBuffer, size, SPI_TRANSFER_TXRX);
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
    retval = SpiTransfer(PSRAM_SPI, writeBuffer, 4, pBuffer, size, SPI_TRANSFER_TXTX);
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
  retval = SpiTransfer(PSRAM_SPI, writeBuffer, 2, NULL, 0, SPI_TRANSFER_TX);
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




