/**
 **************************************************************************************************
 * @file   eventstore.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  Impact and Mini-Event storage
 *
 **************************************************************************************************
 */

#include "global.h"

/*
 **************************************************************************************************
 *                                         PRIVATE DEFINES
 **************************************************************************************************
 */
/* 256 bytes Maximum size allowed for the summary header information */
#define MAX_SUMMARY_INFO_SIZE             (0x100)

/* SPI SRAM Addresses used by this module */
#define IMPACTEVENT_SUMMARY_ADDRESS       SERIAL_RAM_IMPACT_EVENT_AREA_START
#define IMPACTEVENT_DATA_ADDRESS          (SERIAL_RAM_IMPACT_EVENT_AREA_START + MAX_SUMMARY_INFO_SIZE)

/* Needs to be able to store more than one mini-event */
#define MINIEVENT_SUMMARY_ADDRESS         SERIAL_RAM_MINI_EVENT_AREA_START
#define MINIEVENT_DATA_ADDRESS            (SERIAL_RAM_MINI_EVENT_AREA_START + MAX_SUMMARY_INFO_SIZE)

/* Decimation factor used when sending mini-event data to cloud */
#define DECIMATION_FACTOR                 10


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
/* buffer indexes for writing and reading IMU data records to the impact store */
static int  RecordWriteIndex = 0;
static int  RecordReadIndex = 0;

/* Number of events of each type currently stored */
static int  ImpactEventCount = 0;
static int  MiniEventCurrentIndex = 0;

/* Temporary storage of IMU data during decimation */
static ImuData_t ImuData[DECIMATION_FACTOR];


/*
 **************************************************************************************************
 *                                   PRIVATE FUNCTION PROTOTYPES
 **************************************************************************************************
 */
static bool     EventStoreWriteSummary        (EventStore_e EventStore, EventSummary_t EventSummary);
static bool     EventStoreCopyMotionBuffer    (EventStore_e EventStore);
static uint32_t EventStoreGetSummaryAddress   (EventStore_e EventStore, EventSummary_t * pEventSummary);
static uint32_t EventStoreGetDataAddress      (EventStore_e EventStore, EventSummary_t * pEventSummary);
static void     EventStoreIncrementEventCount (EventStore_e EventStore);
static void     EventStoreDecrementEventCount (EventStore_e EventStore);


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */

/**
 * @brief   Initialize the event store module to a known state
 * @param   none
 * @return  none
 */
void EventStoreInitialize (void)
{
  EventSummary_t EventSummary;

  /* clear event counters / indices */
  ImpactEventCount = 0;
  MiniEventCurrentIndex = 0;

  /* invalidate all mini event records */
  for (int EventId = 0; EventId < MAX_MINI_EVENTS; EventId++)
  {
    EventSummary.EventID = EventId;
    EventStoreDeleteEvent(EVENT_STORE_MINI, &EventSummary);
  }
}


/**
 * @brief   Restore the event store module to a known state after LP sleep
 * @param   none
 * @return  none
 */
void SetImpactEventCount (int eCount)
{
  /* clear event counters / indices */
  ImpactEventCount = eCount;
}


/**
 * @brief   Restore the event store module to a known state after LP sleep
 * @param   none
 * @return  none
 */
void SetMiniEventIndex (int eIndex)
{
  MiniEventCurrentIndex = eIndex;
}


/**
 * @brief  Return the event store count to PSRAM, update the value before write action
 * @param   none
 * @return  none
 */
int GetImpactEventCount (void)
{
  return (ImpactEventCount);
}


/**
 * @brief  Return the event store current index module to PSRAM, update the value before write action
 * @param   none
 * @return  none
 */
int GetMiniEventIndex (void)
{
  return (MiniEventCurrentIndex);
}


/**
 * @brief   Write the current Motion Buffer to SPI RAM, along with time and location of event
 * @param   EventStore : The store (impact or mini-ev) to store the motion buffer to
 * @return  bool       : true on success, false on failure
 */
bool EventStoreWrite (EventStore_e EventStore, EventSummary_t EventSummary)
{
  bool result = true;

  /* Do not overwrite existing impact data that has not been processed */
  if ((EventStore == EVENT_STORE_IMPACT) && (ImpactEventCount > 0))
  {
    TRACE_IMU("Error Event store already contains unprocessed data\n\r");
    result = false;
  }

  if (result)
  {
    /* Reset the record write and read indexes */
    RecordWriteIndex = 0;
    RecordReadIndex = 0;

    /* Write the summary information */
    result = EventStoreWriteSummary(EventStore, EventSummary);
  }

  /* Copy the motion buffer data */
  if (result)
  {
    uint32_t preMag = GetImpactMaxMagnitude();
    result = EventStoreCopyMotionBuffer(EventStore);
    /* increment the number of stored events */
    EventStoreIncrementEventCount(EventStore);
    if (EventStore == EVENT_STORE_IMPACT)
    {
      ConsolePrintf("Impact Peak Magnitude updated from %d to %d\r\n", preMag, GetImpactMaxMagnitude());
    }
  }

  /* check and update the peak Speed value */
  if ((result) && (EventStore == EVENT_STORE_IMPACT))
  {
    uint32_t preSpeed = GetImpactMaxSpeed();
    ImpactPeakSpeedUpdate (EventSummary.TimeStamp);
    ConsolePrintf("Impact Peak Speed updated from %d to %d for TS:%d\r\n", preSpeed, GetImpactMaxSpeed(),EventSummary.TimeStamp);
  }

  return result;
}


/**
 * @brief   Read summary information about the impact (time and location) from SPI RAM.
 *          MUST be called before reading any event data records from the store
 * @param   EventStore    : The store (impact or mini-ev) to store the motion buffer to
 * @param   pEventSummary : pointer to buffer to put the summary in
 * @return  bool          : true on success, false on failure
 */
bool EventStoreReadSummary (EventStore_e EventStore, EventSummary_t * pEventSummary)
{
  uint32_t SummaryAddress = EventStoreGetSummaryAddress(EventStore, pEventSummary);

  /* Read the summary data from SPI SRAM into the supplied buffer */
  if (false == SerialRamRead(SummaryAddress, (uint8_t *) pEventSummary, sizeof(EventSummary_t)))
  {
    TRACE_IMU("Error reading Event Summary from SPI SRAM\n\r");
    return false;
  }

  return true;
}


/**
 * @brief   Read one or more sets of IMU data from the event store in SPI RAM
 * @param   EventStore      : The store (impact or mini-ev) to read data from
 * @param   pImuData        : pointer to buffer to put the data in
 * @param   numberOfRecords : the number of records to read
 * @param   pEventSummary   : pointer to buffer containing the summary data
 * @return  bool            : true on success, false on failure
 */
bool EventStoreReadRecords (EventStore_e EventStore, ImuData_t * pImuData, int numberOfRecords, EventSummary_t * pEventSummary)
{
  uint32_t DataAddress = EventStoreGetDataAddress(EventStore, pEventSummary);

  /* make sure we have the requested amount of data */
  if ((RecordReadIndex + numberOfRecords) <= ELEMENTS_IN_IMU_BUFFER)
  {
    /* read the IMU records from the SPI RAM */
    if (true == SerialRamRead((DataAddress + (sizeof(ImuData_t) * RecordReadIndex)),
                              (uint8_t *) pImuData,
                              (sizeof(ImuData_t) * numberOfRecords)))
    {
      /* update the read index */
      RecordReadIndex += numberOfRecords;
      return true;
    }
  }
  TRACE_IMU("Error reading Event data from SPI SRAM\n\r");
  return false;
}

/**
 * @brief   Read one or more sets of IMU data from the event store in SPI RAM without moving Read index pointer
 * @param   EventStore      : The store (impact or mini-ev) to read data from
 * @param   pImuData        : pointer to buffer to put the data in
 * @param   numberOfRecords : the number of records to read
 * @param   offset          : offset record from the first ImuData
 * @param   pEventSummary   : pointer to buffer containing the summary data
 * @return  bool            : true on success, false on failure
 */
bool EventStorePeekRecords (EventStore_e EventStore, ImuData_t * pImuData, int numberOfRecords, int offset, EventSummary_t * pEventSummary)
{
  uint32_t DataAddress = EventStoreGetDataAddress(EventStore, pEventSummary);
  int readIndex = RecordReadIndex + offset;

  /* make sure we have the requested amount of data */
  if ((readIndex + numberOfRecords ) <= ELEMENTS_IN_IMU_BUFFER)
  {
    /* read the IMU records from the SPI RAM */
    if (true == SerialRamRead((DataAddress + (sizeof(ImuData_t) * readIndex)),
                              (uint8_t *) pImuData,
                              (sizeof(ImuData_t) * numberOfRecords)))
    {
      return true;
    }
  }
  TRACE_IMU("Error reading Event data from SPI SRAM for the index of %d\n\r", readIndex);
  return false;
}


/**
 * @brief   Get the number of IMU data records in the store that have not been read
 * @param   EventStore      : The store (impact or mini-ev) to read data from
 * @return  uint32_t  : The number of records currently in the store
 */
uint32_t EventStoreGetRecordCount (EventStore_e EventStore)
{
  int RecordCount = RecordWriteIndex - RecordReadIndex;

  return RecordCount;
}


/**
 * @brief   Get the number of events in a store.
 *          For EVENT_STORE_IMPACT this will be 0 or 1
 *          For EVENT_STORE_MINI this will be 0 to 10
 * @param   EventStore : The store (impact or mini-ev) to read data from
 * @return  uint8_t    : The number of events currently in the store
 */
uint8_t EventStoreGetEventCount (EventStore_e EventStore)
{
  uint8_t EventCount = 0;
  if (EventStore == EVENT_STORE_IMPACT)
  {
    EventCount = (uint8_t) ImpactEventCount;
  }
  else if (EventStore == EVENT_STORE_MINI)
  {
    EventSummary_t EventSummary;
    for (int EvtID = 0; EvtID < MAX_MINI_EVENTS; EvtID++)
    {
      EventSummary.EventID = EvtID;
      EventStoreReadSummary(EVENT_STORE_MINI, &EventSummary);
      /* Note: The EventSummary.EventID is not zero-indexed in SRAM */
      if (EvtID == (EventSummary.EventID - 1))
      {
        EventCount++;
      }
    }
  }
  return EventCount;
}


/**
 * @brief   Read one or more sets of decimated Accelerometer data from the event store in SPI RAM
 *          NOTE: this does NOT return gyroscope data
 * @param   EventStore      : The store (impact or mini-ev) to read data from
 * @param   pImuData        : pointer to buffer to put the data in
 * @param   numberOfRecords : the number of decimated accelerometer records to read
 * @param   pEventSummary   : pointer to buffer containing the summary data
 * @return  bool            : true on success, false on failure
 */
bool EventStoreReadAccelDecimated (EventStore_e EventStore, AccelerationData_t * pBuf, int numberOfRecords, EventSummary_t * pEventSummary)
{
  bool result = true;
  uint32_t DataAddress = EventStoreGetDataAddress(EventStore, pEventSummary);

  RecordReadIndex = 0;
  /* make sure we have the requested amount of data */
  if ((RecordReadIndex + (DECIMATION_FACTOR * numberOfRecords)) > ELEMENTS_IN_IMU_BUFFER)
  {
    TRACE_IMU("Error not enough data available in EventStoreReadAccelDecimated\n\r");
    result = false;
  }
  else
  {
    /* read the IMU records from the SPI RAM, decimate and save to buffer */
    for (int i = 0; i < numberOfRecords; i++)
    {
      /* read the raw data to be decimated into one output record */
      if (true == SerialRamRead((DataAddress + (sizeof(ImuData_t) * RecordReadIndex)),
                                (uint8_t *) &ImuData[0],
                                (sizeof(ImuData_t) * DECIMATION_FACTOR)))
      {
        /*
         * Decimate the data. Do this by selecting the set of data with the largest RMS
         * acceleration in the ten samples.
         */
        uint64_t maxValue = 0;
        uint64_t sumOfSquares;
        int peakIndex = 0;
        for (int j=0; j<DECIMATION_FACTOR; j++)
        {
          sumOfSquares = ((uint64_t)ImuData[j].accel.X * (uint64_t)ImuData[j].accel.X) +
                         ((uint64_t)ImuData[j].accel.Y * (uint64_t)ImuData[j].accel.Y) +
                         ((uint64_t)ImuData[j].accel.Z * (uint64_t)ImuData[j].accel.Z);
          if (sumOfSquares > maxValue)
          {
            maxValue = sumOfSquares;
            peakIndex = j;
          }
        }
        /* Use the peak value */
        pBuf->X = ImuData[peakIndex].accel.X;
        pBuf->Y = ImuData[peakIndex].accel.Y;
        pBuf->Z = ImuData[peakIndex].accel.Z;

        pBuf++;
        RecordReadIndex += DECIMATION_FACTOR;
      }
      else
      {
        TRACE_IMU("Error reading Event data in EventStoreReadAccelDecimated\n\r");
        result = false;
        break;
      }
    }
  }
  return result;
}


/*
 * @brief   Delete an event from the store.
 *          NOTE: The caller must use this function to delete the event once it has been processed
 * @param   EventStore    : The store (impact or mini-ev) to delete event from
 * @param   pEventSummary : pointer to buffer containing the summary data
 * @return  bool          : true on success, false on failure
 */
void EventStoreDeleteEvent (EventStore_e EventStore, EventSummary_t * pEventSummary)
{
  /* Invalidate the specified event */
  if (EventStore == EVENT_STORE_MINI)
  {
    /* Get summary address of event to delete from mini events store */
    uint32_t SummaryAddress = EventStoreGetSummaryAddress(EventStore, pEventSummary);

    /* Invalidate EventID */
    pEventSummary->EventID = (uint8_t) UINT_MAX;

    /* Write the summary data into SPI SRAM */
    if (false == SerialRamWrite(SummaryAddress, (uint8_t *) pEventSummary, sizeof(EventSummary_t)))
    {
      TRACE_IMU("Error deleting Event Summary from SPI SRAM\n\r");
    }
  }
  else
  {
    /* Decrement the number of events in the store */
    EventStoreDecrementEventCount(EventStore);
  }
}


/**
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */

/**
 * @brief   Write summary information about the event (time and location) to SPI RAM.
 * @param   EventStore    : The store (impact or mini-ev) to store event summary data
 * @return  bool          : true on success, false on failure
 */
static bool EventStoreWriteSummary (EventStore_e EventStore, EventSummary_t EventSummary)
{
  /* Set the EventID required for storage of multiple events */
  EventSummary.EventID   = (EventStore == EVENT_STORE_MINI) ? MiniEventCurrentIndex : ImpactEventCount;

  uint32_t SummaryAddress = EventStoreGetSummaryAddress(EventStore, &EventSummary);

  /*
   * Build the summary information.
   * Note: Lat, Long and Time come from the event detection algorithm
   */
  EventSummary.EventID   = MiniEventCurrentIndex + 1;           // mini-event ID not zero indexed

  /* Write the summary data into SPI SRAM */
  if (false == SerialRamWrite(SummaryAddress, (uint8_t *) &EventSummary, sizeof(EventSummary_t)))
  {
    TRACE_IMU("Error writing Event Summary to SPI SRAM\n\r");
    return false;
  }
  return true;
}


/**
 * @brief   Copy the contents of the motion buffer into the SPI SRAM
 * @param   EventStore : The store (impact or mini-ev) to store the motion buffer to
 * @return  bool       : true on success, false on failure
 */
static bool EventStoreCopyMotionBuffer (EventStore_e EventStore)
{
  bool result = false;
  ImuData_t ImuData;
  EventSummary_t EventSummary;

  /* Set the EventID required for storage of multiple events */
  EventSummary.EventID   = (EventStore == EVENT_STORE_MINI) ? MiniEventCurrentIndex : ImpactEventCount;

  uint32_t  BufferStartAddress = EventStoreGetDataAddress(EventStore, &EventSummary);

  /*
   * Simple but slow method currently, copying one entry at a time.  This is because the motion
   * buffer is circular, so we do not have one linear set of data
   * It could be sped up by just doing 2x writes,
   *    - one from read index to end of buffer, and
   *    - one from start of buffer to read index
   */
  MotionBufferFreeze(true);

  for (RecordWriteIndex = 0; RecordWriteIndex < ELEMENTS_IN_IMU_BUFFER; RecordWriteIndex++)
  {
    MotionBufferReadSingle(&ImuData);
    result = SerialRamWrite((BufferStartAddress + (sizeof(ImuData_t) * RecordWriteIndex)), (uint8_t *) &ImuData, sizeof(ImuData_t));
    if (!result)
    {
      TRACE_IMU("Error writing event data to SPI SRAM\n\r");
    }
    else
    {
      if (EventStore == EVENT_STORE_IMPACT)
      {
        /* Update Peak values across the Impact data segment */
        ImpactPeakMagnitudeUpdate(&ImuData);
      }
    }
  }

  MotionBufferFreeze(false);
  return result;
}


/**
 * @brief   Calculate the SPI RAM address of the summary data
 * @param   EventStore    : The store (impact or mini-ev) we want the address of
 * @param   pEventSummary : pointer to buffer containing the summary data
 * @return  uint32_t      : The address in SPI RAM
 */
static uint32_t EventStoreGetSummaryAddress (EventStore_e EventStore, EventSummary_t * pEventSummary)
{
  uint32_t SramAddress = UINT_MAX;

  if (EventStore == EVENT_STORE_IMPACT)
  {
    SramAddress = IMPACTEVENT_SUMMARY_ADDRESS;
  }
  else
  {
    SramAddress =  MINIEVENT_SUMMARY_ADDRESS + (pEventSummary->EventID * MAX_SUMMARY_INFO_SIZE) + (pEventSummary->EventID  * sizeof(ImuData_t) * ELEMENTS_IN_IMU_BUFFER);
  }
  return SramAddress;
}


/**
 * @brief   Calculate the SPI RAM address of the event data data
 * @param   EventStore      : The store (impact or mini-ev) we want the address of
 * @param   pEventSummary   : pointer to buffer containing the summary data
 * @return  uint32_t        : The address in SPI RAM
 */
static uint32_t EventStoreGetDataAddress (EventStore_e EventStore,  EventSummary_t * pEventSummary)
{
  uint32_t SramAddress = UINT_MAX;

  if (EventStore == EVENT_STORE_IMPACT)
  {
    SramAddress = IMPACTEVENT_DATA_ADDRESS;
  }
  else
  {
    SramAddress =  MINIEVENT_DATA_ADDRESS + (pEventSummary->EventID * MAX_SUMMARY_INFO_SIZE) + (pEventSummary->EventID * sizeof(ImuData_t) * ELEMENTS_IN_IMU_BUFFER);
  }
  return SramAddress;
}


/**
 * @brief   Increment the events count for the given store
 * @param   EventStore : The store in question
 * @return  none
 */
static void EventStoreIncrementEventCount (EventStore_e EventStore)
{
  if (EVENT_STORE_IMPACT == EventStore)
  {
    ImpactEventCount = 1;
  }
  else if (EventStore == EVENT_STORE_MINI)
  {
    if (MiniEventCurrentIndex < MAX_MINI_EVENTS - 1)
    {
      MiniEventCurrentIndex++;
    }
    else
    {
      /* Restart CurrentIndex to override oldest data not sent to cloud */
      MiniEventCurrentIndex = 0;
    }
  }
}


/**
 * @brief   Decrement the events count for the given store
 * @param   EventStore : The store in question
 * @return  none
 */
static void EventStoreDecrementEventCount (EventStore_e EventStore)
{
  if (EVENT_STORE_IMPACT == EventStore)
  {
    ImpactEventCount = 0;
  }
}


/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       List Impact event data to the console
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int EventStoreConsoleList (int ArgCount, char **pArguments)
{
  EventSummary_t EventSummary;
  ImuData_t ImuData;

  /* Display the impact summary if any */
  if (EventStoreGetEventCount(EVENT_STORE_IMPACT) > 0)
  {
    RecordReadIndex = 0;
    EventStoreReadSummary(EVENT_STORE_IMPACT, &EventSummary);

    ConsolePrintf("Impact summary:\n\r  time = %d,\n\r  latitude = %d,\n\r  longitude = %d,\n\r  records = %d\n\r",
                EventSummary.TimeStamp,
                EventSummary.Latitude,
                EventSummary.Longitude,
                EventStoreGetRecordCount(EVENT_STORE_IMPACT));

    /* Display the Impact Force Area from integration */
    ConsolePrintf("Impact Force Area:\n\r Area = %d\n\r", DetectEventGetImpactForceArea());

    /* Display the impact data */
    ConsolePrintf("\nImpact data:\n\r AX     AY     AZ     GX     GY     GZ\n\r");

    for (int i = 0; i < 100; i++)
    {
      EventStoreReadRecords(EVENT_STORE_IMPACT, &ImuData, 1, &EventSummary);
      ConsolePrintf("%5d  %5d  %5d  %5d  %5d  %5d\n\r", ImuData.accel.X, ImuData.accel.Y, ImuData.accel.Z,
                                                        ImuData.gyro.X, ImuData.gyro.Y, ImuData.gyro.Z);
      sleep(1);
    }
  }
  else
  {
    ConsolePrintf("\nNo Impact Data\n\r");
  }

  /* Display the mini - event summary if any */
  if (EventStoreGetEventCount(EVENT_STORE_MINI) > 0)
  {
    /* Use Read EventID from EventSummary to check availability of data in SRAM.
     * This enables the console to continue reading historic data after buffer is restarted
     * and no data has been deleted */
    for (int EvtID = 0; EvtID < MAX_MINI_EVENTS; EvtID++)
    {
      EventSummary.EventID = EvtID;
      RecordReadIndex = 0;
      EventStoreReadSummary(EVENT_STORE_MINI, &EventSummary);
      /* Note: The EventSummary.EventID is not zero-indexed in SRAM*/
      if (EvtID == (EventSummary.EventID - 1))
      {
        ConsolePrintf("Mini-event ID %d summary:\n\r  time = %d,\n\r  latitude = %d,\n\r  longitude = %d,\n\r  records = %d\n\r",
            EventSummary.EventID,
            EventSummary.TimeStamp,
            EventSummary.Latitude,
            EventSummary.Longitude,
            EventStoreGetRecordCount(EVENT_STORE_MINI));

        /* Display the Mini-event data */
        ConsolePrintf("\nMini-event data:\n\r AX     AY     AZ     GX     GY     GZ\n\r");

        /* Update EventID for EventStoreReadRecords read due to non-zero index */
        EventSummary.EventID -= 1;

        for (int i = 0; i < 1000; i++)
        {
          EventStoreReadRecords(EVENT_STORE_MINI, &ImuData, 1, &EventSummary);
          ConsolePrintf("%5d  %5d  %5d  %5d  %5d  %5d\n\r", ImuData.accel.X, ImuData.accel.Y, ImuData.accel.Z,
              ImuData.gyro.X, ImuData.gyro.Y, ImuData.gyro.Z);
          sleep(5);
        }
      }
    }
  }
  else
  {
    ConsolePrintf("\nNo Mini-event Data\n\r");
  }

  /* indicate success */
  return (CONSOLE_ERROR_OK);
}

