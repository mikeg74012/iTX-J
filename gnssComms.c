/**
 **************************************************************************************************
 * @file   gnssComms.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  Communication with the GNSS receiver (part of Quectel modem).
 *
 **************************************************************************************************
 * @attention                       (c) Ripcord Designs Limited
 **************************************************************************************************
 */

#include "global.h"

#if MODEM_SEPARATE_GNSS_UART

/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */
/* Debug trace for GNSS Comms */
#if TRACE_GNSSCOMMS_ENABLE
  #define TRACE_GNSSCOMMS(...) TRACE_COLOUR_(FG_RED, BG_DEFAULT, "GNSS", __VA_ARGS__)
#else
  #define TRACE_GNSSCOMMS(...)
#endif

/* ASCII control characters use when decoding AT responses from the modem */
#define CARRIAGE_RETURN           13
#define NEW_LINE                  10

#define SIZE_OF_RESPONSE_BUF      128

/* Message Receipt states */
#define GC_IDLE                   0
#define GC_RECEIVING              1


/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static void GnssCommsInit             (void);
static void GnssCommsProcessData      (char inChar);


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
/* UART access */
static stream_t     * pStream;

/* response buffer to store current response */
static char responseBuf[SIZE_OF_RESPONSE_BUF];
static int  bufIndex = 0;
static int  GnssCommsState = GC_IDLE;


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
/**
 * @brief     The GNSS Comms Task function
 *            NOTE: Currently we listen only to GNSS messages (no transmit from MCU to GNSS)
 * @param     (not used)
 * @return    None
 */
void GnssCommsTask (void const * argument)
{
  uint8_t RxData;

  TRACE_GNSSCOMMS("Task starting...\n\r");

  /* initialise the MCU serial port */
  GnssCommsInit();
  GnssCommsReset();

  /* Main task loop */
  for (;;)
  {
    /*
     * Receive next character from the serial port
     * NOTE: this waits for next character to be received
     */
    RxData = pStream->Get();
    GnssCommsProcessData((char)RxData);
  }
}


/*
 * @brief  Reset the GNSS comms.  Should be called whenever GNSS is powered up.
 * @param  inChar
 * @retval None
 */
void GnssCommsReset (void)
{
  bufIndex = 0;
  GnssCommsState = GC_IDLE;
}


/**
 * @brief  Apply NMEA defined XOR8 checksum algorithm on GNSS returned string
 *          check the ASCII character between $ and *, string checksum is the two chars after *
 * @param  gnssBuf: gnss string
 * @retval true: checksum passed
 */
bool GnssNMEAChecksumXor8(char *gnssBuf)
{
  static uint16_t CrcFailAccumCount = 0;
  uint8_t res          = 0;    /* Init checksum value */
  uint8_t startIndex   = 0;
  uint8_t endIndex     = 0;
  uint8_t j            = 0;
  uint8_t strLength    =  strlen(gnssBuf);

  if (strLength >= SIZE_OF_RESPONSE_BUF)
  {
    CrcFailAccumCount++;
    ConsolePrintf("Invalid GNSS string, Count :(%d) \n\r ", CrcFailAccumCount);
    return false;
  }
  /* locate the checksum sub string, checksum is calculated between $ and * */
  for ( j = 0; j < strLength; j++)
  {
    if (gnssBuf[j] == '$')
    {
      startIndex = j + 1;   /* start */
    }
    else if (gnssBuf[j] == '*')
    {
      endIndex = j - 1 ;   /* end */
    }
  }
  /* if no start and end mark, invalid string, return */
  if ((startIndex == 0) || (endIndex == 0))
  {
    CrcFailAccumCount++;
    ConsolePrintf("invalid GNSS string , Count :(%d) \n\r ", CrcFailAccumCount);
    return false;    /* string start or end missing */
  }
  /* calculate checksum  */
  for ( j = startIndex; j <= endIndex; j++)
  {
    res = res ^ gnssBuf[j];
  }
  /* compare the checksum ASCII code */
  uint8_t nibble1 = (res & 0xF0) >> 4;  /* the first four bits and shifts them 4 positions to the right*/
  uint8_t nibble2 = res & 0x0F;         /* the remaining 4 bits */

  uint8_t crcByte1 = (nibble1<=0x9) ? (nibble1+'0') : (nibble1-10 +'A'); /* converting the number "nibble" into the ASCII representation of that number */
  uint8_t crcByte2 = (nibble2<=0x9) ? (nibble2+'0') : (nibble2-10 +'A');

  /* Check if the checksum calculated from the packet payload matches the checksum in the packet */
  if ((crcByte1 == gnssBuf[endIndex+2]) && (crcByte2 == gnssBuf[endIndex+3]))
  {
    #if TRACE_GNSSCOMMS_ENABLE
      ConsolePrintf(" [%c%c] Checksum OK \n\r", crcByte1,crcByte2);
    #endif
    return true;
  }
  else
  {
    CrcFailAccumCount++;    /* CRC error accumulation count */
    ConsolePrintf("[%c%c-%c%c] GNSS string checksum Failed,  Count :(%d) \n\r ", crcByte1,crcByte2,gnssBuf[endIndex+2],gnssBuf[endIndex+3], CrcFailAccumCount);
    return false;
  }
}



/*
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */
/*
 * @brief  Process incoming data from the GNSS receiver
 *         Sends a message to GSM manager when a complete message has been received
 * @param  inChar
 * @retval None
 */
static void GnssCommsProcessData (char inChar)
{
  /* There are two states, not in message, in message */
  switch (GnssCommsState)
  {
  case GC_IDLE:  /* idle */
    switch (inChar)
    {
    case CARRIAGE_RETURN:
    case NEW_LINE:
      break;
    default:
      bufIndex = 0;
      memset(responseBuf, 0, sizeof(responseBuf));
      responseBuf[bufIndex++] = inChar;
      /* change state */
      GnssCommsState = GC_RECEIVING;
    }
    break;
  case GC_RECEIVING:
    switch (inChar)
    {
    case CARRIAGE_RETURN:
      /* add null terminator */
      if (bufIndex < SIZE_OF_RESPONSE_BUF)
      {
        responseBuf[bufIndex++] = 0;
      }
      break;
    case NEW_LINE:
      TRACE_GNSSCOMMS("Rx-%s ", responseBuf);
      /*
        * Send message to modem task for processing
        * The result code indicates whether the command is complete
        */
      ModemReceiveGnssMessage(responseBuf);

      /* change state */
      GnssCommsState = GC_IDLE;
      break;
    default:
      if (bufIndex < SIZE_OF_RESPONSE_BUF)
      {
        responseBuf[bufIndex++] = inChar;
      }
    }
    break;
  }
}


/**
 * @brief  Initialise the UART used for GNSS communication (RX ONLY)
 * @param  None
 * @retval None
 */
static void GnssCommsInit (void)
{
  uartconfig_t UartConfig;

  UartConfig.BaudRate = 115200;
  UartConfig.DataBits = COM_DATABITS_8;
  UartConfig.StopBits = COM_STOPBITS_1;
  UartConfig.Parity   = COM_PARITY_NONE;
  UartConfig.FlowControl = COM_FLOWCONTROL_NONE;
  pStream = UARTInitialise(GNSS_UART, &UartConfig);
}


#endif /* MODEM_SEPARATE_GNSS_UART */
