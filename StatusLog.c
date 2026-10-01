/**
 ******************************************************************************
 * @file   statuslog.c
 ******************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  Simple status log
 *
 ******************************************************************************
 */
#include "global.h"

/*
 **************************************************************************************************
 *                                         PRIVATE DEFINES
 **************************************************************************************************
 */

/* Maximum number of events in the log at any time */
#define MAX_LOG_EVENTS                   100
#define MAX_RECORD_EVENTS                10
#define INSTANT_RECORD_SENDTIME          300   /* 5 minutes */

typedef struct
{
  uint32_t  EntryTimeStamp;   /* time of last entry */
  uint8_t   eventID;
  uint16_t  eventCount;
} EventRecord_t;


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
/* storage for status events.  Note, the log is sent to the cloud periodically, so it does not have to be very large */
StatusLog_t StatusLog[MAX_LOG_EVENTS];

EventRecord_t InstantEventRecord[MAX_RECORD_EVENTS];

/* Number of events currently stored */
static int  LogEventCount = 0;

/* Semaphore used to block calling tasks to make Status Log atomic */
static SemaphoreHandle_t  statusLogInProcessMutex;



/*
 **************************************************************************************************
 *                                   PRIVATE FUNCTION PROTOTYPES
 **************************************************************************************************
 */

static  void        StatusLogDataReset            (void);
static  bool        WriteEventLogToSpiRAM         (void);
static  bool        ReadEventLogFromSpiRAM        (void);
static  void        IncrInstantEvtCntByStatusLog  (uint8_t logIndex);
static  void        IncrStatusLogPayloadCounter   (uint8_t logIndex);
static  void        incrementStatusLogCount       (StatusLog_t *log);
static  void        StatusLogInstantEventRecordDataReset (void);

/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */

/**
 * @brief   Initialize the status log module to a known state
 * @param   none
 * @return  none
 */
void StatusLogInitialize (void)
{
  /* Set up mutex to ensure sequential access to statusLog */
  statusLogInProcessMutex = xSemaphoreCreateBinary();
  xSemaphoreGive(statusLogInProcessMutex);

  if (IsLowPowerSleepFlagUp())
  {
    ReadEventLogFromSpiRAM();    /* restore the previous values, Instead of reset  */
  }
  else
  {
    /* clear event counter */
    StatusLogDataReset ();
  }
}


/**
 * @brief   Write a status event to the log (timestamp added here)
 * @param   event      : A status event to add to the log
 * @param   data       : 4 byte or uint32_t data for the event
 * @return  bool       : true on success, false on failure
 */
bool StatusLogWriteEventCode (uint8_t eventCode, uint32_t data)
{
  uint32_t TimeStamp = RtcGetTimeStamp();

  if (pdTRUE == xSemaphoreTake(statusLogInProcessMutex, portMAX_DELAY))
  {

    /* find the first free entry in the log */
    for (int LogIndex = 0; LogIndex < MAX_LOG_EVENTS; LogIndex++)
    {
      if (0 == StatusLog[LogIndex].TimeStamp)
      {
        StatusLog[LogIndex].TimeStamp  = TimeStamp;
        StatusLog[LogIndex].EventCode  = eventCode & (~STATUS_INSTANT_REPORT_MASK);
        StatusLog[LogIndex].EventData0 = data & 0xff;
        StatusLog[LogIndex].EventData1 = (data >>8 ) & 0xff;
        StatusLog[LogIndex].EventData2 = (data >> 16) & 0xff;
        StatusLog[LogIndex].EventData3 = (data >> 24) & 0xff;
        LogEventCount++;

        /* Special Handling of Instant Event and Counting Payload */
        if (eventCode & STATUS_INSTANT_REPORT_MASK)
        {
          /* Instant Event, check if the data is for payload or counting */
          if ((data >> 16) != INSTANT_DATA_PAYLOAD)
          {
            /* Update the Instant event Count with the log event of LogIndex */
            IncrInstantEvtCntByStatusLog(LogIndex);
          }
        }

        if (data == STATUS_PAYLOAD_COUNT_ONLY)
        {
          /* Counting Event only, update the payload */
          IncrStatusLogPayloadCounter(LogIndex);
        }
        xSemaphoreGive(statusLogInProcessMutex);
        return true;
      }
    }
    xSemaphoreGive(statusLogInProcessMutex);
  }
  return false;
}


/**
 * @brief   Read a status event from the store.  Optionally, delete the event after reading
 * @param   pEvent : pointer to buffer to put the data in
 * @param   delete : if true, the event will be deleted after reading
 * @return  bool   : true on success, false on failure
 */
bool StatusLogReadEvent (StatusLog_t * pEvent, bool delete)
{
  if (pdTRUE == xSemaphoreTake(statusLogInProcessMutex, portMAX_DELAY))
  {
    /* find the first used entry in the log */
    for (int LogIndex = 0; LogIndex < MAX_LOG_EVENTS; LogIndex++)
    {
      if (0 != StatusLog[LogIndex].TimeStamp)
      {
        memcpy(pEvent, &StatusLog[LogIndex], sizeof(StatusLog_t));
        if (delete)
        {
          memset(&StatusLog[LogIndex], 0, sizeof(StatusLog_t));
          if (LogEventCount)
          {
            LogEventCount--;
          }
        }
        xSemaphoreGive(statusLogInProcessMutex);
        return true;
      }
    }
    xSemaphoreGive(statusLogInProcessMutex);
  }
  return false;
}


/**
 * @brief   Get the number of status events in the log
 * @param   none
 * @return  uint8_t  : The number of events currently in the log
 */
uint8_t  StatusLogGetEventCount (void)
{
  return LogEventCount;
}


/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       List status events to the console
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int StatusLogConsoleList (int ArgCount, char **pArguments)
{
  int EventCount = StatusLogGetEventCount();

  ConsolePrintf("\nEvent Count: %d\n\r", EventCount);
  ConsolePrintf("Status Log:\n\r Timestamp              EventCode\tbyte0\tbyte1\tbyte2\tbyte3\n\r");

  /* scan through the log printing any used entries */
  for (int LogIndex = 0; LogIndex < MAX_LOG_EVENTS; LogIndex++)
  {
    if (0 != StatusLog[LogIndex].TimeStamp)
    {
      ConsolePrintf(" ");
      RtcPrintTimestamp(StatusLog[LogIndex].TimeStamp);
      ConsolePrintf(" %5d", StatusLog[LogIndex].EventCode);
      ConsolePrintf(" \t\t %d\t %d\t %d\t %d\n\r", StatusLog[LogIndex].EventData0,
          StatusLog[LogIndex].EventData1 ,StatusLog[LogIndex].EventData2, StatusLog[LogIndex].EventData3);
      sleep(5);
    }
  }

  for (int recIndex = 0; recIndex < MAX_RECORD_EVENTS; recIndex++)
  {
    if (0 != InstantEventRecord[recIndex].EntryTimeStamp)
    {
      ConsolePrintf(" ");
      RtcPrintTimestamp(InstantEventRecord[recIndex].EntryTimeStamp);
      ConsolePrintf(" %5d (I)", InstantEventRecord[recIndex].eventID);
      ConsolePrintf(" \t %d\t\n\r", InstantEventRecord[recIndex].eventCount);
      sleep(2);
    }
  }
  /* indicate success */
  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       Clear status events
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int StatusLogConsoleClear (int ArgCount, char **pArguments)
{
  StatusLogDataReset();

  /* indicate success */
  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       Add a number of dummy events to the log (for testing)
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int StatusLogConsoleAdd (int ArgCount, char **pArguments)
{
  int NoOfEvents = 0;

  if (1 == ArgCount)
  {
    /* find out how many events to add */
    sscanf(pArguments[0], "%d", &NoOfEvents);

    /* add the events */
    for (int i = 0; i < NoOfEvents; i++)
    {
      StatusLogWriteEventCode(STATUS_DUMMY_EVENT, STATUS_DATA_LOAD_NONE);
    }

    /* indicate success */
    return (CONSOLE_ERROR_OK);
  }
  return (CONSOLE_ARG_NO);
}


/**
 * @brief       Remove a number of events from the log (for testing)
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int StatusLogConsoleRemove (int ArgCount, char **pArguments)
{
  int NoOfEvents = 0;
  StatusLog_t StatusEvent;

  if (1 == ArgCount)
  {
    /* find out how many events to remove */
    sscanf(pArguments[0], "%d", &NoOfEvents);

    /* remove the events */
    for (int i = 0; i < NoOfEvents; i++)
    {
      StatusLogReadEvent(&StatusEvent, true);
    }

    /* indicate success */
    return (CONSOLE_ERROR_OK);
  }
  return (CONSOLE_ARG_NO);
}


/**
 * @brief       update current data set to PRAM
 * @param       none
 * @return      true on success
 */
bool StatusLogWritetoPSRAM (void)
{
  return (WriteEventLogToSpiRAM ());
}


/**
 * @brief       Check if any Instant Event is matured for sending
 * @param       none
 * @return      True if any instant event is mature to be send
 *              otherwise return false
 */
bool StatusLogInstantEventMatureCheck (void)
{
  int deltaTS = 0;
  bool haveEventMatured = false;

  if (pdTRUE == xSemaphoreTake(statusLogInProcessMutex, portMAX_DELAY))
  {
    /* check the record need to send */
    for (int recIndex = 0; recIndex < MAX_RECORD_EVENTS; recIndex++)
    {
        /* Only handle event that ever happened */
      if (InstantEventRecord[recIndex].eventCount)
      {
        /* Any expired event will trigger the message send */
        deltaTS = RtcGetTimeStamp() - InstantEventRecord[recIndex].EntryTimeStamp;

        if (deltaTS > INSTANT_RECORD_SENDTIME)
        {
          haveEventMatured = true;
          break;
        }
      }
    }
    xSemaphoreGive(statusLogInProcessMutex);
  }
  if (haveEventMatured)
  {
    /* have matured reocrd, pack them */
    StatusLogAppendInstantEventRecords();
  }
  return haveEventMatured;
}


/*
 **************************************************************************************************
 *                                     PRIVATE FUNCTIONS
 **************************************************************************************************
 */
/**
 * @brief     reset EventLog data
 * @param     none
 * @return    none
 */
static void  StatusLogDataReset (void)
{
  LogEventCount = 0;

  if (pdTRUE == xSemaphoreTake(statusLogInProcessMutex, portMAX_DELAY))
  {
    /* invalidate all log entries */
    for (int LogIndex = 0; LogIndex < MAX_LOG_EVENTS; LogIndex++)
    {
      StatusLog[LogIndex].TimeStamp = 0;
      StatusLog[LogIndex].EventCode = 0;
      StatusLog[LogIndex].EventData0 = 0;
      StatusLog[LogIndex].EventData1 = 0;
      StatusLog[LogIndex].EventData2 = 0;
      StatusLog[LogIndex].EventData3 = 0;
    }
    xSemaphoreGive(statusLogInProcessMutex);
    /*  record reset */
    StatusLogInstantEventRecordDataReset();
  }
}


/**
 * @brief     Reset the Instant Eventlog record data
 * @param     none
 * @return    none
 */
static void  StatusLogInstantEventRecordDataReset (void)
{
  if (pdTRUE == xSemaphoreTake(statusLogInProcessMutex, portMAX_DELAY))
  {
    for (int recIndex = 0; recIndex < MAX_RECORD_EVENTS; recIndex++)
    {
      InstantEventRecord[recIndex].EntryTimeStamp = 0;
      InstantEventRecord[recIndex].eventID = 0;
      InstantEventRecord[recIndex].eventCount = 0;
    }
    xSemaphoreGive(statusLogInProcessMutex);
  }
}


/**
 * @brief     Write a event log data to SPI RAM
 * @param     none
 * @return    none
 */
static bool WriteEventLogToSpiRAM (void)
{
  bool success = false;
  if (pdTRUE == xSemaphoreTake(statusLogInProcessMutex, portMAX_DELAY))
  {
    uint32_t startAddress = SERIAL_RAM_EVEVT_LOG_AREA_START;

    /* Write the Status Log to Serial Ram */
    int offset = sizeof (StatusLog);
    if (SerialRamWrite(startAddress, (uint8_t*)StatusLog, offset))
    {
      /* Write the instant event record to Serial Ram */
      success = SerialRamWrite(startAddress + offset, (uint8_t*)&InstantEventRecord, sizeof(InstantEventRecord));
      if (success)
      {
        /* Write Log Event Count to Serial Ram */
        offset = offset + sizeof(InstantEventRecord);
        success = SerialRamWrite(startAddress + offset, (uint8_t*)&LogEventCount, sizeof(int));
      }
      #ifdef DEBUG
        ConsolePrintf("-->Write eventLog Count=%d RamSize[%d]\r\n", LogEventCount, offset+4);
      #endif
    }
    else
    {
      ConsolePrintf("Failed backup EventLog data to SPI RAM\r\n");
    }
    xSemaphoreGive(statusLogInProcessMutex);
  }
  return (success);
}


/**
 * @brief     Read event log data to SPI RAM
 * @param     none
 * @return    none
 */
static bool ReadEventLogFromSpiRAM (void)
{
  bool success = false;
  if (pdTRUE == xSemaphoreTake(statusLogInProcessMutex, portMAX_DELAY))
  {
     uint32_t startAddress = SERIAL_RAM_EVEVT_LOG_AREA_START;

    int nl = sizeof (StatusLog);
    if (SerialRamRead(startAddress, (uint8_t*)StatusLog, nl))
    {
      success = SerialRamRead(startAddress + nl, (uint8_t*)&InstantEventRecord, sizeof(InstantEventRecord));
      if (success)
      {
        nl = nl + sizeof(InstantEventRecord);
        success = SerialRamRead(startAddress + nl, (uint8_t*)&LogEventCount, sizeof(int));
      }
    }
    else
    {
      ConsolePrintf("Failed read back EventLog data from SPI RAM\r\n");
    }
    xSemaphoreGive(statusLogInProcessMutex);
  }
  return (success);
}


/**
 * @brief     Increment the Instant Event Count by the the Status Log Index
 *            The repeated Status Log will be removed, as it is recorded
 *            by the status event
 *            * statusLogInProcessMutex assumed accquried before this function call
 * @param     logIndex: status log index to check
 * @return    none
 */
static void  IncrInstantEvtCntByStatusLog (uint8_t logIndex)
{
  bool newRecord = true;
  /* search current records */
  for (int recIndex = 0; recIndex < MAX_RECORD_EVENTS; recIndex++)
  {
    if (InstantEventRecord[recIndex].eventID == StatusLog[logIndex].EventCode)
    {
      InstantEventRecord[recIndex].EntryTimeStamp = StatusLog[logIndex].TimeStamp;
      InstantEventRecord[recIndex].eventCount++;
      newRecord = false;
      /* clear this repeated event from status event entry, the first entry of this event will kept and send out */
      StatusLog[logIndex].TimeStamp = 0;
      StatusLog[logIndex].EventCode = 0;
      if (LogEventCount)
      {
        LogEventCount--;
      }
      break;
    }
  }

  if (newRecord)  /* add to the empty record */
  {
    for (int recIndex = 0; recIndex < MAX_RECORD_EVENTS; recIndex++)
    {
      if (InstantEventRecord[recIndex].EntryTimeStamp == 0)
      {
        InstantEventRecord[recIndex].EntryTimeStamp = StatusLog[logIndex].TimeStamp;
        InstantEventRecord[recIndex].eventID = StatusLog[logIndex].EventCode;
        InstantEventRecord[recIndex].eventCount = 1;
        break;
      }
    }
  }
}

/**
 * @brief     Function to increment the counter in the StatusLog_t struct
 * @param     log : StatusLog pointer to be handled
 * @return    none
 */
static void incrementStatusLogCount (StatusLog_t *log)
{
  if (log)
  {
    if (log->EventData0 == UINT8_MAX) 
    {
        /* EventData0 is at maximum value, time to carry */
        if (log->EventData1 == UINT8_MAX) 
        {
            /* Both counters are at maximum, time to wrap around */
            log->EventData0 = 0;
            log->EventData1 = 0;
        } else {
            /* time to carry */
            log->EventData1++;
            log->EventData0 = 0;
        }
    } 
    else 
    {
        /* Increment EventData0 with one */
        log->EventData0++;
    }
  }
}


/**
 * @brief     Increment the Status Log and use the payload as counter
 *            The repeated Status Log will be removed 
 *            * statusLogInProcessMutex assumed accquried before this function call
 * @param     logIndex:  currnet log Index that has to be handled
 * @return    none
 */
static void IncrStatusLogPayloadCounter (uint8_t logIndex)
{
  for (int i = 0; i < MAX_LOG_EVENTS; i++)
  {
    if (StatusLog[logIndex].EventCode == StatusLog[i].EventCode)
    {
      incrementStatusLogCount(&StatusLog[i]);
      if (!(StatusLog[i].EventData0 == 1 && StatusLog[i].EventData1 == 0))
      {
        /* Counter is not one, so it is repeat event */
        /* Reset the repeated event from status event entry, the first entry of this event will kept with counting */
        StatusLog[logIndex].TimeStamp = 0;
        StatusLog[logIndex].EventCode = 0;
        if (LogEventCount)
        {
          LogEventCount--;
        }
      }
      break;
    }
  }
}  


/**
 * @brief     Append the instant event record to the Status Log if there is count
 *            After append, the instant event record will be reset
 * @param     none
 * @return    none
 */
void StatusLogAppendInstantEventRecords (void)
{
  /* then fire with the time expired one */
  for (int i=0; i < MAX_RECORD_EVENTS; i ++)
  {
    if (InstantEventRecord[i].eventCount)
    {
      StatusLogWriteEventCode((STATUS_INSTANT_REPORT_MASK | InstantEventRecord[i].eventID), (uint32_t)(InstantEventRecord[i].eventCount | INSTANT_DATA_PAYLOAD << 16 ));
    }
  }
  StatusLogInstantEventRecordDataReset();
}

