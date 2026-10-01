/*
 **************************************************************************************************
 * @file   smsmessages.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  This module encodes and decodes messages used for SMS communications
 *
 **************************************************************************************************
 */

#include "global.h"

/*
 **************************************************************************************************
 *                                         PRIVATE DEFINES
 **************************************************************************************************
 */
#define ASCII_ZERO               48
#define MAGNITUDE_PRECISION      100.0
#define MAX_SMS_GIM_DATA_LENGTH  16


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
/* storage for the SMS message being built */
static uint8_t MessageBufferSms[MAX_SMS_LENGTH];

/*
 * Device Initialisation
 * Used to store and report WUST results for Dim message body
 */
static WUST_ReportInfo_t deviceSnapShot = { 0 };


/*
 **************************************************************************************************
 *                                   PRIVATE FUNCTION PROTOTYPES
 **************************************************************************************************
 */
/* sms message encoding functions */
static int  MessageEncodeSmsEimBody      (uint8_t * pBuffer, bool gprsTxed);
static int  MessageEncodeSmsGimBody      (uint8_t * pBuffer);
static int  MessageEncodeSmsSimBody      (uint8_t * pBuffer);
static int  MessageEncodeSmsCimBody      (uint8_t * pBuffer, uint8_t blockType, uint8_t blockLength, char * pBlockData);
static int  MessageEncodeConfigBlock     (uint8_t * pBuffer, uint8_t blockType, uint8_t blockLength, char * pBlockData);
static int  MessageEncodeSmsCrc          (uint8_t * pBuffer, int dataLength);
static int  MessageEncodeSmsDeviceStatus (uint8_t * pBuffer, uint8_t messageType);
static int  MessageAddDecimal            (uint8_t * pBuffer, uint32_t data, uint8_t digits);
static int  MessageAddSignedHex          (uint8_t * pBuffer, uint32_t data, uint8_t digits);


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */

/**
 * @brief   Encode a SMS emergency summary information message sent via SMS.
 * @param   pMessage - pointer to pointer to put the message location in
 * @param   pMessageSize - pointer to return the size
 * @param   eEmergencyType - emergency type to indicate whether Red button triggered or Impact
 * @param   gprsTxed - flag to indicate whether preceding GPRS/cloud message was transmitted successfully
 * @retval  none
 */
void MessageEncodeSmsEIM (uint8_t ** ppMessage, int * pMessageSize, eEmergencyType messageType, bool gprsTxed)
{
  memset(MessageBufferSms, 0, sizeof(MessageBufferSms));

  /*
   * Build in the standard device status message and copy into the sms message buffer
   */
  int Length = MessageEncodeSmsDeviceStatus(MessageBufferSms, (messageType == ET_IMPACT) ?  (uint8_t) SMS_MESSAGE_TYPE_EMERGENCY :  (uint8_t) SMS_MESSAGE_TYPE_EMERGENCY_RED);

  /* Build in SMS emergency information message and copy into the SMS message buffer */
  Length += MessageEncodeSmsEimBody(MessageBufferSms + Length, gprsTxed);

  /* Calculate CRC and add to the message */
  Length += MessageEncodeSmsCrc(MessageBufferSms, Length);

  /* provide the SMS message details to the caller */
  *ppMessage = MessageBufferSms;
  *pMessageSize = Length;
}


/**
 * @brief   Encode a GSM information message sent via SMS.
 * @param   pMessage - pointer to pointer to put the message location in
 * @param   pMessageSize - pointer to return the size
 * @retval  none
 */
void MessageEncodeSmsGim (uint8_t ** ppMessage, int * pMessageSize)
{
  memset(MessageBufferSms, 0, sizeof(MessageBufferSms));

  /* Build in the standard device status message and copy into the SMS message buffer */
  int Length = MessageEncodeSmsDeviceStatus(MessageBufferSms, (uint8_t) SMS_MESSAGE_TYPE_STATUS_GSM);

  /* Build in SMS GSM information message and copy into the SMS message buffer */
  Length += MessageEncodeSmsGimBody(MessageBufferSms + Length);

  /* Calculate CRC and add to the message */
  Length += MessageEncodeSmsCrc(MessageBufferSms, Length);

  /* provide the SMS message details to the caller */
  *ppMessage = MessageBufferSms;
  *pMessageSize = Length;
}


/**
 * @brief   Encode a SMS device status information message sent via SMS.
 * @param   pMessage - pointer to pointer to put the message location in
 * @param   pMessageSize - pointer to return the size
 * @retval  none
 */
void MessageEncodeSmsSim (uint8_t ** ppMessage, int * pMessageSize)
{
  memset(MessageBufferSms, 0, sizeof(MessageBufferSms));

  /* Build in the standard device status message and copy into the SMS message buffer */
  int Length = MessageEncodeSmsDeviceStatus(MessageBufferSms, (uint8_t) SMS_MESSAGE_TYPE_STATUS_INFO);

  /* Build in SMS device information message and copy into the SMS message buffer */
  Length += MessageEncodeSmsSimBody(MessageBufferSms + Length);

  /* Calculate CRC and add to the message */
  Length += MessageEncodeSmsCrc(MessageBufferSms, Length);

  /* provide the SMS message details to the caller */
  *ppMessage = MessageBufferSms;
  *pMessageSize = Length;
}


/**
 * @brief   Encode a SMS device initialisation message for SMS transmission
 * @param   pMessage - pointer to pointer to put the message location in
 * @param   pMessageSize - pointer to return the size
 * @retval  none
 */
void MessageEncodeSmsDim (uint8_t ** ppMessage, int * pMessageSize)
{
  memset(MessageBufferSms, 0, sizeof(MessageBufferSms));

  /* Build in the standard device status message and copy into the SMS message buffer */
  int Length = MessageEncodeSmsDeviceStatus(MessageBufferSms, (uint8_t) SMS_MESSAGE_TYPE_INIITIALIZATION);

  /* Calculate CRC and add to the message */
  Length += MessageEncodeSmsCrc(MessageBufferSms, Length);

  /* provide the SMS message details to the caller */
  *ppMessage = MessageBufferSms;
  *pMessageSize = Length;
}


/**
 * @brief   Encode a config information message for SMS transmission
 * @param   pMessage - pointer to pointer to put the message location in
 * @param   pMessageSize - pointer to return the size
 * @retval  none
 */
void MessageEncodeSmsCim (uint8_t ** ppMessage, int * pMessageSize, uint8_t blockType, uint8_t blockLength, char * pBlockData)
{
  memset(MessageBufferSms, 0, sizeof(MessageBufferSms));

  /* Build the config information message and copy into the SMS message buffer */
  int Length = MessageEncodeSmsCimBody(MessageBufferSms, blockType, blockLength, pBlockData);

  /* Calculate CRC and add to the message */
  Length += MessageEncodeSmsCrc(MessageBufferSms, Length);

  /* provide the SMS message details to the caller */
  *ppMessage = MessageBufferSms;
  *pMessageSize = Length;
}


/**
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */


/**
 * @brief  Encode the body for SMS emergency information message
 * @param  pBuffer     : pointer to a buffer to copy the data into
 * @param  gprsTxed    : flag to indicate whether preceding GPRS/cloud message was transmitted successfully
 * @retval bytesAdded  : number of bytes added to the buffer
 */
static int MessageEncodeSmsEimBody (uint8_t * pBuffer, bool gprsTxed)
{
  uint8_t * pBuf = pBuffer;

  /* Add the SMS message type ID number */
  *pBuf++ = (gprsTxed == true) ? (uint8_t) 'Y' :(uint8_t) 'N';

  /* Add the peak Speed at time of impact */
  pBuf += MessageAddDecimal(pBuf, (uint8_t) GetImpactMaxSpeed (), 3);

  /* Add the Peak magnitude of impact in unit of g*10.  65 = 6.5g */
  pBuf += MessageAddDecimal(pBuf, (uint8_t) (GetImpactMaxMagnitude ()/MAGNITUDE_PRECISION), 3);

  /* Add the impactArea to the SMS message */
  pBuf += MessageAddSignedHex(pBuf, DetectEventGetImpactForceArea(), 8);

  /* return the number of bytes added to the buffer */
  return (int)(pBuf - pBuffer);
}


/**
 * @brief  Encode the body for SMS device status information message
 * @param  pBuffer     : pointer to a buffer to copy the data into
 * @retval bytesAdded  : number of bytes added to the buffer
 */
static int MessageEncodeSmsGimBody (uint8_t * pBuffer)
{
  char msgData[MAX_SMS_GIM_DATA_LENGTH];
  uint8_t * pBuf = pBuffer;

  /* IMEI */
  for (int i = 0; i < sizeof(ModemStatus.IMEI); i++) 
  {
    uint8_t byte = (ModemStatus.IMEI >> (i * 8)) & 0xFF;  // Extract each byte (little-endian)
    pBuf += MessageAddSignedHex(pBuf, byte, 2);           // Add byte to the buffer
  }

  /* Connection Error number */
  pBuf += MessageAddSignedHex(pBuf, ModemGetNetworkError(), 1);

  /* GSM Signal Strength */
  pBuf += MessageAddDecimal(pBuf, ModemStatus.RSSI, 2);

  /* Journey Data in Queue (Journey Count)*/
  pBuf += MessageAddSignedHex(pBuf, MessageQueueCountMsgType(MESSAGE_TYPE_START_OF_JOURNEY), 2);

  /* Journey Data in Queue (Duration)*/
  pBuf += MessageAddSignedHex(pBuf, MessageQueueGetTotalJourneyDuration(), 4);

  /* Status Info (Battery Voltage)*/
  uint16_t deviceStatus;
  deviceStatus = BatteryMonitorGetUsbState();
  deviceStatus |= ((BatteryMonitorGetVoltage() & 0x1FFF) << 1);
  pBuf += MessageAddSignedHex(pBuf, deviceStatus, 4);

  /* Config Block Type*/
  const int CONFIG_TYPES[] = {IP_ADDRESS_TYPE, IP_PORT_TYPE, APN_USERNAME_TYPE, APN_PASSWORD_TYPE};

  for (int i = 0; i < sizeof(CONFIG_TYPES) / sizeof(CONFIG_TYPES[0]); i++)
  {
    if (ConfigTextGetValue(CONFIG_TYPES[i], 0, msgData, MAX_SMS_GIM_DATA_LENGTH))
    {
      /* Build the SMS Config Value message */
      int BlockLength = strlen(msgData);
      pBuf += MessageEncodeConfigBlock(pBuf, CONFIG_TYPES[i], BlockLength, msgData);
    }
  }

  /* return the number of bytes added to the buffer */
  return (int)(pBuf - pBuffer);
}


/**
 * @brief  Encode the body for SMS device status information message
 * @param  pBuffer     : pointer to a buffer to copy the data into
 * @retval bytesAdded  : number of bytes added to the buffer
 */
static int MessageEncodeSmsSimBody (uint8_t * pBuffer)
{
  int BytesAdded = 0;
  char str[14] = { 0 };

  deviceSnapShot = getWUSTReportInfoSnapShot();

  /* Add Device Status Flag */
  sprintf(str, "%.4X", (int) deviceSnapShot.WUSTFlag);

  for (int i = 0; i < 4 ; i++)
  {
    *pBuffer++ = (uint8_t)str[i];
    BytesAdded += 1;
  }

  /* Add GPS Time to First Fix */
  memset(str, 0, sizeof(str));
  sprintf(str, "%.4X", (int) ModemStatus.GPSTimeToFix );
  for (int i = 0; i < 4 ; i++)
  {
    *pBuffer++ = (uint8_t)str[i];
    BytesAdded += 1;
  }

  /* return the number of bytes added to the buffer */
  return BytesAdded;
}


/**
 * @brief  Encode the body for a config value information message
 * @param  pBuffer     : pointer to a buffer to copy the data into
 * @param  configItem  : The config item to read
 * @retval bytesAdded  : number of bytes added to the buffer
 */
static int MessageEncodeSmsCimBody (uint8_t * pBuffer, uint8_t blockType, uint8_t blockLength, char * pBlockData)
{
  uint8_t *       pBuf = pBuffer;

  /* Add the SMS message type ID number */
  *pBuf++ = SMS_MESSAGE_TYPE_CONFIG_INFO;

  /* Add the SMS message version ID */
  *pBuf++ = (uint8_t) SMS_MSG_VERSION_CHAR_1;
  *pBuf++ = (uint8_t) SMS_MSG_VERSION_CHAR_2;
  
  /* populate the config block with the type and length */
  pBuf += MessageEncodeConfigBlock(pBuf, blockType, blockLength, pBlockData);

  /* return the number of bytes added to the buffer */
  return (int)(pBuf - pBuffer);
}


/**
 * @brief  Encode the body for a config value information message
 * @param  pBuffer     : pointer to a buffer to copy the data into
 * @param  configItem  : The config item to read
 * @retval bytesAdded  : number of bytes added to the buffer
 */
static int MessageEncodeConfigBlock (uint8_t * pBuffer, uint8_t blockType, uint8_t blockLength, char * pBlockData)
{
  uint8_t *       pBuf = pBuffer;

  /* populate the block type */
  pBuf += MessageAddDecimal(pBuf, blockType, 2);

  /* populate the block length */
  pBuf += MessageAddDecimal(pBuf, blockLength, 3);

  /* populate the data */
  for (int i = 0; i < blockLength ; i++)
  {
    *pBuf++ = *pBlockData++;
  }

  /* return the number of bytes added to the buffer */
  return (int)(pBuf - pBuffer);
}


/**
 * @brief  Encode the SMS data packet CRC and add it to the buffer
 * @param  pBuffer     : pointer to a buffer to copy the CRC into
 * @param  dataLength  : number of bytes to calculate across
 * @retval bytesAdded  : number of bytes added to the buffer
 */
static int MessageEncodeSmsCrc (uint8_t * pBuffer, int dataLength)
{
  int BytesAdded = 0;
  char str[5] = { 0 };

  /* Calculate the CRC */
  uint16_t Crc = CrcDriverCalculateCrc16(pBuffer, dataLength);

  /* Copy the CRC to the buffer */
  pBuffer = pBuffer + dataLength;

  /* Add SMS CRC */
  memset(str, 0, sizeof(str));
  sprintf(str, "%.4X", (int) Crc);
  for( int i = 0; i < 4 ; i++)
  {
    *pBuffer++ = (uint8_t)str[i];
    BytesAdded += 1;
  }

  /* return the number of bytes added to the buffer */
  return (BytesAdded);
}


/**
 * @brief  Fill a standard sms message header with current device status data
 * @param  pBuffer     : pointer to a buffer to copy the header into
 * @param  messageType : the message type for this message
 * @retval Length      : Number of bytes added to the buffer
 */
static int MessageEncodeSmsDeviceStatus (uint8_t * pBuffer, uint8_t messageType)
{
  uint8_t *   pBuf = pBuffer;
  static char str[16];

  /* Add the SMS message type ID number */
  *pBuf++ = messageType;

  /* Add the SMS message version ID */
  *pBuf++ = (uint8_t) SMS_MSG_VERSION_CHAR_1;
  *pBuf++ = (uint8_t) SMS_MSG_VERSION_CHAR_2;

  /* Add the firmware version */
  deviceSnapShot = getWUSTReportInfoSnapShot();

  pBuf += MessageAddDecimal(pBuf, deviceSnapShot.AppFirmwareVersion.MajorVersion, 2);
  pBuf += MessageAddDecimal(pBuf, deviceSnapShot.AppFirmwareVersion.MinorVersion, 2);
  pBuf += MessageAddDecimal(pBuf, deviceSnapShot.AppFirmwareVersion.PatchVersion, 2);

  /* Add the current time stamp */
  sprintf(str, "%.10d", (int) RtcGetTimeStamp());
  for (int i = 0; i < 10 ; i++)
  {
    *pBuf++ = (uint8_t)str[i];
  }

  /* Add the Latitude (+/-)DD.dddddd */
  memset(str, 0, sizeof(str));

  /* note - sprintf with floating point argument not currently working on GD32 build */
  snprintf(str, sizeof(str),"%c%02d.%6d", (ModemStatus.GnssData.Latitude > 0) ? '+' : '-', (int)abs((ModemStatus.GnssData.Latitude / 1000000)), abs(ModemStatus.GnssData.Latitude % 1000000));
  for (int i = 0; i < 10 ; i++)
  {
    *pBuf++ = (uint8_t)str[i];
  }

  /* Add the Longitude (+/-)DDD.dddddd */
  memset(str, 0, sizeof(str));

  /* note - sprintf with floating point argument not currently working on GD32 build */
  snprintf(str, sizeof(str),"%c%03d.%6d", (ModemStatus.GnssData.Longitude > 0)?  '+' : '-', (int)abs((ModemStatus.GnssData.Longitude / 1000000)), abs(ModemStatus.GnssData.Longitude % 1000000));
  for (int i = 0; i < 11 ; i++)
  {
    *pBuf++ = (uint8_t)str[i];
  }

  /* Add the Speed SSS.s */
  memset(str, 0, sizeof(str));

  /* note - sprintf with floating point argument not currently working on GD32 build */
  snprintf(str, sizeof(str),"%03d.%1d", (ModemStatus.GnssData.Speed / 10), ModemStatus.GnssData.Speed % 10);
  for( int i = 0; i < 5 ; i++)
  {
    *pBuf++ = (uint8_t)str[i];
  }

  /* Add the Heading HHH.h  */
  memset(str, 0, sizeof(str));

  /* note - sprintf with floating point argument not currently working on GD32 build */
  snprintf(str, sizeof(str),"%03d.%1d", (ModemStatus.GnssData.Heading / 10), ModemStatus.GnssData.Heading % 10);
  for (int i = 0; i < 5 ; i++)
  {
    *pBuf++ = (uint8_t)str[i];
  }

  /* Get current reference acceleration data */
  AccelerationData_t accelData;
  XyzGetFilteredData(&accelData);

  /* Add the Reference Acceleration X */
  pBuf += MessageAddSignedHex(pBuf, accelData.X, 4);

  /* Add the Reference Acceleration Y */
  pBuf += MessageAddSignedHex(pBuf, accelData.Y, 4);

  /* Add the Reference Acceleration Z */
  pBuf += MessageAddSignedHex(pBuf, accelData.Z, 4);

  /* get the orientation */
  int pitch;
  int roll;
  XyzGetOrientation(&pitch, &roll);

  /* Add Pitch*/
  pBuf += MessageAddSignedHex(pBuf, pitch, 4);

  /* Add Roll*/
  pBuf += MessageAddSignedHex(pBuf, roll, 4);

  /* Add GSM LAC */
  memset(str, 0, sizeof(str));
  sprintf(str, "%04X", (int) ModemStatus.GsmLac);
  for (int i = 0; i < 4 ; i++)
  {
    *pBuf++ = (uint8_t)str[i];
  }

  /* Add GSM CID */
  memset(str, 0, sizeof(str));
  sprintf(str, "%04X", (int) ModemStatus.GsmCid);
  for (int i = 0; i < 4 ; i++)
  {
    *pBuf++ = (uint8_t)str[i];
  }

  /* Add Country Code */
  memset(str, 0, sizeof(str));
  sprintf(str, "%03d", (int) ModemStatus.countryCode);
  for (int i = 0; i < 3 ; i++)
  {
    *pBuf++ = (uint8_t)str[i];
  }

  /* Add Operator Code */
  memset(str, 0, sizeof(str));
  sprintf(str, "%03d", (int) ModemStatus.operatorCode);
  for (int i = 0; i < 3 ; i++)
  {
    *pBuf++ = (uint8_t)str[i];
  }

  /* Add Battery Charge Percentage */
  memset(str, 0, sizeof(str));
  sprintf(str, "%02X", (int) (BatteryMonitorGetChargingState() ? ((BatteryMonitorGetLevel() << 1) | 0x01) : (BatteryMonitorGetLevel() << 1)) );
  for (int i = 0; i < 2 ; i++)
  {
    *pBuf++ = (uint8_t)str[i];
  }

  /* Add Device Temperature */
  int8_t temp = (int8_t) (BatteryMonitorGetTemperature() / 10);
  pBuf += MessageAddSignedHex(pBuf, temp, 2);

  /* Add GNSS Status */
  memset(str, 0, sizeof(str));
  sprintf(str, "%02X", (int) ModemStatus.GnssData.Status);
  for (int i = 0; i < 2 ; i++)
  {
    *pBuf++ = (uint8_t)str[i];
  }

  /* return the number of bytes added to the buffer */
  return (int)(pBuf - pBuffer);
}


/*
 * @brief  Convert u8 data characters and add to the buffer
 * @param  pBuffer     : pointer to a buffer to copy the data into
 * @param  data        : the data to add
 * @retval bytesAdded  : number of bytes added to the buffer
 */
static int MessageAddDecimal (uint8_t * pBuffer, uint32_t data, uint8_t digits)
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


/*
 * @brief  Add a signed hex value to the buffer
 * @param  pBuffer     : pointer to a buffer to copy the data into
 * @param  data        : the data to add
 * @retval bytesAdded  : number of bytes added to the buffer
 */
static int MessageAddSignedHex (uint8_t * pBuffer, uint32_t data, uint8_t digits)
{
  char str[14];

  /* sanity check / clip number of digits */
  if (digits > 8) digits = 8;

  /*
   * Create a string representation of the data, always 8 characters wide.
   * Note: this is to take care of the negative number case, where sprintf
   * always left pads with 'F' characters to 32-bits (8 characters)
   */
  memset(str, 0, sizeof(str));
  sprintf(str, "%08X", (unsigned int) data);

  /* copy the digits we care about into the buffer */
  for (int i = (8 - digits); i < 8; i++)
  {
    *pBuffer++ = (uint8_t) str[i];
  }

  return digits;
}


