/*
 **************************************************************************************************
 * @file   messages.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  This module encodes and decodes messages used for cloud communications
 *
 **************************************************************************************************
 */

#include "global.h"

/*
 **************************************************************************************************
 *                                         PRIVATE DEFINES
 **************************************************************************************************
 */
/* current protocol version */
#define MESSAGE_PROTOCOL_VERSION     1

/* Size of CRC in bytes */
#define MESSAGE_CRC_SIZE             4

/* Maximum body size */
#define MESSAGE_MAX_BODY_SIZE        (MESSAGE_MAX_MESSAGE_SIZE - sizeof(MessageHeader_t) - MESSAGE_CRC_SIZE)

/* 
 * Max PJM record size (GNSS valid case)
 * GnssData.Status (1), LatitudeDelta(2), LongitudeDelta(2), GnssData.Speed(2), GnssData.Heading(2)
 */ 
#define PJM_RECORDS_MAX_SIZE       9

/* Mini-event Message details */
#define MAX_MINIEVENTS_PER_MESSAGE    5
#define IMU_RECORDS_PER_MINIEVENT     30

/* ASCII character codes */
#define ASCII_ZERO       48

#define FW_SW_MAJOR_MASK    0x0F
#define FW_HW_MAJOR_MASK    0xF0

/* Standard Packet Header.  Starts all out-bound messages */
typedef struct
{
  uint16_t   PacketCount;
  uint8_t    MessageType;
  uint8_t    FwVersionMajor;  /* Upper 4bits is hardware Version, lower 4bits is the Major Version*/
  uint8_t    FwVersionMinor;
  uint8_t    FwVersionPatch;
  uint8_t    Customer;
  uint64_t   IMEI;
  int32_t    Latitude;
  int32_t    Longitude;
  uint32_t   TimeStamp;
  uint16_t   GsmLac;
  uint16_t   GsmCid;
  uint16_t   GsmCountryCode;
  uint16_t   GsmOperatorCode;
  uint8_t    GnssStatus;
  uint16_t   DeviceStatus;
  uint8_t    BatteryPercent;
  uint16_t   GPSTTF;
  int8_t     Temperature;
  uint16_t   Pitch;
  uint16_t   Roll;
  uint16_t   Filtered_X;
  uint16_t   Filtered_Y;
  uint16_t   Filtered_Z;
} MessageHeader_t;

/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
/*
 * Packet Count / ID
 * Used to uniquely identify each packet for the purpose of packet acknowledge and re-sending
 * Reset to 0 on exit from hibernation
 */
static uint16_t PacketCount = 0;

/*
 * Journey Segment
 * Used to uniquely identify each message that makes up a journey.
 * Resets to 0 on Start of Journey.
 */
static int JourneySegment = 0;

/*
 * Impact ID
 * Used to uniquely identify an impact event.
 */
static int ImpactId = 0;

/*
 * Impact Fragment
 * Used to uniquely identify each message that makes up a single impact event.
 * Resets to 0 with each new impact.
 */
static int ImpactFragment = 0;

/*
 * Red Button ID
 * Used to uniquely identify a red button event
 */
static uint8_t RedButtonId = 0;

/*
 * Red Button Fragment
 * Used to uniquely identify each message that makes up a single Red Button event.
 * Resets to 0 with each new Red Button event.
 */
static uint8_t RedButtonFragment = 0;

/*
 * Device Initialisation
 * Used to store and report WUST results for Dim message body
 */
static WUST_ReportInfo_t deviceSnapShot = { 0 };

/* storage for the message being built */
static uint8_t MessageBuffer[MESSAGE_MAX_MESSAGE_SIZE];

/* keep the last packet header time stamp */
static uint32_t  previousTP = 0;

/*
 **************************************************************************************************
 *                                   PRIVATE FUNCTION PROTOTYPES
 **************************************************************************************************
 **************************************************************************************************#
 */
static bool      MessageEnqueueSoj       (void);
static bool      MessageEnqueueEoj       (void);
static bool      MessageEnqueuePjm       (void);

/* message encoding functions */
static int       MessageEncodeHeader     (uint8_t * pBuffer, uint8_t messageType);
static int       MessageEncodeDimBody    (uint8_t * pBuffer);
static int       MessageEncodeDsmBody    (uint8_t * pBuffer);
static int       MessageEncodeRsmBody    (uint8_t * pBuffer);
static int       MessageEncodeSojEojBody (uint8_t * pBuffer, bool isSoj);
static int       MessageEncodePjmBody    (uint8_t * pBuffer);
static int       MessageEncodeIevBody    (uint8_t * pBuffer);
static int       MessageEncodeMemBody    (uint8_t * pBuffer);
static int       MessageEncodeRbmBody    (uint8_t * pBuffer);
static int       MessageEncodeCimBody    (uint8_t * pBuffer);
static int       MessageEncodeCrc        (uint8_t * pBuffer, int dataLength);
static int       MessageAddU8            (uint8_t * pBuffer, uint8_t data);
static int       MessageAddU16           (uint8_t * pBuffer, uint16_t data);
static int       MessageAddU32           (uint8_t * pBuffer, uint32_t data);
static int       MessageAddAsciiDecimal  (uint8_t * pBuffer, uint32_t data, uint8_t digits);

/* helper function for the validation of the message content */
static bool      MessageHeaderValidateXyz (int16_t X, int16_t Y, int16_t Z);

#if MESSAGE_PRINT_MESSAGES
static void      MessagePrintMessage     (int Length);
#endif


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */

/**
 * @brief   Enqueue Journey related Message (SOJ, PJM, EOJ),
 *          if RTC timestamp did not have initial fix, no message will be enqueued
 * @param   messageType journeyType to be enqueued
 * @retval  true on success, false on failure
 */
bool MessageEnqueueJourneyMessage (MessageType messageType)
{
  bool ret = false;

  /* Journey Type message will only valid if the RTC have the initial fixed */
  if (RtcTimeValid())
  {
    switch(messageType)
    {
    case MESSAGE_TYPE_START_OF_JOURNEY:
      ret = MessageEnqueueSoj();
      break;
    case MESSAGE_TYPE_PERIODIC_JOURNEY:
      ret = MessageEnqueuePjm();
      break;
    case MESSAGE_TYPE_END_OF_JOURNEY:
      ret = MessageEnqueueEoj();
      break;
    case MESSAGE_TYPE_STATUS:
      ret = MessageEnqueueDsm();
      break;
    default:
      break;
    }
  }
  else
  {
    ConsolePrintf("No Valid RTC, Message %d not enqueued\n\r", messageType);
  }
  return ret;
}



/**
 * @brief   Encode a Device initialisation Message AND add to the GPRS transmission queue
 * @param   None
 * @retval  true on success, false on failure
 */
bool MessageEnqueueDim (void)
{
  /* Build in the standard message header and copy into the message buffer */
  int Length = MessageEncodeHeader(MessageBuffer, MESSAGE_TYPE_INITIAISATION);

  /* Add the message body */
  Length += MessageEncodeDimBody(MessageBuffer + Length);

  /* Calculate CRC and add to the message */
  Length += MessageEncodeCrc(MessageBuffer, Length);

  /* Add the message to the GPRS transmission queue */
  MessageQueueWrite(MESSAGE_PRIORITY_LOW, MessageBuffer, Length);

  return true;
}


/**
 * @brief   Encode a Device Heartbeat Message AND add to the GPRS transmission queue
 * @param   None
 * @retval  true on success, false on failure
 */
bool MessageEnqueueDhm (void)
{
  /* Build in the standard message header and copy into the message buffer */
  int Length = MessageEncodeHeader(MessageBuffer, MESSAGE_TYPE_HEARTBEAT);

  /* Add the message body (NOTE: Heartbeat uses the same message body format as a status message) */
  Length += MessageEncodeDsmBody(MessageBuffer + Length);

  /* Calculate CRC and add to the message */
  Length += MessageEncodeCrc(MessageBuffer, Length);

  /* Add the message to the GPRS transmission queue */
  MessageQueueWrite(MESSAGE_PRIORITY_LOW, MessageBuffer, Length);

  return true;
}


/**
 * @brief   Encode a Device Status Message AND add to the GPRS transmission queue
 * @param   None
 * @retval  true on success, false on failure
 */
bool MessageEnqueueDsm (void)
{
  /* We only send status messages if there are status events waiting */
  if (StatusLogGetEventCount())
  {
    /* Build in the standard message header and copy into the message buffer */
    int Length = MessageEncodeHeader(MessageBuffer, MESSAGE_TYPE_STATUS);

    /* Add the message body */
    Length += MessageEncodeDsmBody(MessageBuffer + Length);

    /* Calculate CRC and add to the message */
    Length += MessageEncodeCrc(MessageBuffer, Length);

    /* Add the message to the GPRS transmission queue */
    if (SequencerInJourney())
    {
      MessageQueueWrite(MESSAGE_PRIORITY_HIGH, MessageBuffer, Length);
    }
    else
    {
       MessageQueueWrite(MESSAGE_PRIORITY_LOW, MessageBuffer, Length);
    }
  }

  return true;
}


/**
 * @brief   Encode a Start of Journey Message AND add to the GPRS transmission queue
 * @param   None
 * @retval  true on success, false on failure
 */
static bool MessageEnqueueSoj (void)
{
  /*
   * Enqueue the start of a new journey so we update our counters
   * JourneyFragment is reset to indicate the first packet in this journey
   */
  JourneySegment = 0;

  /* Build in the standard message header and copy into the message buffer */
  int Length = MessageEncodeHeader(MessageBuffer, MESSAGE_TYPE_START_OF_JOURNEY);

  /* Add the message body */
  Length += MessageEncodeSojEojBody(MessageBuffer + Length, true);

  /* Calculate CRC and add to the message */
  Length += MessageEncodeCrc(MessageBuffer, Length);

  /* Add the message to the GPRS transmission queue */
  MessageQueueWrite(MESSAGE_PRIORITY_LOW, MessageBuffer, Length);

  return true;
}


/**
 * @brief   Encode an End of Journey Message AND add to the GPRS transmission queue
 * @param   None
 * @retval  true on success, false on failure
 */
static bool MessageEnqueueEoj (void)
{
  JourneySegment++;

  /* Build in the standard message header and copy into the message buffer */
  int Length = MessageEncodeHeader(MessageBuffer, MESSAGE_TYPE_END_OF_JOURNEY);

  /* Add the message body */
  Length += MessageEncodeSojEojBody(MessageBuffer + Length, false);

  /* Calculate CRC and add to the message */
  Length += MessageEncodeCrc(MessageBuffer, Length);

  /* Add the message to the GPRS transmission queue */
  MessageQueueWrite(MESSAGE_PRIORITY_LOW, MessageBuffer, Length);

  return true;
}


/**
 * @brief   Encode Periodic Journey Messages AND add to the GPRS transmission queue
 * @param   None
 * @retval  true on success, false on failure
 */
static bool MessageEnqueuePjm (void)
{
  int Length = 0;

  /* encode the available journey data into one or many messages */
  while (GnssEventGetRecordCount())
  {
    /* increment JourneyFragment to indicate the next packet in this journey */
    JourneySegment++;

    /* Build in the standard message header and copy into the message buffer */
    Length = MessageEncodeHeader(MessageBuffer, MESSAGE_TYPE_PERIODIC_JOURNEY);

    /* Add the message body */
    Length += MessageEncodePjmBody(MessageBuffer + Length);

    /* Calculate CRC and add to the message */
    Length += MessageEncodeCrc(MessageBuffer, Length);
    /* Add the message to the GPRS transmission queue */
    MessageQueueWrite(MESSAGE_PRIORITY_LOW, MessageBuffer, Length);
  }

#if MESSAGE_PRINT_MESSAGES
  MessagePrintMessage(Length);
#endif

  return true;
}


/**
 * @brief   Encode Impact Event messages AND add to the GPRS transmission queue
 * @param   None
 * @retval  true on success, false on failure
 */
bool MessageEnqueueIem (void)
{
  /* update the unique impact identifier */
  ImpactId++;
  ImpactFragment = 0;

  /* encode the impact data into multiple messages */
  while (EventStoreGetRecordCount(EVENT_STORE_IMPACT))
  {
    /* Build in the standard message header and copy into the message buffer */
    int Length = MessageEncodeHeader(MessageBuffer, MESSAGE_TYPE_IMPACT);

    /* Add the message body */
    Length += MessageEncodeIevBody(MessageBuffer + Length);

    /* Calculate CRC and add to the message */
    Length += MessageEncodeCrc(MessageBuffer, Length);

    /* Add the message to the GPRS transmission queue */
    MessageQueueWrite(MESSAGE_PRIORITY_HIGH, MessageBuffer, Length);
  }

  /* We have finished with this event, so delete it from the store */
  EventStoreDeleteEvent(EVENT_STORE_IMPACT, NULL);

  return true;
}


/**
 * @brief   Encode Mini Event messages AND add to the GPRS transmission queue
 * @param   None
 * @retval  true on success, false on failure
 */
bool MessageEnqueueMem (void)
{
  /* update the unique red-button identifier */
  RedButtonId++;
  RedButtonFragment = 0;

  /* Encode the mini-event data into multiple messages */
  while (EventStoreGetEventCount(EVENT_STORE_MINI))
  {
    /* Build in the standard message header and copy into the message buffer */
    int Length = MessageEncodeHeader(MessageBuffer, MESSAGE_TYPE_MINI_EVENT);

    /* Add the message body */
    Length += MessageEncodeMemBody(MessageBuffer + Length);

    /* Calculate CRC and add to the message */
    Length += MessageEncodeCrc(MessageBuffer, Length);

    /* Add the message to the GPRS transmission queue */
    MessageQueueWrite(MESSAGE_PRIORITY_HIGH, MessageBuffer, Length);
  }
  return true;
}


/**
 * @brief   Encode a Red Button message AND add to the GPRS transmission queue
 * @param   None
 * @retval  true on success, false on failure
 */
bool MessageEnqueueRbm (void)
{
  /* Build in the standard message header and copy into the message buffer */
  int Length = MessageEncodeHeader(MessageBuffer, MESSAGE_TYPE_REDBUTTON);

  /* Add the message body */
  Length += MessageEncodeRbmBody(MessageBuffer + Length);

  /* Calculate CRC and add to the message */
  Length += MessageEncodeCrc(MessageBuffer, Length);

  /* Add the message to the GPRS transmission queue */
  MessageQueueWrite(MESSAGE_PRIORITY_HIGH, MessageBuffer, Length);

  return true;
}


/**
 * @brief   Encode a Radio Scan message AND add to the GPRS transmission queue
 * @param   None
 * @retval  true on success, false on failure
 */
bool MessageEnqueueRsm (void)
{
  /* Build in the standard message header and copy into the message buffer */
  int Length = MessageEncodeHeader(MessageBuffer, MESSAGE_TYPE_RADIOSCAN);

  /* Add the message body */
  Length += MessageEncodeRsmBody(MessageBuffer + Length);

  /* Calculate CRC and add to the message */
  Length += MessageEncodeCrc(MessageBuffer, Length);

  /* Add the message to the GPRS transmission queue */
  MessageQueueWrite(MESSAGE_PRIORITY_LOW, MessageBuffer, Length);

  return true;
}


/**
 * @brief   Encode a Config Value Information message AND add to the GPRS transmission queue
 * @param   None
 * @retval  true on success, false on failure
 */
bool MessageEnqueueCim (void)
{
  /* Build in the standard message header and copy into the message buffer */
  int Length = MessageEncodeHeader(MessageBuffer, MESSAGE_TYPE_CONFIG_VALUE);

  /* Add the message body */
  Length += MessageEncodeCimBody(MessageBuffer + Length);

  /* Calculate CRC and add to the message */
  Length += MessageEncodeCrc(MessageBuffer, Length);

  /* Add the message to the GPRS transmission queue */
  MessageQueueWrite(MESSAGE_PRIORITY_LOW, MessageBuffer, Length);

  return true;
}


/**
 * @brief   Set static values after LP sleep wake up
 * @param   pCount: value to set
 * @retval  none
 */
void MessageLPSSetPacketCount (uint32_t pCount)
{
  PacketCount = pCount;
}


/**
 * @brief   return the current packet count
 * @param   none
 * @retval  current packet count value
 */
uint32_t MessageLPSGetPacketCount (void)
{
  return PacketCount;
}


/**
 * @brief   return the current impact or red button ID
 * @param   idType: ET_IMPACT or ET_BUTTON
 * @retval  requested current id value
 */
uint8_t MessageGetIDs (uint8_t idType)
{
  return ((idType == ET_IMPACT)? (uint8_t) ImpactId: RedButtonId);
}


/**
 * @brief   restore impact or red button ID from PSRAM
 * @param   idType: ET_IMPACT or ET_BUTTON
 * @param   idValue: state value
 * @retval  none
 */
void MessageRestoreIDs (uint8_t idType, uint8_t idValue)
{
  (idType == ET_IMPACT)? (ImpactId = idValue) : (RedButtonId = idValue);
}

/**
 * @brief   Retrieve the message type from a message buffer.
 * @param   pMessage: Pointer to the message buffer.
 * @retval  Message type (uint8_t) extracted from the message.
 */
MessageType MessageGetMessageType (uint8_t * pMessage)
{
  if (pMessage == NULL)
  {
    return MESSAGE_TYPE_INVALID;
  }
  MessageHeader_t *pHeader = (MessageHeader_t *)pMessage;
  return pHeader->MessageType;
}


/**
 * @brief   Retrieve the timestamp from a message buffer.
 * @param   pMessage: Pointer to the message buffer.
 * @retval  Timestamp (uint32_t) extracted from the message.
 */
uint32_t MessageGetMessageTimeStamp (uint8_t * pMessage)
{
  if (pMessage == NULL)
  {
    return 0;
  }

  MessageHeader_t *pHeader = (MessageHeader_t *)pMessage;
  return pHeader->TimeStamp;  
}


/**
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */

/**
 * @brief   Checks if the given message type requires GNSS event data from PSRAM
 * @param   messageType: The type of message
 * @return  true if the message needs PSRAM data, false otherwise
 */
static bool MessageTypeUsesGNSSEventInSpiRAM(uint8_t messageType)
{
  switch (messageType)
  {
    /* message types that need PSRAM */
    case MESSAGE_TYPE_PERIODIC_JOURNEY:
      return true;
    default:
      return false;
  }
}


/**
 * @brief  Fill the standard message header with current data
 * @param  pBuffer     : pointer to a buffer to copy the header into
 * @param  messageType : the message type for this message
 * @retval Length      : Number of bytes added to the buffer
 */
static int MessageEncodeHeader (uint8_t * pBuffer, uint8_t messageType)
{
  MessageHeader_t * pMessageHeader = (MessageHeader_t *) pBuffer;
  GnssEventMotionData_t GnssMotionData;
  int pitch, roll;
  AccelerationData_t accelData;

  /* store then increment the unique packet ID */
  pMessageHeader->PacketCount     = PacketCount++;
  pMessageHeader->MessageType     = messageType;

  /* Firmware version information */
  deviceSnapShot = getWUSTReportInfoSnapShot();
  /* Clear FwVersionMajor */
  pMessageHeader->FwVersionMajor = 0;
  /* Set lower 4 bits for Major version */
  pMessageHeader->FwVersionMajor = (deviceSnapShot.AppFirmwareVersion.MajorVersion & FW_SW_MAJOR_MASK);
  /* Set upper 4 bits for hardware version */
  pMessageHeader->FwVersionMajor |= ((GetHardwareVersion()<<4) & FW_HW_MAJOR_MASK);

  pMessageHeader->FwVersionMinor = deviceSnapShot.AppFirmwareVersion.MinorVersion;
  pMessageHeader->FwVersionPatch = deviceSnapShot.AppFirmwareVersion.PatchVersion;

  /* Add the Customer field */
  pMessageHeader->Customer        = ConfigGetCustomer();

  /* Add the current timestamp */
  pMessageHeader->TimeStamp       = RtcGetTimeStamp();
  if (abs(pMessageHeader->TimeStamp - previousTP) > (3600*24))  /* more the a day, date change */
  {
    if (previousTP != 0 )   /* log the event */
    {
      StatusLogWriteEventCode(STATUS_HEAD_DATE_DRIFT, (uint32_t) abs(pMessageHeader->TimeStamp - previousTP) );
    }
    pMessageHeader->TimeStamp = RtcGetTimeStamp();  /* read once more */
  }
  previousTP = pMessageHeader->TimeStamp;
  /* Add the modem status */
  pMessageHeader->IMEI            = ModemStatus.IMEI;
  pMessageHeader->Latitude        = ModemStatus.GnssData.Latitude;
  pMessageHeader->Longitude       = ModemStatus.GnssData.Longitude;
  pMessageHeader->GnssStatus      = ModemStatus.GnssData.Status;
  pMessageHeader->GsmLac          = ModemStatus.GsmLac;
  pMessageHeader->GsmCid          = ModemStatus.GsmCid;
  pMessageHeader->GsmCountryCode          = ModemStatus.countryCode;
  pMessageHeader->GsmOperatorCode         = ModemStatus.operatorCode;

  /* Add general device status */
  pMessageHeader->DeviceStatus    = BatteryMonitorGetUsbState();
  pMessageHeader->DeviceStatus    |= ((BatteryMonitorGetVoltage() & 0x1FFF) << 1);

  pMessageHeader->BatteryPercent  = BatteryMonitorGetLevel() << 1;
  if (BatteryMonitorGetChargingState())
  {
    pMessageHeader->BatteryPercent |= 0x01;
  }
  pMessageHeader->GPSTTF          = ModemStatus.GPSTimeToFix;
  pMessageHeader->Temperature     = (int8_t)(BatteryMonitorGetTemperature() / 10);

  /* Peek PSRAM for GNSS motion data if this type of Message Body needs GNSS Event in the PSRAM  */
  /* If retrieval from PSRAM fails (e.g., PSRAM error or data out of range), fallback to use the data at the time of packet creation.*/
  if (MessageTypeUsesGNSSEventInSpiRAM(messageType) 
        && GnssEventPeek(&GnssMotionData)
        && MessageHeaderValidateXyz(GnssMotionData.accelData.X, GnssMotionData.accelData.Y, GnssMotionData.accelData.Z))
  {
    accelData = GnssMotionData.accelData;
  }
  else
  {
    XyzGetFilteredData(&accelData);
  }
  /* Device Orientation got (either from PSRAM or real-timx XYZ data), pack it into the packet header */
  CalculateOrientation((float)accelData.X, (float)accelData.Y, (float)accelData.Z, &pitch, &roll);
  pMessageHeader->Pitch      = (int16_t)pitch;
  pMessageHeader->Roll       = (int16_t)roll;
  pMessageHeader->Filtered_X = accelData.X;
  pMessageHeader->Filtered_Y = accelData.Y;
  pMessageHeader->Filtered_Z = accelData.Z;

  return (sizeof(MessageHeader_t));
}


/**
 * @brief  Patch In invalid record if there is a gap in lastPjmTS and gnssTimeStamp, 
 *          max patch PJM_RECORDS_MAX_SIZE reocrd only at a time to avoid buffer overflow
 * @param  pBuffer          : pointer to a buffer to PJM body
 * @param  lastPjmTS        : latest timestamp of the current the PJM body
 * @param  gnssTimeStamp    : timestamp from the GNSS (absolute true timestamp to be catched up with)
 * @retval bytesAdded       : number of bytes added to the buffer
 */
static uint8_t PatchPJMWithInvalidData(uint8_t* pBuf, uint32_t lastPjmTS, uint32_t gnssTimeStamp)
{
  uint32_t timeStampDiff = gnssTimeStamp - lastPjmTS;
  uint8_t recordPatched = 0;

  /* Patch max PJM_RECORDS_MAX_SIZE of record at a time to avoid overflow of the message buffer */
  uint8_t p = (timeStampDiff > PJM_RECORDS_MAX_SIZE) ? PJM_RECORDS_MAX_SIZE:timeStampDiff;
  for (uint8_t i = 0; i < p; i++)
  {
    /* fill up with an invalid data valid flag (0 = invalid) + No Jamming detect + number of satellites = 0  */
    *pBuf++ = 0;
    recordPatched++;
  }
  return recordPatched;
}

/**
 * @brief  Encode the PJM message body and add it to the buffer
 * @param  pBuffer     : pointer to a buffer to copy the PJM body into
 * @retval bytesAdded  : number of bytes added to the buffer
 */
static int MessageEncodePjmBody (uint8_t * pBuffer)
{
  static GnssEventMotionData_t GnssMontionData;

  /*
   * Reference Latitude and Longitude  This is the location of the first VALID fix in the message
   */
  int32_t   LastLatitude = 0;
  int32_t   LastLongitude = 0;
  bool      FirstValidFound = false;
  uint16_t  NoOfRecordsAdded = 0;
  int16_t   LatitudeDelta;
  int16_t   LongitudeDelta;
  uint8_t * pSecondsRecord;
  uint8_t * pTimeStampRecord;
  uint8_t * pRefLatRecord;
  uint8_t * pRefLongRecord;
  uint8_t * pBuf = pBuffer;

  /* reference point to tell the last avaliable memory slot for a pjm record to avoid overflow */
  const uint8_t * pLastPJMRecordPosition = (pBuffer + MESSAGE_MAX_BODY_SIZE - PJM_RECORDS_MAX_SIZE); 
  bool pBufFull = false;

  /* variable across segments to handle when record in PSRAM is missed */
  static uint32_t FirstPjmStarttime = 0;
  static uint32_t TotalPjmRecordAcrossSegments = 0;

  /* Add the Journey ID number */
  pBuf += MessageAddU32(pBuf, SeqJourneyGetJourneyId());

  /* Add the Journey Segment number */
  pBuf += MessageAddU16(pBuf, JourneySegment);

  /* Skip over the Number of seconds in the message - we wont know how many until the message is full */
  pSecondsRecord = pBuf;
  pBuf += 2;

  /* Skip over the Timestamp - we will fill this in once we have read the GNSS data */
  pTimeStampRecord = pBuf;
  pBuf += 4;

  /* Skip over the Reference Lat and Long - we will fill these in once we have read the GNSS data */
  pRefLatRecord = pBuf;
  pBuf += 4;
  pRefLongRecord = pBuf;
  pBuf += 4;

  /* update if message buffer full yet */
  pBufFull = (pBuf > pLastPJMRecordPosition);
  /*
   * Add GNSS data to the message, if:
   *  a) There is still record in the PSRAM, and 
   *  b) the message is not full
   *  
   *  Note: pBufFull always assume PJM_RECORDS_MAX_SIZE for the next last record
   */
  while (GnssEventGetRecordCount() && !(pBufFull))
  {
    if (FirstValidFound)
    {
      if (GnssEventPeek(&GnssMontionData))
      {
        if (GnssMontionData.gnssData.Status & GNSS_STATUS_FLAG_DATA_VALID)
        {
          /* Check if overflow happen in Lat/Long, mark it up and jump to the next segment */
          if((abs(GnssMontionData.gnssData.Latitude - LastLatitude) > 0x7FFF) || (abs(GnssMontionData.gnssData.Longitude - LastLongitude) > 0x7FFF))
          {
            /* Overflow occurred in the GNSS delta, break to stop the current segment */
            MessageHeader_t * pMessageHeader = (MessageHeader_t *) MessageBuffer;
            pMessageHeader->GnssStatus |= GNSS_STATUS_FLAG_OVERFLOW;
            break;
          }

          /* Valid GNSS data, check aganist with absolute timestamp to see if any PSRAM data missed (may be caused by GNSS char missing) */
          uint32_t pjmBodyTimestamp = FirstPjmStarttime + TotalPjmRecordAcrossSegments + NoOfRecordsAdded;
          /* Timestamp jumped, Patch in "invalid record" until timestamp aligned  */
          while ((!pBufFull) && (pjmBodyTimestamp < GnssMontionData.gnssData.TimeStamp))
          {
            uint8_t reocrdPatched = PatchPJMWithInvalidData(pBuf, pjmBodyTimestamp, GnssMontionData.gnssData.TimeStamp);
            pjmBodyTimestamp += reocrdPatched;
            NoOfRecordsAdded += reocrdPatched;

            /* Move forward the memory buffer accordingly */
            pBuf += reocrdPatched;
            pBufFull = (pBuf > pLastPJMRecordPosition);
          }
        }
      }
    }

    /* PJM Memory full, break to stop the current segment */
    if (pBufFull)
      break;

    /* Read and consume the Data in PSRAM, fill the content to the message */
    if (GnssEventRead(&GnssMontionData))
    {
      /* The data to add depends on whether the GNSS data is valid */
      if (GnssMontionData.gnssData.Status & GNSS_STATUS_FLAG_DATA_VALID)
      {
        /* data is valid - add the valid flag to the message */
        *pBuf++ = GnssMontionData.gnssData.Status;   // valid flag (1 = valid) + Jamming detect + number of satellites

        /* if this is the first valid fix, we must store the absolute Lat / Long */
        if (!FirstValidFound)
        {
          FirstValidFound = true;
          MessageAddU32(pRefLatRecord, GnssMontionData.gnssData.Latitude);
          MessageAddU32(pRefLongRecord, GnssMontionData.gnssData.Longitude);
          LastLatitude = GnssMontionData.gnssData.Latitude;
          LastLongitude = GnssMontionData.gnssData.Longitude;

          /* Calculate the journey start time based on the first valid GNSS timestamp */
          MessageAddU32(pTimeStampRecord, GnssMontionData.gnssData.TimeStamp - NoOfRecordsAdded);
          ConsolePrintf("Segment %d Timestamp %d\n\r", JourneySegment, GnssMontionData.gnssData.TimeStamp- NoOfRecordsAdded);
          {
            /* update this PJM packet header with first fix data, its030*/
            MessageHeader_t * pMessageHeader = (MessageHeader_t *) MessageBuffer;
            pMessageHeader->TimeStamp = GnssMontionData.gnssData.TimeStamp - NoOfRecordsAdded;
            pMessageHeader->Latitude  = GnssMontionData.gnssData.Latitude;
            pMessageHeader->Longitude = GnssMontionData.gnssData.Longitude;

            if (JourneySegment == 1)
            {
              /* record the first Pjm StartTime for the reference to check if PSRAM data is complete */
              FirstPjmStarttime = pMessageHeader->TimeStamp;
            }
          }
        }

        /* calculate the latitude and longitude delta from previous fix */
        LatitudeDelta = (int16_t)(GnssMontionData.gnssData.Latitude - LastLatitude);
        LongitudeDelta = (int16_t)(GnssMontionData.gnssData.Longitude - LastLongitude);
        LastLatitude = GnssMontionData.gnssData.Latitude;
        LastLongitude = GnssMontionData.gnssData.Longitude;

        /* add postion, speed and heading data to the message */
        pBuf += MessageAddU16(pBuf, LatitudeDelta);
        pBuf += MessageAddU16(pBuf, LongitudeDelta);
        pBuf += MessageAddU16(pBuf, GnssMontionData.gnssData.Speed);
        pBuf += MessageAddU16(pBuf, GnssMontionData.gnssData.Heading);
      }
      else
      {
        /* data not valid - add invalid flag only */
        *pBuf++ = GnssMontionData.gnssData.Status;   // valid flag (0 = invalid) + Jamming detect + number of satellites
      }
      pBufFull = (pBuf > pLastPJMRecordPosition);
      NoOfRecordsAdded++;
    }
    else
    {
      ConsolePrintf("Error: Failed to read GNSS event\n\r");
      break;
    }
  }

  /* Now add the number of "second records" in this message */
  MessageAddU16(pSecondsRecord, NoOfRecordsAdded);

  if (JourneySegment == 1)
  {
    TotalPjmRecordAcrossSegments = NoOfRecordsAdded;
  }
  else
  {
    TotalPjmRecordAcrossSegments += NoOfRecordsAdded;
  }

  /* return the number of bytes added to the buffer */
  return (int)(pBuf - pBuffer);
}


/**
 * @brief  Encode the body for Start and End of Journey messages
 * @param  pBuffer     : pointer to a buffer to copy the data into
 *         isSoj       : bool flag to tell it is SOJ or EOJ Message
 * @retval bytesAdded  : number of bytes added to the buffer
 */
static int MessageEncodeSojEojBody (uint8_t * pBuffer, bool isSoj)
{
  uint8_t * pBuf = pBuffer;

  GnssData_t* gnss_data = (GnssData_t*) SeqJourneyGetGNSSData(isSoj);

  /* Add the Journey ID number */
  pBuf += MessageAddU32(pBuf, SeqJourneyGetJourneyId());

  pBuf += MessageAddU8(pBuf, gnss_data->Status); // valid flag (1 = valid) + Jamming detect + number of satellites

  /* The data to add depends on when the GNSS data is valid */
  if (gnss_data->Status & GNSS_STATUS_FLAG_DATA_VALID)
  {
    /* Add the GNSS info (Lat/Long timestamp etc) for the soj/eoj event */
    pBuf += MessageAddU32(pBuf, gnss_data->TimeStamp);

    pBuf += MessageAddU32(pBuf, gnss_data->Latitude);
    pBuf += MessageAddU32(pBuf, gnss_data->Longitude);

    pBuf += MessageAddU16(pBuf, gnss_data->Speed);
    pBuf += MessageAddU16(pBuf, gnss_data->Heading);
  }

  /* return the number of bytes added to the buffer */
  return (int)(pBuf - pBuffer);
}


/**
 * @brief  Encode the Device Initialiasion message body and add it to the buffer
 * @param  pBuffer     : pointer to a buffer to copy the data into
 * @retval bytesAdded  : number of bytes added to the buffer
 */
static int MessageEncodeDimBody (uint8_t * pBuffer)
{
  uint8_t * pBuf = pBuffer;
  uint32_t bootloaderVersion = GetBLVersionFromSpiRAM();

  /* Get WUST Report Snapshot */
  deviceSnapShot = getWUSTReportInfoSnapShot();

  /* Add reset cause and self test data to message */
  *pBuf++ = (uint8_t)(deviceSnapShot.resetCause);
  pBuf += MessageAddU16(pBuf, deviceSnapShot.WUSTFlag);
  pBuf += MessageAddU16(pBuf, deviceSnapShot.batteryData.BattVoltage);
  pBuf += MessageAddU16(pBuf, deviceSnapShot.batteryData.BattCurrent);

  /* Add 3bytes of bootloader version, major come first */
  pBuf += MessageAddU8(pBuf, bootloaderVersion>>16 &0xff);
  pBuf += MessageAddU8(pBuf, bootloaderVersion>>8  &0xff);
  pBuf += MessageAddU8(pBuf, bootloaderVersion>>0  &0xff);

  /* return the number of bytes added to the buffer */
  return (int)(pBuf - pBuffer);
}


/**
 * @brief  Encode the Device Status message body and add it to the buffer
 * @param  pBuffer     : pointer to a buffer to copy the data into
 * @retval bytesAdded  : number of bytes added to the buffer
 */
static int MessageEncodeDsmBody (uint8_t * pBuffer)
{
  StatusLog_t StatusEvent;
  uint8_t * pBuf = pBuffer;  /*size:  MESSAGE_MAX_MESSAGE_SIZE */

  uint8_t EventCount = 0;

  /* place holder for the number of event field.  We will fill this in at the end */
  pBuf ++ ;
  
  /* Check whether any instant event record exist, append at the end before pack */
  StatusLogAppendInstantEventRecords();
  
  /* Add the events to the Message Body if there is somehting */
  while( StatusLogReadEvent(&StatusEvent, true) )
  {
    pBuf += MessageAddU32(pBuf, StatusEvent.TimeStamp);
    pBuf += MessageAddU8(pBuf, StatusEvent.EventCode);
    pBuf += MessageAddU8(pBuf, StatusEvent.EventData0);
    pBuf += MessageAddU8(pBuf, StatusEvent.EventData1);
    pBuf += MessageAddU8(pBuf, StatusEvent.EventData2);
    pBuf += MessageAddU8(pBuf, StatusEvent.EventData3);
    EventCount++;
  }

  /* fill in the total number of Event in the message */
  *pBuffer = EventCount;

  /* return the number of bytes added to the buffer */
  return (int)(pBuf - pBuffer);
}


/**
 * @brief  Encode the Radio Scan message body and add it to the buffer
 * @param  pBuffer     : pointer to a buffer to copy the data into
 * @retval bytesAdded  : number of bytes added to the buffer
 */
static int MessageEncodeRsmBody (uint8_t * pBuffer)
{
  uint8_t * pBuf = pBuffer;
  BleMac_t  BleScanResult;
  WifiMac_t WifiScanResult;
  int       ResultIndex;
  int       MacIndex;

  /* Add WiFi entries */
  for (ResultIndex = 0; ResultIndex < WIFI_NO_OF_RESULTS; ResultIndex++)
  {
    WifiGetScanResult (&WifiScanResult, ResultIndex);
    for (MacIndex = 0; MacIndex < 12; MacIndex++)
    {
      pBuf += MessageAddU8(pBuf, WifiScanResult.MacAddress[MacIndex]);
    }
    pBuf += MessageAddU8(pBuf, WifiScanResult.Rssi);
  }

  /* Add Bluetooth entries */
  for (ResultIndex = 0; ResultIndex < BLE_NO_OF_RESULTS; ResultIndex++)
  {
    GsmBleGetScanResult(&BleScanResult, ResultIndex);
    for (MacIndex = 0; MacIndex < 12; MacIndex++)
    {
      pBuf += MessageAddU8(pBuf, BleScanResult.MacAddress[MacIndex]);
    }
    pBuf += MessageAddU8(pBuf, BleScanResult.Rssi);
  }

  /* return the number of bytes added to the buffer */
  return (int)(pBuf - pBuffer);
}


/**
 * @brief  Encode the Impact Event message body and add it to the buffer.
 *         Note: Due to its size, the impact data will be spread across several messages
 * @param  pBuffer     : pointer to a buffer to copy the data into
 * @retval bytesAdded  : number of bytes added to the buffer
 */
static int MessageEncodeIevBody (uint8_t * pBuffer)
{
  uint8_t *       pBuf = pBuffer;
  EventSummary_t  EventSummary;
  uint16_t        NoOfRecordsAdded = 0;
  int             BytesAdded = 0;
  uint8_t         tmpData = 0;

  /* Add the Impact ID number */
  *pBuf++ = ImpactId;

  /* Add the Impact Fragment number */
  *pBuf++ = ImpactFragment++;

  /*
   * Add total number of records, and records in this message.
   * Note: MESSAGE_IMPACT_RECORDS_TOTAL is fixed values.
   *       Each impact event is sent as (MESSAGE_IMPACT_RECORDS_TOTAL/MESSAGE_IMPACT_RECORDS_FRAG) messages, 
   *       each containing MESSAGE_IMPACT_RECORDS_FRAG records
   * 
   *       MESSAGE_IMPACT_RECORDS_FRAG depends on the MESSAGE_MAX_BODY_SIZE
   */

  /* Read the number of IMU records remaining in the buffer and make it fit in the MESSAGE_MAX_BODY_SIZE */
  uint32_t NoOfImuRecords = EventStoreGetRecordCount(EVENT_STORE_IMPACT);

  NoOfRecordsAdded = (NoOfImuRecords < MESSAGE_IMPACT_RECORDS_FRAG) ? NoOfImuRecords : MESSAGE_IMPACT_RECORDS_FRAG;
  
  pBuf += MessageAddU16(pBuf, MESSAGE_IMPACT_RECORDS_TOTAL);
  pBuf += MessageAddU8(pBuf, NoOfRecordsAdded);

  /*
   * Get the timestamp and location of the impact, and add to the buffer
   */
  EventStoreReadSummary(EVENT_STORE_IMPACT, &EventSummary);
  pBuf += MessageAddU32(pBuf, EventSummary.TimeStamp);
  pBuf += MessageAddU32(pBuf, EventSummary.Latitude);
  pBuf += MessageAddU32(pBuf, EventSummary.Longitude);

  /* Get the call status of the last call  */
  tmpData = GsmGetVoiceCallStatus();

  /* Check with SeqEmergency if these records with free fall in it */
  tmpData |= SeqEmergencyImpactWithFreeFall() << 7;
  
  /* Put the data into the message buffer */
  pBuf += MessageAddU8(pBuf, tmpData);

  /* Get the call Duration of the last call */
  pBuf += MessageAddU32(pBuf, GsmVoiceGetCallDuration());

  /*
   * Add Impact data to the message, until either:
   *  a) the data is exhausted, or
   *  b) the message is full
   */

  /* Read the next record from SPI SRAM directly into the message buffer */
  EventStoreReadRecords(EVENT_STORE_IMPACT, (ImuData_t *) pBuf, NoOfRecordsAdded, &EventSummary);
  BytesAdded = (NoOfRecordsAdded * sizeof(ImuData_t)) + (pBuf - pBuffer);

  /* return the number of bytes added to the buffer */
  return BytesAdded;
}


/**
 * @brief  Encode a Mini-Event message body and add it to the buffer
 * @param  pBuffer     : pointer to a buffer to copy the data into
 * @retval bytesAdded  : number of bytes added to the buffer
 */
static int MessageEncodeMemBody (uint8_t * pBuffer)
{
  uint8_t *       pBuf = pBuffer;
  EventSummary_t  EventSummary;

  /*
   * Add total number of mini-events and number of mini events in this message.
   */
  uint8_t NoOfMiniEventsRemaining = EventStoreGetEventCount(EVENT_STORE_MINI);
  uint8_t NoOfMiniEventsThisMessage = (NoOfMiniEventsRemaining < MAX_MINIEVENTS_PER_MESSAGE) ? NoOfMiniEventsRemaining : MAX_MINIEVENTS_PER_MESSAGE;
  pBuf += MessageAddU8(pBuf, NoOfMiniEventsThisMessage);

  /* For EACH mini-event... */
  int EvtCnt = 0;
  for (int EvtID = 0; EvtID < MAX_MINI_EVENTS; EvtID++)
  {
    EventSummary.EventID = EvtID;
    /* Add summary information (time and location) */
    EventStoreReadSummary(EVENT_STORE_MINI, &EventSummary);
    /* Confirm valid event
     * Note: The EventSummary.EventID is not zero-indexed in SRAM */
    if (EvtID == (EventSummary.EventID - 1))
    {
      pBuf += MessageAddU8 (pBuf, EventSummary.EventID);
      pBuf += MessageAddU32(pBuf, EventSummary.TimeStamp);
      pBuf += MessageAddU32(pBuf, EventSummary.Latitude);
      pBuf += MessageAddU32(pBuf, EventSummary.Longitude);

      /*
       * Before we do the data decimation, and then delete the event, update EventID due to non-zero index
       * to ensure that we access the correct event.
       */
      EventSummary.EventID -= 1;
      /* Add IMU data to the message.  We always add 3 seconds worth of data, decimated to 10 Hz */
      EventStoreReadAccelDecimated(EVENT_STORE_MINI, (AccelerationData_t *) pBuf, IMU_RECORDS_PER_MINIEVENT, &EventSummary);
      pBuf += (IMU_RECORDS_PER_MINIEVENT * sizeof(AccelerationData_t));

      /* We have finished with this event, so delete it from the store */
      EventStoreDeleteEvent(EVENT_STORE_MINI, &EventSummary);

      /* Increment events in buffer */
      EvtCnt++;
    }
    if (EvtCnt == NoOfMiniEventsThisMessage)
    {
      break;
    }
  }

  /* return the number of bytes added to the buffer */
  return (int)(pBuf - pBuffer);
}


/**
 * @brief  Encode the Red Button message body and add it to the buffer
 * @param  pBuffer     : pointer to a buffer to copy the data into
 * @retval bytesAdded  : number of bytes added to the buffer
 */
static int MessageEncodeRbmBody (uint8_t * pBuffer)
{
  uint8_t *       pBuf = pBuffer;

  /* Add the RedButton ID number */
  *pBuf++ = RedButtonId;

  /* add the latitude Longitude when press the button  */
  pBuf += MessageAddU32(pBuf, ModemStatus.GnssData.Latitude);
  pBuf += MessageAddU32(pBuf, ModemStatus.GnssData.Longitude);

  /* Get the call status of the last call  */
  pBuf += MessageAddU8(pBuf, GsmGetVoiceCallStatus());

  /* Get the call Duration of the last call */
  pBuf += MessageAddU32(pBuf, GsmVoiceGetCallDuration());

  /* return the number of bytes added to the buffer */
  return (int)(pBuf - pBuffer);
}


/**
 * @brief  Encode the Config Value Information message body and add it to the buffer
 * @param  pBuffer     : pointer to a buffer to copy the data into
 * @retval bytesAdded  : number of bytes added to the buffer
 */
static int MessageEncodeCimBody (uint8_t * pBuffer)
{
  uint8_t *   pBuf       = pBuffer;
  uint8_t     BlockType  = 1;
  uint8_t     BlockParam = 0;  // id of flag or parameter
  char        BlockData[80];
  int         NoOfParameters = 0;
  int         NoOfBlocks = 0;

  /* place holder for the number of blocks field.  We will fill this in at the end */
  pBuf += 3;

  /* for each config block */
  for (BlockType = 1; BlockType < CONFIG_MAX_TLVS; BlockType++)
  {
  switch (BlockType)
  {
  case FLAG_TYPE:
    NoOfParameters = CONFIG_MAX_FLAGS;
    break;
  case PARAMETER_TYPE:
    NoOfParameters = CONFIG_MAX_PARAMS;
    break;
  default:
    NoOfParameters = 1;
  }

  for (BlockParam = 0; BlockParam < NoOfParameters; BlockParam++)
  {
      /* get the config data value */
      ConfigTextGetValue(BlockType, BlockParam, BlockData, 80);
      int BlockLength = strlen(BlockData);

      /* populate the block type */
      pBuf += MessageAddAsciiDecimal(pBuf, BlockType, 2);

      /* populate the block length */
      pBuf += MessageAddAsciiDecimal(pBuf, BlockLength, 3);

      /* populate the data */
      for (int i = 0; i < BlockLength; i++)
      {
        *pBuf++ = BlockData[i];
      }
      /* increment the number of blocks added to the message */
      NoOfBlocks++;
  }
  }

  /* fill in the total number of blocks in the message */
  MessageAddAsciiDecimal(pBuffer, NoOfBlocks, 3);

  /* return the number of bytes added to the buffer */
  return (int)(pBuf - pBuffer);
}


/**
 * @brief  Encode the CRC and add it to the buffer
 * @param  pBuffer     : pointer to a buffer to copy the CRC into
 * @param  dataLength  : number of bytes to calculate across
 * @retval bytesAdded  : number of bytes added to the buffer
 */
static int MessageEncodeCrc (uint8_t * pBuffer, int dataLength)
{
  /* Calculate the CRC */
  uint32_t Crc = CrcDriverCalculateCrc32(pBuffer, dataLength);

  /* Copy the CRC to the buffer */
  pBuffer = pBuffer + dataLength;
  MessageAddU32(pBuffer, Crc);

  /* return the number of bytes added to the buffer */
  return (sizeof(Crc));
}


/**
 * @brief  Add u8 data to the buffer
 * @param  pBuffer     : pointer to a buffer to copy the data into
 * @param  data        : the data to add
 * @retval bytesAdded  : number of bytes added to the buffer
 */
static int MessageAddU8 (uint8_t * pBuffer, uint8_t data)
{
  *pBuffer++ = (uint8_t)((data >>  0) & 0xFF);

  return 1;
}


/**
 * @brief  Add u16 data to the buffer
 * @param  pBuffer     : pointer to a buffer to copy the data into
 * @param  data        : the data to add
 * @retval bytesAdded  : number of bytes added to the buffer
 */
static int MessageAddU16 (uint8_t * pBuffer, uint16_t data)
{
  *pBuffer++ = (uint8_t)((data >>  0) & 0xFF);
  *pBuffer++ = (uint8_t)((data >>  8) & 0xFF);

  return 2;
}


/**
 * @brief  Add u32 data to the buffer
 * @param  pBuffer     : pointer to a buffer to copy the data into
 * @param  data        : the data to add
 * @retval bytesAdded  : number of bytes added to the buffer
 */
static int MessageAddU32 (uint8_t * pBuffer, uint32_t data)
{
  *pBuffer++ = (uint8_t)((data >>  0) & 0xFF);
  *pBuffer++ = (uint8_t)((data >>  8) & 0xFF);
  *pBuffer++ = (uint8_t)((data >> 16) & 0xFF);
  *pBuffer++ = (uint8_t)((data >> 24) & 0xFF);

  return 4 ;
}


/*
 * @brief  Convert u8 data to ASCII characters in decimal format and add to the buffer
 * @param  pBuffer     : pointer to a buffer to copy the data into
 * @param  data        : the data to add
 * @retval bytesAdded  : number of bytes added to the buffer
 */
static int MessageAddAsciiDecimal (uint8_t * pBuffer, uint32_t data, uint8_t digits)
{
  uint8_t tmp[8];

  /* sanity check / clip number of digits */
  if (digits > 8) digits = 8;

  /* calculate the decimal digits */
  for (int i = 0; i < digits; i++)
  {
    tmp[i] = (data % 10);
    data = data / 10;
  }

  /* Convert to ASCII and copy into the buffer */
  for (int i = digits-1; i >= 0; i--)
  {
    *pBuffer++ = tmp[i] + ASCII_ZERO;
  }

  return digits;
}


/**
 * @brief   ITS-136 Check data is within expected bounds 
 *          Validate XYZ value within expected bounds befor putting into the Message Header
 * @param   XYZ: imu data to be checked
 * @return  true if the data is OK, false otherwise
 */
static bool MessageHeaderValidateXyz(int16_t X, int16_t Y, int16_t Z)
{
  bool valid = true;

  if ((abs(X) > ERROR_XYZ) 
    || (abs(Y) > ERROR_XYZ) 
    || (abs(Z) > ERROR_XYZ) 
  )
  {
    valid = false;
  }
  return valid;
}

#ifdef MESSAGE_PRINT_MESSAGES
/**
 * @brief  Print the message data
 * @param  Length  : the number of bytes to print
 * @retval None
 */
static void MessagePrintMessage (int Length)
{
  int i, j = 0;

  /* print the header */
  ConsolePrintf("---------- MESSAGE QUEUED ----------\n\r");
  for (i = 0; i <= (Length / 16); i++)
  {
    for (j = 0; j < 16; j++)
    {
      ConsolePrintf(" %02x", MessageBuffer[(i*16) + j]);
    }
    ConsolePrintf("\n\r");
  }
}
#endif
