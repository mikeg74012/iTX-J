/**
 **************************************************************************************************
 * @file   gnssevent.c
 **************************************************************************************************
 *
 * @author Ripcord Designs - UA
 * @date   15/05/2020
 *
 * @brief  Source File - Handles GNSS-Event storage and retrieve from SRAM.
 *
 **************************************************************************************************
 */

#include "global.h"


/*
 **************************************************************************************************
 *                                         PRIVATE DEFINES
 **************************************************************************************************
 */
#define GNSS_FREQUENCY_HZ            1
#define GNSS_BUFFER_SIZE_IN_SECONDS  (2*60*60)  // 2 hours of data
#define NO_OF_GNSS_EVENTS_IN_BUFFER  (GNSS_FREQUENCY_HZ * GNSS_BUFFER_SIZE_IN_SECONDS)


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
static uint32_t gnssEventReadIndex = 0;
static uint32_t gnssEventWriteIndex = 0;
static uint32_t gnssEventCount = 0;


/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static bool WriteGnssEventDataToSpiRAM  (const uint32_t eventstorageindex, const GnssEventMotionData_t *pBuffer);
static bool ReadGnssEventDataFromSpiRAM (const uint32_t eventstorageindex, GnssEventMotionData_t *pBuffer);
static void GnssEventIncrementIndex     (uint32_t * pCurrentIndex);


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
/**
 * @brief     Add one GNSS data record to SPI SRAM
 * @param     Pointer to GNSS data element
 * @return    None
 */
void GnssEventWrite (GnssEventMotionData_t *pBuffer)
{
  if (pBuffer != NULL)
  {
    /* Store GNSS-Event in SPI RAM */
    if (WriteGnssEventDataToSpiRAM(gnssEventWriteIndex, pBuffer))
    {
      /* increment the write index, wrap if end of buffer reached */
      GnssEventIncrementIndex(&gnssEventWriteIndex);

      /*
       * if the write index has caught up with the read index (buffer full)
       * then we increment the read index (lose the oldest data)
       */
      if (gnssEventWriteIndex == gnssEventReadIndex)
      {
        GnssEventIncrementIndex(&gnssEventReadIndex);
      }
      else
      {
        gnssEventCount++;
      }
    }
  }
}


/**
 * @brief     Read one GNSS event record from SPI RAM (FIFO)
 * @param     Pointer to a buffer to hold the Event data
 * @return    True if successful, false otherwise
 */
bool GnssEventRead (GnssEventMotionData_t *pBuffer)
{
  bool result = false;

  if (pBuffer != NULL)
  {
    /* Read GNSS-Event in SPI RAM */
    if (ReadGnssEventDataFromSpiRAM(gnssEventReadIndex, pBuffer))
    {
      result = true;

      /* increment read index, wrap to start of buffer when end reached */
      GnssEventIncrementIndex(&gnssEventReadIndex);

      /* update the number of events in the buffer */
      if (gnssEventCount > 0)
      {
        gnssEventCount--;
      }
    }
  }
  return result;
}


/**
 * @brief    Read number of GNSS records stored in SPI RAM
 * @param    none
 * @return   uint32_t gnssEventCount
 */
uint32_t GnssEventGetRecordCount (void)
{
  return gnssEventCount;
}

/**
 * @brief    Get number of GNSS records stored size in SPI RAM
 * @param    none
 * @return   uint32_t total No of event record
 */
uint32_t GnssEventGetCountSize (void)
{
  return (NO_OF_GNSS_EVENTS_IN_BUFFER);
}


/**
 * @brief    Read current GNSS records write index
 * @param    none
 * @return   uint32_t gnssEventWriteIndex
 */
uint32_t GnssEventWriteIndex (void)
{
  return (gnssEventWriteIndex);
}

/**
 * @brief     Read one GNSS event record from SPI RAM (FIFO)
 *            without change the ReadIndex and EventCount
 * @param     gnssEventReadIndex: read event index
 * @param     Pointer to a buffer to hold read back Event data
 * @return    True if successful, false otherwise
 */
bool GnssEventGet (uint32_t gnssEventReadIndex, GnssEventMotionData_t *pBuffer)
{
  bool result = false;

  if ((pBuffer != NULL) && (gnssEventCount > 0))
  {
    /* Read GNSS-Event in SPI RAM */
    if (ReadGnssEventDataFromSpiRAM(gnssEventReadIndex, pBuffer))
    {
      result = true;
    }
  }
  return result;
}


/**
 * @brief     Read latest index of Gnss event data stored SPI RAM without removing from the buffer
 * @param     Pointer to a buffer to hold the Event data
 * @return    True if successful, false otherwise
 */
bool GnssEventReadLatest (GnssEventMotionData_t *pBuffer)
{
  bool result = false;

  if ((pBuffer != NULL) && (gnssEventCount > 0))
  {
    if (ReadGnssEventDataFromSpiRAM(gnssEventWriteIndex, pBuffer))
    {
      result = true;
    }
  }
  return result;
}


/**
 * @brief     Read one GNSS event record from SPI RAM (FIFO)
 *              without change the gnssEventReadIndex and EventCount
 * @param     Pointer to a buffer to hold the Event data
 * @return    True if successful, false otherwise
 */
bool GnssEventPeek (GnssEventMotionData_t *pBuffer)
{
  bool result = false;

  if (pBuffer != NULL)
  {
    /* Read GNSS-Event in SPI RAM */
    if (ReadGnssEventDataFromSpiRAM(gnssEventReadIndex, pBuffer))
    {
      result = true;
    }
  }
  return result;
}


/**
 * @brief     Reset the GNSS event store.  Should be done at the start of each journey to make sure
 *            old / stale data is not sent as part of a new journey.
 * @param     None
 * @return    None
 */
void GnssEventReset (void)
{
  gnssEventReadIndex = 0;
  gnssEventWriteIndex = 0;
  gnssEventCount = 0;
}


/*
 **************************************************************************************************
 *                                        PRIVATE SECTION
 **************************************************************************************************
 */

/**
 * @brief     Write a GNSS event to SPI RAM
 * @param     The storage index to use
 * @param     Pointer to a buffer containing the data to write (GnssEventMotionData_t)
 * @return    True if successful, false otherwise
 */
static bool WriteGnssEventDataToSpiRAM (const uint32_t eventstorageindex, const GnssEventMotionData_t *pBuffer)
{
  bool success = false;

  if ((pBuffer != NULL) && (eventstorageindex <= NO_OF_GNSS_EVENTS_IN_BUFFER))
  {
    /* calculate the storage address in SPI RAM */
    uint32_t Address = SERIAL_RAM_GNSS_EVENT_AREA_START + (eventstorageindex * sizeof(GnssEventMotionData_t));

    if (SerialRamWrite(Address, (uint8_t*) pBuffer, sizeof(GnssEventMotionData_t)))
    {
      success = true;
    }
  }

  if (!success)
  {
    ConsolePrintf("Error writing to Gnss Event Buffer Index %u\n\r", eventstorageindex);
  }
  return success;
}


/**
 * @brief     Read a GNSS event info header from SPI RAM
 * @param     The storage index to use
 * @param     Pointer to a buffer to hold the Event data
 * @return    True if successful, false otherwise
 */
static bool ReadGnssEventDataFromSpiRAM (const uint32_t eventstorageindex, GnssEventMotionData_t *pBuffer)
{
  bool success = false;

  if ((pBuffer != NULL) && (eventstorageindex <= NO_OF_GNSS_EVENTS_IN_BUFFER))
  {
    /* calculate the storage address in SPI RAM */
    uint32_t Address = SERIAL_RAM_GNSS_EVENT_AREA_START + (eventstorageindex * sizeof(GnssEventMotionData_t));

    if (SerialRamRead(Address, (uint8_t*) pBuffer, sizeof(GnssEventMotionData_t)))
    {
      success = true;
    }
  }

  if (!success)
  {
    ConsolePrintf("Error reading from Gnss Event Buffer Index %u\n\r", eventstorageindex);
  }
  return success;
}


/**
 * @brief     Increment the index passed as a parameter, wrapping if the end of buffer is reached
 * @param     pCurrentIndex : pointer to the index to increment
 * @return    none
 */
static void GnssEventIncrementIndex (uint32_t * pCurrentIndex)
{
  (*pCurrentIndex)++;

  if (*pCurrentIndex >= NO_OF_GNSS_EVENTS_IN_BUFFER)
  {
    *pCurrentIndex = 0;
  }
}

/**
 * @brief   Store the CURRENT GNSS data and CURRENT accel Data to SPI SRAM
 * @param   None
 * @return  None
 */
void GnssEventSaveToSram (void)
{
  GnssEventMotionData_t gnssEventMotionData;

  /* Get the Filtered XYZ value */
  XyzGetFilteredData(&gnssEventMotionData.accelData);
  
  /* Get the current Gnss Data */
  GsmGnssGetCurrentGnss(&gnssEventMotionData.gnssData);

  /* Write the complete structure to SPI SRAM */
  GnssEventWrite(&gnssEventMotionData);
}


/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       Prints all GNSS event data from SRAM to console
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int GnssEventConsoleList (int ArgCount, char **pArguments)
{
  static GnssEventMotionData_t gnssMotionBuffer = { 0 };

  if (gnssEventCount == 0)
  {
    ConsolePrintf("No GNSS-Event Info \n\r");
  }
  else
  {
    for (int i = 0; i < gnssEventCount; i++)
    {
      if (ReadGnssEventDataFromSpiRAM(i, &gnssMotionBuffer))
      {
        ConsolePrintf("GNSS-Event %u - %ld %ld %d %u, time = %d X,Y,Z= %d %d %d\n\r", i, gnssMotionBuffer.gnssData.Latitude, gnssMotionBuffer.gnssData.Longitude,
            gnssMotionBuffer.gnssData.Heading, gnssMotionBuffer.gnssData.Speed, gnssMotionBuffer.gnssData.TimeStamp, 
            gnssMotionBuffer.accelData.X, gnssMotionBuffer.accelData.Y, gnssMotionBuffer.accelData.Z);
      }
      else
      {
        ConsolePrintf("Error getting GNSS-Event %u\n\r", i);
      }
      if (0 == i % 10)
      {
        sleep(50);
      }
    }
  }

  /* indicate success */
  return (CONSOLE_ERROR_OK);
}
