/**
 **************************************************************************************************
 * @file   GsmGnss.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  This file implements the function to access the GNSS data from the module
 *
 **************************************************************************************************
 * @attention                       (c) Ripcord Designs Limited
 **************************************************************************************************
 */

#include "global.h"


/*
 **************************************************************************************************
 *                                       PRIVATE DEFINES
 **************************************************************************************************
 */
/* Debug trace for the GNSS */
#if TRACE_GNSS_ENABLE
  #define TRACE_GNSS(...) TRACE_COLOUR_(FG_CYAN, BG_DEFAULT, "GNSS", __VA_ARGS__)
#else
  #define TRACE_GNSS(...)
#endif

/* Choose whether to synchronise time using NTP (1=enable, 0=disable) */
#define GNSS_TIME_SYNC_NTP          1

/* Choose whether to use Quectel GNSS procedure A or B (1=A, 0=B) */
#define QUECTEL_GNSS_MODE_A         1

/* Enable use of NMEA VTG sentence for speed and heading (1=enable, 0=disable) */
#define GNSS_USE_VTG                0

/* Enable gathering of satellite details and SNR (1=enable, 0=disable) */
#define GNSS_GET_SAT_DETAILS        0

#define GNSS_TIMESYNC_TIMEOUT_SECS  5

/* Multiplier to convert knots to kph */
#define KNOT_TO_KPH                 (1.85)

/* GNSS vs MCU time difference before update in seconds */
#define RTCTIMEUPDATETIMEDIFFSEC    (1)

#define GPSNOTACQUIREDVALUE                 (0xFFFFu)

#define GNSS_INITIAL_FIX_TIMEOUT_SECONDS    (600)

#define GNSS_JAMMER_LOG_DELAY_SECONDS       (300)

#define GNSS_STATUS_JAMMER_FREE             1
#define GNSS_STATUS_JAMMER_WARNING          2
#define GNSS_STATUS_JAMMER_CRITICAL         3
#define GNSS_STATUS_JAMMER_MSK              0x03
#define GNSS_STATUS_JAMMER_LOG_MSK          0x04

/* Formula 1 car 0 to 60mph(~52knots) takes 2.1s, it gives the value of ~25 */
#define GNSS_MAX_ACCELERATION_KNOTS_PER_SEC               (25U)
/* 
 * Convert to km/h per second: 1 knot = KNOT_TO_KPH km/h
 * Then scale x10 to store as tenths of km/h for the acceleration comparison
 * ((uint16_t)(GNSS_MAX_ACCELERATION_KNOTS_PER_SEC * KNOT_TO_KPH)/SPEED_PRECISION)
*/
#define GNSS_MAX_ACCELERATION_TENTH_KMH_PER_SEC           (463U)

/* Taking 350 km/h as the max reasonable speed */
#define GNSS_MAX_SPEED_IN_KNOT                            (190.0)
#define GNSS_CONFIDENT_SPEED_COUNT          (3)


typedef struct
{
  uint16_t TimeToFirstFix;
  uint8_t  NoOfSatellites;
  int      AverageSNR;
  int      BestSatSNR;
  int      BestSatId;
  int      BestSatElevation;
  int      BestSatAzimuth;
  uint8_t  JammingStatus;
  uint32_t JammingLogTS;
} GnssStatus_t;



/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
static GnssData_t currentGnssData;

/* Store GPS time to fix from start till we get a valid GPS data */
static int TTFFStartTime;

static bool initialFixStatusReported = false;

/* Store if time synced from GNSS, this flag is used to stop time from syncing again if it is in ajourney */
static bool timeSynced = false;

static char AtCommand[MAX_CONSOLE_AT_LENGTH];

/* GNSS statistics */
static GnssStatus_t GnssStatus;


static uint8_t  GnssConsecutiveValidCnt;
static uint16_t GnssLastTrustedSpeedKphTenths = 0;     /* scaled trusted speed */
static uint32_t GnssLastTrustedTime  = 0;

/*
 **************************************************************************************************
 *                                   EXTERNAL GLOBAL VARIABLES
 **************************************************************************************************
 */
extern bool EPOEnabled;
extern char GNSSData[GNSS_DATA_SIZE];    /* buffer to store GNSS NMEA format data*/
extern bool GNSSDataStored;              /* flag that indicates the current GNSS NMEA-lat and long data is stored in Local buffer */
extern bool velocityStored;              /* flag that indicates the current GNSS NMEA-velocity and heading data is stored in Local buffer */
extern bool GNSSTimeSync;                /* flag that indicates the current GNSS Real Time Clock sync flag */
extern bool NTPTimeSync;                 /* flag that indicates if we have got NTP time sync */
extern bool GNSSPowerON;                 /* flag that indicates the current GNSS power status */
extern bool gnssDataValid;               /* flag that indicates the current GNSS data validity status */


/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static bool GsmGnssSynchroniseTime           (void);
static bool GsmGnssEnableEpo                 (void);
static bool GsmGnssSetReferenceLatLong       (void);
static bool GsmGnssPowerStatus               (void);
static bool GsmGnssPowerOn                   (void);
static bool GsmGnssPowerOff                  (void);
static bool GsmGnssGetLocation               (void);
static bool GsmGnssGetGGA                    (void);
#if GNSS_GET_SAT_DETAILS
 static bool GsmGnssGetSatDetails             (void);
#endif
#if GNSS_USE_VTG
 static void GsmGnssDecodeMessageVTG          (char * GnssData);
#endif
static bool GsmGnssProcessTimeAndDate        (char * pTimeString, char * pDateString);
static void GsmGnssProcessLatLong            (char *moduleLat, char *moduleLong, char latDirection, char longDirection);
static bool GsmGnssSyncRealTimeClock         (struct tm dateTimeGNSS);
static bool GsmGnssSetPmtkMessages           (void);
static void CheckAndLogJammerOnEvent         (int status);
static uint16_t validateGnssSpeed            (float speedKnotVehicle, uint32_t timeStamp);


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
/**
 * @brief  Start up the GNSS service
 * @param  None
 * @retval true on success, false on failure
 */
bool GsmGnssStart (void)
{
  bool result = true;

  /* clear GNSS module time sync flags */
  GNSSTimeSync = false;
  NTPTimeSync = false;

  /* clear GNSS status */
  memset(&GnssStatus, 0, sizeof(GnssStatus));

  /* Reset GPS Time to fix message store flag to not yet acquired flag */
  ModemStatus.GPSTimeToFix = (uint16_t) GPSNOTACQUIREDVALUE;

  /* Get and Store GPS TTF current time stamp */
  TTFFStartTime = RtcGetTimeStamp();

  /* Init the reported flag as false when GNSS Start */
  initialFixStatusReported = false;

  /* Reset the time synced flag when GNSS Start */
  timeSynced = false;

  /* Reset the speed filtering for each GNSS start */
  GnssConsecutiveValidCnt = 0;
  GnssLastTrustedSpeedKphTenths = 0;
  GnssLastTrustedTime = 0;

  /* Reset the Gnss Comms layer */
#if MODEM_SEPARATE_GNSS_UART
  GnssCommsReset();
#endif

#if QUECTEL_GNSS_MODE_A
  /* Power up the GNSS receiver */
  result = GsmGnssPowerOn();
  sleep(1000);
#endif

  /* Make sure we are connected to GPRS */
  result = GsmGprsConnect(2, false);

  /* Make sure GNSS receiver time is synchronised */
  if (result)
  {
    result = GsmGnssSynchroniseTime();
  }

  if ((GNSSTimeSync) || (NTPTimeSync))
  {
    /*
     * If we have existing location data, we use Quectel FastFix Online
     * to set the reference coordinates.  This can significantly speed up TTFF
     * from a cold start.
     */
    GsmGnssSetReferenceLatLong();

    /* Now enable EPO */
    result = GsmGnssEnableEpo();
  }

#if !QUECTEL_GNSS_MODE_A
  /* Power up the GNSS receiver */
  result = GsmGnssPowerOn();
  sleep(1000);
#endif

  /*
   * Set the GNSS receiver to only provide the messages we are interested in.
   * This helps to reduce the amount of junk data we have to deal with (and reduce UART interrupts etc)
   */
  result = GsmGnssSetPmtkMessages();

  return result;
}


/**
 * @brief  Stop the GNSS service
 * @param  None
 * @retval true on success, false on failure
 */
bool GsmGnssStop (void)
{
  bool result = GsmGnssPowerOff();

  return result;
}


/**
 * @brief  Get basic GNSS data (position, speed and heading)
 * @param  storeToSram : if true, the GNSS data is stored to SPI SRAM
 * @retval true of valid GNSS data got in the flag, false otherwise
 */
bool GsmGnssGetData (bool storeToSram)
{
  bool result = false;
  bool validGNSS = false;

  /* Request RMC - get location, speed and heading data from the modem */
  result = GsmGnssGetLocation();

  /* process and store the data */
  if ((result) && (GNSSDataStored))
  {
    /* decode the received RMC and VTG messages */
    result = GsmGnssDecodeMessageRMC(GNSSData);
#if GNSS_USE_VTG
    GsmGnssDecodeMessageVTG(GNSSData);
#endif
    /* reset the stored flag as the data consumed */
    GNSSDataStored = false;
  }
  if (!result)
  {
    /* RMC string decode failed, set GNSS data not valid */
    validGNSS = false;
  }
  else
  {
    /* RMC string decode succeed, check the GNSS data valid flag */
    validGNSS = currentGnssData.Status & GNSS_STATUS_FLAG_DATA_VALID;
  }

  /* Request GGA - Global Positioning Fix Data to get no. of Satellites used */
  if (validGNSS)
  {
    bool ggaResult = GsmGnssGetGGA();

    /* process and store the data */
    if ((ggaResult) && (GNSSDataStored))
    {
      /* decode the received GGA messages */
      GsmGnssDecodeMessageGGA(GNSSData);

      /* reset the stored flag as the data consumed */
      GNSSDataStored = false;
    }
  }

  /* save this gnss data to PSRAM if Decodable, regardless valid or invalid */
  if (result)
  {
    if (storeToSram)
    {
      GnssEventSaveToSram();
    }
  }
  else
  {
    TRACE_GNSS("Error reading / decoding GNSS data\n\r");
  }

#if GNSS_GET_SAT_DETAILS
  GsmGnssGetSatDetails();
#endif

  return (validGNSS);
}


/**
 * @brief  Abort any ongoing Gnss function
 * @param  None
 * @retval true on success, false on failure
 */
bool GsmGnssAbort (void)
{
  //Nothing to do
  return true;
}


/**
 * @brief   Decode the location message (NMEA RMC)
 *          Note: the location data may be valid or invalid
 *           ~ Valid data format: $GNRMC,141947.000,A,5143.1219,N,00017.2387,W,0.57,23.11,120220,,,A*
 *           ~ Invalid data format: $GNRMC,154256.181,V,,,,,0.26,189.92,240220,,,N*53OK"
 * @param   None
 * @return  Invalid data: true on get the invalid flag. false on failed to get the flag
 *          Valid data  : true on get the flag and time stamp from the string. no false return
 */
bool GsmGnssDecodeMessageRMC (char * GnssData)
{
  bool isDataProcessed = false;
  bool msgDecodable = false;
  char dataValidity;
  char DDMMlatModuleOutPut[10];
  char DDMMlongModuleOutput[10];
  char latDir = 0;
  char longDir = 0;
  char time[10];
  char date[10];
  float speedKnotVehicle = 0.0;
  float vehicleHeading = 0.0;

  gnssDataValid = false;

  /* first we read the valid flag to see if the location data is valid */
  msgDecodable = (sscanf(GnssData,"$GNRMC,%*[^,],%c", &dataValidity) == 1);

  if (msgDecodable)
  {
    bool checksumOK = GnssNMEAChecksumXor8(GnssData);
    /* We only decode the rest of the message if the data is valid and no checksum error*/
    if ((!checksumOK) || ('A' != dataValidity))
    {
      /* location data is not valid */
      currentGnssData.Status = 0;
      if (ModemStatus.Jammed)
      {
        currentGnssData.Status |= GNSS_STATUS_FLAG_GSM_JAMMED;
      }
      /* zero out the location data */
      currentGnssData.Latitude = 0;
      currentGnssData.Longitude = 0;
      currentGnssData.Heading = 0;
      currentGnssData.Speed = 0;

#if PROCESS_INVALID_GNSS_TIMEDATE
      /* Decode time and date only.  NOTE: no guarantee values are good */
      msgDecodable = (sscanf(GnssData,"$GNRMC,%[^,],%*c,,,,,%*f,%*f,%[^,]", time, date) == 2);
      if (msgDecodable)
      {
        result = GsmGnssProcessTimeAndDate(time, date);
      }
#else
      currentGnssData.TimeStamp = 0;
#endif
      /* Data procssed but is invalid data got from GNSS or checksum error, 
        Saving as invalid Data to PSRAM to maintain the 1-second interval*/
      isDataProcessed = true;
    }
    else
    {
      gnssDataValid = true;

      /* location data is valid */
      currentGnssData.Status = GNSS_STATUS_FLAG_DATA_VALID;
      if (ModemStatus.Jammed)
      {
        currentGnssData.Status |= GNSS_STATUS_FLAG_GSM_JAMMED;
      }

      /* Update GPSTTF time stamp and copy to store if required */
      if (ModemStatus.GPSTimeToFix == (uint16_t) GPSNOTACQUIREDVALUE)
      {
        /* Evaluate GPS TTF */
        GnssStatus.TimeToFirstFix = RtcGetTimeStamp() - TTFFStartTime;

        /* copy to the modem status store */
        ModemStatus.GPSTimeToFix = GnssStatus.TimeToFirstFix;

        TRACE_GNSS("Time To First Fix (GPS TTF) = %d second(s)\n\r", (int)GnssStatus.TimeToFirstFix);
      }

      /* decode the location data */
      msgDecodable = (sscanf(GnssData,"$GNRMC,%[^,],%*c,%[^,],%c,%[^,],%c,%f,%f,%[^,]", time,
                                                                      DDMMlatModuleOutPut,
                                                                      &latDir,
                                                                      DDMMlongModuleOutput,
                                                                      &longDir,
                                                                      &speedKnotVehicle,
                                                                      &vehicleHeading,
                                                                      date) == 8);

      /* Process the received data strings into a more usable format */
      if (msgDecodable)
      {
        GNSSTimeSync = true;  /* can be decode, GNSS Time is there also */
        currentGnssData.Heading = (vehicleHeading / HEADING_PRECISION);

        /* its-144 validate the Modem reported speed to avoid spike speed reading */
        currentGnssData.Speed = validateGnssSpeed(speedKnotVehicle, RtcGetTimeStamp());

        GsmGnssProcessLatLong(DDMMlatModuleOutPut, DDMMlongModuleOutput, latDir, longDir);
        if (!GsmGnssProcessTimeAndDate(time, date))
        {
          /* change to and invalid point if failed to get time stamp */
          currentGnssData.Status &= (~GNSS_STATUS_FLAG_DATA_VALID);
        }
      }
      else
      {
        /* change to and invalid point if can not decode */
        currentGnssData.Status &= (~GNSS_STATUS_FLAG_DATA_VALID);
      }
      isDataProcessed = true;  /* save this gnss data to PSRAM , valid or invalid */
    }
  }
  else
  {
    /* failed to get validity flag, data cant be processed at all, do not save to PSRAM */
    ConsolePrintf(" GNSS Valid flag decode failed !\n\r");
  }
  return isDataProcessed;
}


/**
 * @brief   Decode the 3D location and accuracy data message (NMEA GGA)
 * @param   None
 * @return  true on success, false on failure
 */
bool GsmGnssDecodeMessageGGA (char * GnssData)
{
  bool result = false;
  int noOfSatellites;

  /* clear the number of satellites bits of the GNSS Status byte */
  currentGnssData.Status &= 0x7;

  /* first we read the valid flag to see if the location data is valid */
  result = (sscanf(GnssData,"$GNGGA,%*[^,],%*[^,],%*[^,],%*[^,],%*[^,],%*[^,],%d", &noOfSatellites) == 1);

  if ((result) && (GnssNMEAChecksumXor8(GnssData)))
  {
    /* Set the number of satellites bits of the GNSS Status byte */
    currentGnssData.Status |= (uint8_t) noOfSatellites << 3;
    GnssStatus.NoOfSatellites = noOfSatellites;

    TRACE_GNSS("Number of SATs: %d\n\r", noOfSatellites);
  }
  else
  {
    /* Checksum error or cannot decode */
    GnssStatus.NoOfSatellites = 0;
  }


  /*
   * Copy the latest GNSS data to the modem store
   * NOTE: This is done once (here) and updates RMC and GGA data.  There is no separate
   *       update in RMC decode, in order to keep the data consistent
   */
  memcpy(&ModemStatus.GnssData, &currentGnssData, sizeof(GnssData_t));

  return result;
}


/**
 * @brief   Decode the satellite information message (PMTKSPF) for Jamming status update
 * @param   GnssData  : GNSS data string
 * @return  true on success, false on failure
 */
bool GsmGnssDecodeMessageMTK (char * GnssData)
{
  bool result = false;
  int  Jamstatus = 0;
  result = (sscanf(GnssData,"$PMTKSPF,%d", &Jamstatus) == 1);
  if ((result) && (ConfigGetFlag(CONFIG_FLAG_GNSS_JAMMING_DETECT)))
  {
    if ((GnssStatus.JammingStatus & GNSS_STATUS_JAMMER_MSK) != (GNSS_STATUS_JAMMER_MSK & Jamstatus))
    {
      TRACE_GNSS ("--> Jammer %s, status=[%d] [%d]\r\n", (Jamstatus == GNSS_STATUS_JAMMER_FREE)? "Free":"Detected !", Jamstatus, GnssStatus.JammingStatus);
      if (GNSS_STATUS_JAMMER_FREE == Jamstatus)
      {
        ModemStatus.Jammed = false;
        if ((GnssStatus.JammingStatus & GNSS_STATUS_JAMMER_MSK) > GNSS_STATUS_JAMMER_FREE)  /* if Jam is previously on */
        {   /* log Jammer off event */
          if (GnssStatus.JammingStatus & GNSS_STATUS_JAMMER_LOG_MSK)
          { /* recording off conditions once */
            StatusLogWriteEventCode(STATUS_GNSS_JAMMER_OFF, STATUS_DATA_LOAD_NONE);
            GnssStatus.JammingStatus &= (~GNSS_STATUS_JAMMER_LOG_MSK);
          }
        }
      }
      else
      {
        ModemStatus.Jammed = true;
        CheckAndLogJammerOnEvent (Jamstatus);
      }
    }

    /* for the case that the jammer switched on in the 5 minutes pause window, log it now */
    if ((!(GnssStatus.JammingStatus & GNSS_STATUS_JAMMER_LOG_MSK)) && (ModemStatus.Jammed))
    {
      CheckAndLogJammerOnEvent (Jamstatus);
    }

    GnssStatus.JammingStatus &= (~GNSS_STATUS_JAMMER_MSK);  /* clear before update */
    GnssStatus.JammingStatus |= (GNSS_STATUS_JAMMER_MSK & Jamstatus);  /* update */
  }
  return result;
}


/**
 * @brief   Decode the satellite information message (GSV). Only called in factory test mode.
 * @param   None
 * @return  true on success, false on failure
 */
bool GsmGnssDecodeMessageGSV (char * GnssData)
{
  bool result = false;
  int TotalMessages = 0;
  int SequenceNumber = 0;
  int SatsInView = 0;
  int SatSnr = 99;
  int SatId = 0;
  int SatEle = 0;
  int SatAzi = 0;
  static int SatCount = 0;
  static int AverageSNR=0;
  static int BestSatSNR = 99; // lower is better
  static int BestSatId = 0;
  static int BestSatElevation = 0;
  static int BestSatAzimuth = 0;

  /*
   * First we determine the number of messages and the sequence number of this message
   * NOTE - we only process GPS satellites for now (GPGSV) and not BeiDou (BNGSV) etc.  This function could be
   *        updated to process all different types
   */
  result = (sscanf(GnssData,"$GPGSV,%d,%d,%d", &TotalMessages, &SequenceNumber, &SatsInView) == 3);

  if (result)
  {
    /* if this is the first message in the sequence, we clear the running stats */
    if (1 == SequenceNumber)
    {
      BestSatSNR = 99;
      AverageSNR = 0;
      SatCount = 0;
    }

    /* Calculate the number of satellites in this message */
    int SatsThisMsg = (SatsInView >= (SequenceNumber * 4)) ? 4 : SatsInView - ((SequenceNumber - 1) * 4);

    /* now process the satellites in the message string */
    while (SatsThisMsg)
    {
      SatsThisMsg--;

      /* skip over 4 fields */
      for (int i = 0; i < 4; i++)
      {
        while ((GnssData[0] != ',') && (GnssData[0] != '*')) GnssData++;
        GnssData++;
      }

      if (sscanf(GnssData,"%d,%d,%d,%d", &SatId, &SatEle, &SatAzi, &SatSnr) == 4)
      {
        /* good data so update the stats */
        if (SatSnr < BestSatSNR)
        {
          BestSatSNR = SatSnr;
          BestSatId = SatId;
          BestSatElevation = SatEle;
          BestSatAzimuth = SatAzi;
        }
        AverageSNR += SatSnr;
        SatCount++;
      }
    }

    /* if this is the last message in the sequence, we store the results */
    if (TotalMessages == SequenceNumber)
    {
      if (SatCount)
      {
        GnssStatus.AverageSNR = AverageSNR / SatCount;
      } else {
          GnssStatus.AverageSNR = 0;
      }
      GnssStatus.BestSatSNR = BestSatSNR;
      GnssStatus.BestSatId = BestSatId;
      GnssStatus.BestSatElevation = BestSatElevation;
      GnssStatus.BestSatAzimuth = BestSatAzimuth;
    }
  }
  return result;
}




/**
 * @brief   Status check for the initial fix timing
 * @param   None
 * @return  None
 */
void GsmGnssInitialFixStatusCheck (void)
{
  /* Still no GPS time fix from GNSS start */
  if (ModemStatus.GPSTimeToFix == (uint16_t) GPSNOTACQUIREDVALUE)
  {
    if ((RtcGetTimeStamp() - TTFFStartTime) > GNSS_INITIAL_FIX_TIMEOUT_SECONDS)
    {
      if (!initialFixStatusReported)
      {
        StatusLogWriteEventCode(STATUS_GNSS_NO_INITIAL_FIX, STATUS_DATA_LOAD_NONE);
        initialFixStatusReported = true;
      }
    }
  }
}


/**
 * @brief   return the current GNSS time stamp
 * @param   None
 * @return  uint32_t latest updated time stamp
 */
uint32_t GsmGnssGetTimeStamp (void)
{
  return currentGnssData.TimeStamp;
}


/**
 * @brief  Enable/disable Jamming detection of GNSS
 * @param  nswitch  :true to enable the Jamming status report function
 * @retval true on success, false on failure
 */
bool GsmGnssJamDetectionControl (bool nswitch)
{
  eAtResult AtResult;
  if (nswitch)
  {
    TRACE_GNSS("Enable Jamming detection\n\r");
    AtResult = gsmCommsSendAtCommand("AT+QGNSSCMD=0,\"$PMTK838,1*2C\"", DEFAULT_AT_TIMEOUT_MS);  /* enable jamming detection */
  }
  else
  {
    TRACE_GNSS("Disable Jamming detection\n\r");
    AtResult = gsmCommsSendAtCommand("AT+QGNSSCMD=0,\"$PMTK838,0*2D\"", DEFAULT_AT_TIMEOUT_MS);
  }

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief     Get current GNSS Data from Modem
 * @param     pGnssData  : Pointer to Current GNSS Data
 * @return    true if success, otherwise false
 */
bool GsmGnssGetCurrentGnss (GnssData_t *pGnssData)
{
  if (pGnssData != NULL)
  {
    memcpy(pGnssData, &currentGnssData, sizeof(GnssData_t));
    return true;
  }
  else
  {
    return false;
  }
}


/**
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */

/**
 * @brief  Set the GNSS receiver to only send the messages we are interested in.  This is to reduce
 *         the number of junk messages we have to process.
 * @param  None
 * @retval true on success, false on failure
 */
static bool GsmGnssSetPmtkMessages (void)
{
  eAtResult AtResult;

  TRACE_GNSS("Set GNSS PMTK Messages\n\r");

  if (TestModeGet())
  {
    /* Set GNSS receiver to output RMC, GGA and GSV messages */
    AtResult = gsmCommsSendAtCommand("AT+QGNSSCMD=0,\"$PMTK314,0,1,0,1,0,5,0,0,0,0,0,0,0,0,0,0,0,0,0*2D\"", DEFAULT_AT_TIMEOUT_MS);
  }
  else
  {
    /* Set GNSS receiver to output RMC and GGA messages only */
    AtResult = gsmCommsSendAtCommand("AT+QGNSSCMD=0,\"$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28\"", DEFAULT_AT_TIMEOUT_MS);
  }

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief  Use Quectel FastFix Online to set reference Lat Long
 * @param  None
 * @retval true on success, false on failure
 */
static bool GsmGnssSetReferenceLatLong (void)
{
  eAtResult AtResult = AT_COMPLETE;

  /* we only set the reference if we have existing, valid location data */
  if ((ModemStatus.GnssData.Latitude) && (ModemStatus.GnssData.Longitude))
  {
    char LatString[13];
    char LongString[13];

    /* build the latitude string */
    int LatDegrees = (int)(ModemStatus.GnssData.Latitude / 1000000);
    int LatDecimal = (int)(ModemStatus.GnssData.Latitude % 1000000);
    if ((0 == LatDegrees) && (LatDecimal < 0))
    {
      snprintf(LatString, sizeof(LatString), "-%d.%06d", LatDegrees, abs(LatDecimal));
    }
    else
    {
      snprintf(LatString, sizeof(LatString), "%d.%06d", LatDegrees, abs(LatDecimal));
    }

    /* build the longitude string */
    int LongDegrees = (int)(ModemStatus.GnssData.Longitude / 1000000);
    int LongDecimal = (int)(ModemStatus.GnssData.Longitude % 1000000);
    if ((0 == LongDegrees) && (LongDecimal < 0))
    {
      snprintf(LongString, sizeof(LongString), "-%d.%06d", LongDegrees, abs(LongDecimal));
    }
    else
    {
      snprintf(LongString, sizeof(LongString), "%d.%06d", LongDegrees, abs(LongDecimal));
    }

    /* Now set up the whole AT command */
    snprintf(AtCommand, (size_t) MAX_CONSOLE_AT_LENGTH, "AT+QGREFLOC=%s,%s", LatString, LongString);

    /* send command to the modem to start dialling */
    AtResult = gsmCommsSendAtCommand(AtCommand, DEFAULT_AT_TIMEOUT_MS);
  }
  return (AtResult == AT_COMPLETE);
}


/**
 * @brief  enable EPO for faster lock
 * @param  None
 * @retval true on success, false on failure
 */
static bool GsmGnssEnableEpo (void)
{
  eAtResult AtResult;

  /* check if EPO is enabled */
  AtResult = gsmCommsSendAtCommand("AT+QGNSSEPO?", DEFAULT_AT_TIMEOUT_MS);

  /* enable EPO if needed */
  if (!EPOEnabled)
  {
    if (AT_COMPLETE == AtResult)
    {
      AtResult = gsmCommsSendAtCommand("AT+QGNSSEPO=1", DEFAULT_AT_TIMEOUT_MS);
    }
  }

#if QUECTEL_GNSS_MODE_A
  if (AT_COMPLETE == AtResult)
  {
    /* trigger EPO usage - not used in Quectel "mode B" */
    AtResult = gsmCommsSendAtCommand("AT+QGEPOAID", DEFAULT_AT_TIMEOUT_MS);
  }
#endif

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief  Request the power status of the GNSS receiver
 * @param  None
 * @retval true on success, false on failure
 */
static bool GsmGnssPowerStatus (void)
{
  eAtResult AtResult;

  TRACE_GNSS("Get GNSS Power Status\n\r");

  /* Request power status of the GNSS receiver */
  AtResult = gsmCommsSendAtCommand("AT+QGNSSC?", DEFAULT_AT_TIMEOUT_MS);

  return ((AtResult == AT_COMPLETE) ? true : false);
}



/**
 * @brief  Turn on power to the GNSS receiver
 *         Note: Checks to see if GNSS is already powered on
 * @param  None
 * @retval true on success, false on failure
 */
static bool GsmGnssPowerOn (void)
{
  eAtResult AtResult = AT_ERROR;
  bool checkStatus = GsmGnssPowerStatus();

  if (isModemSupportGSMOffMode())
  {
    /* GNSS is in stand alone power control mode. reset GNSS power control pin to power it off */
    TRACE_GNSS("GNSS power is enabled now \n\r");
    GpioWritePin(GNSS_POWER_ENABLE_PORT, GNSS_POWER_ENABLE_PIN, IO_PIN_SET);  /* GNSS power off */
    sleep(100);
    AtResult = AT_COMPLETE;
  }
  /* for All-in-One mode */
  if (checkStatus && !GNSSPowerON)
  {
    TRACE_GNSS("Turn GNSS On\n\r");

    /* Power up the GNSS receiver */
    AtResult = gsmCommsSendAtCommand("AT+QGNSSC=1", DEFAULT_AT_TIMEOUT_MS); /* GNSS power on from modem */
  }
  else if (checkStatus)
  {
    AtResult = AT_COMPLETE;
    TRACE_GNSS("GNSS already On\n\r");
  }
  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief  Turn off power to the GNSS receiver
 *         Note: Checks to see if GNSS is already powered off
 * @param  None
 * @retval true on success, false on failure
 */
static bool GsmGnssPowerOff (void)
{
  eAtResult AtResult = AT_ERROR;
  bool checkStatus = false;
  /* for stand alone mode, disable the power control pin */
  if (isModemSupportGSMOffMode())
  {
    /* GNSS is in stand alone power control mode. reset GNSS power control pin to power it off */
    TRACE_GNSS("GNSS power pin is disabled now \n\r");
    GpioWritePin(GNSS_POWER_ENABLE_PORT, GNSS_POWER_ENABLE_PIN, IO_PIN_RESET);  /* GNSS power pin reset off */
    AtResult = AT_COMPLETE;
  }
  else
  {
    checkStatus = GsmGnssPowerStatus();

    if (checkStatus && GNSSPowerON)
    {
      TRACE_GNSS("Turn GNSS Off\n\r");

      /* Power off the GNSS receiver */
      AtResult = gsmCommsSendAtCommand("AT+QGNSSC=0", DEFAULT_AT_TIMEOUT_MS);
    }
    else if (checkStatus)
    {
      AtResult = AT_COMPLETE;
      TRACE_GNSS("GNSS already Off\n\r");
    }
  }

  GnssStatus.JammingStatus = 0;  /* reset status */

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief  Global Positioning Fix Data - provides 3D Location and accuracy data
 * @param  None
 * @retval true on success, false on failure
 */
static bool GsmGnssGetGGA (void)
{
  eAtResult AtResult;

  TRACE_GNSS("Read GNSS 3D data (GGA)\n\r");

  /* Request 3D Location and accuracy data (NMEA GGA) */
  AtResult = gsmCommsSendAtCommand("AT+QGNSSRD=\"NMEA/GGA\"", DEFAULT_AT_TIMEOUT_MS);

  return ((AtResult == AT_COMPLETE) ? true : false);
}



/**
 * @brief  Request location, speed and heading from the modem
 * @param  None
 * @retval true on success, false on failure
 */
static bool GsmGnssGetLocation (void)
{
  eAtResult AtResult;

  TRACE_GNSS("Read GNSS data (RMC)\n\r");

  /* Request location data (NMEA RMC) */
  AtResult = gsmCommsSendAtCommand("AT+QGNSSRD=\"NMEA/RMC\"", DEFAULT_AT_TIMEOUT_MS);

#if GNSS_USE_VTG
  if (AT_COMPLETE == AtResult)
  {
    /* Request speed and heading (NMEA VTG) */
    AtResult = gsmCommsSendAtCommand("AT+QGNSSRD=\"NMEA/VTG\"", DEFAULT_AT_TIMEOUT_MS);
  }
#endif

  return ((AtResult == AT_COMPLETE) ? true : false);
}


#if GNSS_GET_SAT_DETAILS
/**
 * @brief  Request satellite details including SNR
 * @param  None
 * @retval true on success, false on failure
 */
static bool GsmGnssGetSatDetails (void)
{
  eAtResult AtResult;

  TRACE_GNSS("Read GNSS data (GSV)\n\r");
  AtResult = gsmCommsSendAtCommand("AT+QGNSSRD=\"NMEA/GSV\"", DEFAULT_AT_TIMEOUT_MS);

  return ((AtResult == AT_COMPLETE) ? true : false);
}
#endif


/**
 * @brief  Synchronise the GNSS receiver clock using GNSS time or NTP
 * @param  None
 * @retval true on success, false on failure
 */
static bool GsmGnssSynchroniseTime (void)
{
  /* wait for up to five seconds for GNSS Time Sync from the network */
  for (int timeout = 0; timeout < GNSS_TIMESYNC_TIMEOUT_SECS; timeout++)
  {
    gsmCommsSendAtCommand("AT+QGNSSTS?", DEFAULT_AT_TIMEOUT_MS);
    if (GNSSTimeSync)
    {
      break;
    }
    sleep(ONE_SECOND_IN_MS);
  }

#if GNSS_TIME_SYNC_NTP
  if (false == GNSSTimeSync)
  {
    /*
     * We don't have network time for some reason - try NTP instead
     * NOTE: We use pool.ntp.org to find a server.
     * TODO - Need to check legal requirements for commercial use / work with client to make compliant
     */
    if (AT_COMPLETE == gsmCommsSendAtCommand("AT+QNTP=\"pool.ntp.org\"", DEFAULT_AT_TIMEOUT_MS))
    {
      for (int timeout = 0; timeout < GNSS_TIMESYNC_TIMEOUT_SECS; timeout++)
      {
        if (NTPTimeSync)
        {
          break;
        }
        sleep(ONE_SECOND_IN_MS);
      }
    }
  }
#endif

  return (GNSSTimeSync || NTPTimeSync);
}


/**
 * @brief    Check the MCU vs GNSS RTC time deviation and updates MCU time accordingly  *
 * @param    dateTime - the date and time to compare with MCU timestamp
 * @retval   true on success, false on failure
 */
static bool GsmGnssSyncRealTimeClock (struct tm dateTimeGNSS)
{
  bool result = false;
  struct tm DateTimeMCU;
  time_t timeGnss, timeMCU;
  static bool gpsTTFUpdated = false;

  /* Make sure GNSS receiver time is synchronised */
  if ((GNSSTimeSync) || (NTPTimeSync))
  {
    /* 
     * If in a journey, only sync time if not synced
     * If not in a journey, always sync the time if available
     */
    bool isSyncAllowedForSeq = SequencerInJourney() ? !timeSynced : true;

    /* get the MCU current date and time */
    DateTimeMCU = RtcGetDateTime();

    timeGnss = mktime(&dateTimeGNSS);
    timeMCU = mktime(&DateTimeMCU);

    double timeDiff = difftime(timeMCU, timeGnss);

    /* Compare MCU and GNSS time to see if differ by more than 10 seconds */
    if ( ((timeDiff < -RTCTIMEUPDATETIMEDIFFSEC) || (timeDiff > RTCTIMEUPDATETIMEDIFFSEC)) //  <-- Do we need to protect against the quectel bug here?  Reports from iTelematiX indicate RTC is sometimes set back to 1/1/2000
      && (isSyncAllowedForSeq))  /* sync only the seq allowed  */
    {
      TRACE_GNSS("Set Clock\n\r");
      TRACE_GNSS("  MCU  = %02d:%02d:%02d %02d-%02d-%4d\n\r",
          DateTimeMCU.tm_hour, DateTimeMCU.tm_min, DateTimeMCU.tm_sec,
          DateTimeMCU.tm_mday, DateTimeMCU.tm_mon + 1, DateTimeMCU.tm_year + 1900);
          TRACE_GNSS("  GNSS = %02d:%02d:%02d %02d-%02d-%4d\n\r",
          dateTimeGNSS.tm_hour, dateTimeGNSS.tm_min, dateTimeGNSS.tm_sec,
          dateTimeGNSS.tm_mday, dateTimeGNSS.tm_mon + 1, dateTimeGNSS.tm_year + 1900);

      /* Update MCU time with GNSS time */
      dateTimeGNSS.tm_wday = DateTimeMCU.tm_wday;  /* keep the weekday setting */
      RtcSetDateTime(dateTimeGNSS, true);  /* set valid RTC flag */

      /* Update current time stamp used for GPS TTF and copy to store if required */
      if ((!gpsTTFUpdated) && (ModemStatus.GPSTimeToFix == (uint16_t) GPSNOTACQUIREDVALUE))
      {
        gpsTTFUpdated = true;
        /* Update current time stamp used for GPS TTF  */
        TTFFStartTime = RtcGetTimeStamp();
      }

      /* Set time synced flag */
      timeSynced = true;
    }

    RtcSyncedFlagVerify (true);  /* verify to make sure Synced flag has set */

    result = true;
  }

  return result;
}




#if GNSS_USE_VTG
/**
  * @brief   Decode the speed and heading message (NMEA VTG)
  *          Message format: $GNVTG,211.09,T,,M,0.34,N,0.63,K,A*2A
  * @param   None
  * @retval  None
  */
static void GsmGnssDecodeMessageVTG (void)
{
  char state;
  char magnetic;
  char headingDir;
  float speedKnotVehicle = 0.0;
  float speedKmphVehicle = 0.0;
  float vehicleHeading = 0.0;

  if (velocityStored)
  {
    if (sscanf(GNSSData,"$GNVTG,%f,%c,,%c,%f,%c,%f",
                              &vehicleHeading,
                              &state,
                              &magnetic,
                              &speedKnotVehicle,
                              &headingDir,
                              &speedKmphVehicle) == 0)
    {
      /* sscanf Failed */
      TRACE_GNSS("Error processing VTG Data\n\r");
      char state;
      char magnetic;
      headingDir;
      speedKnotVehicle = 0.0;
      speedKmphVehicle = 0.0;
      vehicleHeading = 0.0;
    }
    else
    {
      currentGnssData.heading = (vehicleHeading / HEADING_PRECISION);
      currentGnssData.speed = (speedKnotVehicle / SPEED_KNOT_PRECISION);
    }
  }
}
#endif


/**
 * @brief   Convert the NMEA Time and Date strings into TimeStamp format (seconds since 1/1/2000)
 * @param   pTimeString  : Time string from the GNSS receiver, NMEA format
 * @param   pDateString  : Date string from the GNSS receiver, NMEA format
 * @return  true on success, false on failure
 */
static bool GsmGnssProcessTimeAndDate (char * pTimeString, char * pDateString)
{
  bool result;
  struct tm DateTime;
  uint32_t gnssTimeStamp = 0;

  /* Process the Date and time string from GNSS */
  result = (sscanf(pTimeString,"%2d%2d%2d", &DateTime.tm_hour, &DateTime.tm_min, &DateTime.tm_sec) == 3);

  if (result)
  {
    result = (sscanf(pDateString,"%2d%2d%2d", &DateTime.tm_mday, &DateTime.tm_mon, &DateTime.tm_year) == 3);
    DateTime.tm_year += 100;  //tm_year format is years since 1900
    DateTime.tm_mon -= 1;     //tm_mon format is months since January (i.e. Jan = 0)
  }

  /* convert the GNSS Time and Date to timestamp format if apporiate */
  if (result)
  {
    if ((DateTime.tm_year + 1900) >= 2079)
    {
      TRACE_GNSS("Quectel UTC Time Bug detected - using RTC time\n\r");
      DateTime = RtcGetDateTime();  // fallback to RTC time
    }
    else
    {
      /* Synchronise MCU RTC clock with GNSS time */
      GsmGnssSyncRealTimeClock(DateTime);
    }

    /* Convert current DateTime (from either RTC or GNSS) to timestamp */
    gnssTimeStamp = RtcDateTimeToTimeStamp(DateTime);
    
    /* detect and report discontinuity in timestamps */
    if ((currentGnssData.TimeStamp + 1) != gnssTimeStamp)
    {
      TRACE_GNSS("GNSS unexpected Timestamp (last %d, new %d)\n\r", currentGnssData.TimeStamp, gnssTimeStamp);
    }
    /* Update the timestamp for the current GNSS Data, ready to copy to the modem store */
    currentGnssData.TimeStamp = gnssTimeStamp;
  }
  else
  {
    TRACE_GNSS("Error decoding GNSS Time / Date\n\r");
  }

  return result;
}


/**
 * @brief   Convert the NMEA Latitude and longitude to Decimal Degree Lat Long
 * @param   moduleLat     : Latitude in the form of DDMM.MMMM
 * @param   moduleLong    : Longitude in the form of DDMM.MMMM
 * @param   latDirection  : Latitude  direction character, 'N' or 'S'
 * @param   longDirection : Longitude direction character, 'E' or 'W'
 * @return  None
 */
static void GsmGnssProcessLatLong (char *moduleLat, char *moduleLong, char latDirection, char longDirection)
{
  /*
   * latitude Range -- > -90 to +90
   * (module output for latitude --> llll.ll)
   * longitude Range --> -180 to +180
   * (module output for longitude --> yyyyy.yy)
   * Note: Latitude - DDMM.MMMM
   *       Longitude - DDDMM.MMMM
   *       So first convert the minutes to degrees
   *       then,
   *       DD = D + ( Minutes / 60 )
   */
  int degreeLat = 0;
  int degreeLong = 0;
  double minutesLat = 0.0;
  double minutesLong = 0.0;

  /* Latitude Conversion */
  sscanf(moduleLat,"%2d%lf",&degreeLat,&minutesLat);
  /* convert minutes to degree */
  minutesLat = (minutesLat / 60.0);
  /* add both the DD + MM(in Degree) = Decimal Degree */
  double finalDDLat = degreeLat + minutesLat;
  /* Degree precision upto 0.000001*/
  currentGnssData.Latitude = (finalDDLat / DEGREE_PRECISION);
  /* Assign Direction Sign to latitude */
  if (latDirection == 'S')
  {
    currentGnssData.Latitude = - currentGnssData.Latitude;
  }

  /* Longitude Conversion */
  sscanf(moduleLong,"%3d%lf",&degreeLong,&minutesLong);
  /* convert minutes to degree */
  minutesLong = (minutesLong / 60.0);
  /* add both the DDD + MM(in Degree) = Decimal Degree */
  double finalDDLong = degreeLong + minutesLong;
  /* Degree precision upto 0.000001*/
  currentGnssData.Longitude = (finalDDLong / DEGREE_PRECISION);
  /* Assign Direction Sign to Longitude */
  if (longDirection == 'W')
  {
    currentGnssData.Longitude = - currentGnssData.Longitude;
  }
}


/**
 * @brief    log the Jameer on event with defined pause window  *
 * @param    Jamstatus  : jammer status
 * @retval   none
 */
static void CheckAndLogJammerOnEvent (int Jamstatus)
{
  if ((RtcGetTimeStamp () - GnssStatus.JammingLogTS) > GNSS_JAMMER_LOG_DELAY_SECONDS)
  {  /* log Jammer detected event, for both cases */
    GnssStatus.JammingLogTS = RtcGetTimeStamp ();
    StatusLogWriteEventCode(STATUS_GNSS_JAMMER_DETECTED, Jamstatus);
    GnssStatus.JammingStatus |= GNSS_STATUS_JAMMER_LOG_MSK;
  }
}


/**
 * @brief    validate the reported GNSS speed against the history with confident level
 * @param    speedKnotVehicle     : reported speed from GNSS Module (in knot, float)
 * @param    reportedTimeStamp    : timestamp of the reported speed (from RTC)
 * @retval   return converted speed in kph in x10 scale with defined precision
 */
static uint16_t validateGnssSpeed (float speedKnotVehicle, uint32_t reportedTimeStamp)
{
  /* Sanity check for the speed range */
  if ((speedKnotVehicle < 0) || (speedKnotVehicle > GNSS_MAX_SPEED_IN_KNOT))
  {
    GnssConsecutiveValidCnt = 0;   // reset confidence if non-resonable speed reported
    return 0;
  }

  /* convert speed is to the x10 scale with defined precision */
  uint16_t vehicleSpeedKphTenths = ((KNOT_TO_KPH * speedKnotVehicle) / SPEED_PRECISION);

  if (GnssConsecutiveValidCnt > GNSS_CONFIDENT_SPEED_COUNT)
  {
    /* We got cetain sense of reasonable speed samples, check if it is in the acceptable acceleration (ie. delta speed/ delta time) */
    uint32_t deltaTime = (reportedTimeStamp > GnssLastTrustedTime) ? (reportedTimeStamp - GnssLastTrustedTime) : 1; /* avoid timestamp moved backward by sync */
    uint16_t deltaSpeed = abs(vehicleSpeedKphTenths - GnssLastTrustedSpeedKphTenths);

    /* Check if reasonable acceleration */
    if ((deltaSpeed/deltaTime) > GNSS_MAX_ACCELERATION_TENTH_KMH_PER_SEC)
    {
      /* The acceleration is too large, return the last trusted speed */
      return GnssLastTrustedSpeedKphTenths;
    }
  }

  /* acceleration looks good, trust it and report speed from Modem */
  if (GnssConsecutiveValidCnt < 255)       /* limit of uint8_t of GnssConsecutiveValidCnt */
  {
    GnssConsecutiveValidCnt++;
  }

  GnssLastTrustedSpeedKphTenths = vehicleSpeedKphTenths;
  GnssLastTrustedTime = reportedTimeStamp;
  return vehicleSpeedKphTenths;
}

/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */

/**
 * @brief       Start the GNSS receiver
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int GsmGnssConsoleStart (int argCount, char **pArguments)
{
  if (0 == argCount)
  {
    /* for now the configuration is fixed */
    ModemGnssStart();  /* launch in SEQ */
    return (CONSOLE_ERROR_OK);
  }
  return (CONSOLE_ARG_NO);
}


/**
 * @brief       Stop the GNSS receiver
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int GsmGnssConsoleStop (int argCount, char **pArguments)
{
  if (0 == argCount)
  {
    /* for now the configuration is fixed */
    ModemGnssStop();
    return (CONSOLE_ERROR_OK);
  }
  return (CONSOLE_ARG_NO);
}


/**
 * @brief       Read the GNSS data
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int GsmGnssConsoleRead (int argCount, char **pArguments)
{
  if (0 == argCount)
  {
    /* for now the configuration is fixed */
    GsmGnssGetData(false);
    return (CONSOLE_ERROR_OK);
  }
  return (CONSOLE_ARG_NO);
}


/**
 * @brief       Set the reference lat / long
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int GsmGnssConsoleSetRef (int argCount, char **pArguments)
{
  if (2 == argCount)
  {
    /* get the latitude and longitude */
    sscanf(pArguments[0],  "%d", &ModemStatus.GnssData.Latitude);
    sscanf(pArguments[1],  "%d", &ModemStatus.GnssData.Longitude);
  }
  else
  {
    /* set to Ripcord office if currently zero */
    if ((0==ModemStatus.GnssData.Latitude) && (0==ModemStatus.GnssData.Longitude))
    {
      ConsolePrintf("Setting to Ripcord Office\n\r");
      ModemStatus.GnssData.Latitude = 51718825;
      ModemStatus.GnssData.Longitude = -287288;
    }
  }
  ConsolePrintf("Current reference: lat = %d, long = %d\n\r", ModemStatus.GnssData.Latitude, ModemStatus.GnssData.Longitude);
  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       Output status information about the GNSS system. Used for EOL testing.
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int GsmGnssConsoleStatus (int argCount, char **pArguments)
{
  ConsolePrintf("TTFF = %d\n\r", GnssStatus.TimeToFirstFix);
  ConsolePrintf("No of Satellites = %d\n\r", GnssStatus.NoOfSatellites);
  ConsolePrintf("Ave SNR  = %d\n\r", GnssStatus.AverageSNR);
  ConsolePrintf("Best SNR  = %d\n\r", GnssStatus.BestSatSNR);
  ConsolePrintf("Best Satellite  = %d\n\r", GnssStatus.BestSatId);
  ConsolePrintf("Best Elevation  = %d\n\r", GnssStatus.BestSatElevation);
  ConsolePrintf("Best Azimuth  = %d\n\r", GnssStatus.BestSatAzimuth);
  ConsolePrintf("Jamming Status  = %d\n\r", GnssStatus.JammingStatus & GNSS_STATUS_JAMMER_MSK);
  ConsolePrintf("Last event log  = %s\n\r", (GnssStatus.JammingStatus & GNSS_STATUS_JAMMER_LOG_MSK)? "Jam On":"Jam Free");
  ConsolePrintf("Modem LAC CID  = %d, %d\n\r",  ModemStatus.GsmLac, ModemStatus.GsmCid);
  ConsolePrintf("Modem Country Operator Code  = %d, %d\n\r",  ModemStatus.countryCode, ModemStatus.operatorCode);
  ConsolePrintf("Queued MSG Count  = HP=%d,LP=%d\n\r", GetMessageQueueCount(1), GetMessageQueueCount(0));
  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       access the gnss jammer functions.
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int GsmGnssConsoleJammer (int argCount, char **pArguments)
{
  eAtResult AtResult = AT_NONE;

  if (1 == argCount)
  {
    if (!strcmp(pArguments[0], "on"))
    {
      return ((GsmGnssJamDetectionControl (true))? CONSOLE_ERROR_OK: -1);
    }
    if (!strcmp(pArguments[0], "now"))
    {
      TRACE_GNSS("Update Jamming Info\n\r");
      AtResult = gsmCommsSendAtCommand("AT+QGNSSRD?", DEFAULT_AT_TIMEOUT_MS);  /* looking for $PMTKSPF, jam on first before read */
      return ((AtResult == AT_COMPLETE) ? CONSOLE_ERROR_OK: -1);
    }
    if (!strcmp(pArguments[0], "off"))
    {
       return ((GsmGnssJamDetectionControl (false)) ? CONSOLE_ERROR_OK: -1);
    }
    return (CONSOLE_CMD_UNKNOWN);
  }
  else if (2 == argCount)
  {
    /* Test routine for the speed filtering its-144*/
    float speed = atof(pArguments[1]);
    if (!strcmp(pArguments[0], "speed"))
    {
      ConsolePrintf("Input speed as if GNRMC %d.%02d\r\n", ((int)(speed*100)) / 100, ((int)(speed*100)) % 100);
      ConsolePrintf("Rtn val after validation of speed and accel %d\r\n", validateGnssSpeed(speed, RtcGetTimeStamp()));
      ConsolePrintf("Last trusted speed value %d\r\n", GnssLastTrustedSpeedKphTenths);
    }
  }
  return CONSOLE_ERROR_OK;
}

