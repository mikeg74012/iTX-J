/*
 **************************************************************************************************
 * @file   detectevent.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  This module scans the accelerometer ringbuffer for various event types, such as
 *         start of journey, end of journey, impact and mini-event
 *
 **************************************************************************************************
 */

#include "global.h"


/*
 **************************************************************************************************
 *                                         PRIVATE DEFINES
 **************************************************************************************************
 */

/* Time delay to wait after each event, to allow data after the event to be captured */
#define MM_IMPACT_CAPTURE_DELAY_TICKS  ((3 * 1000) / MM_TIMER_PERIOD_MS)       // 3 seconds in MM ticks
#define MM_MINIEV_CAPTURE_DELAY_TICKS  ((8 * 1000) / MM_TIMER_PERIOD_MS)       // 8 seconds in MM ticks

#define RA_BUFFER_LEN           50
#define RA_BUFFER_LEN_3S        300
#define SPEED_UPDATE_LEN        15            /* 15 gnss data set, 15s */
#define EARTH_GRAVITY_VALUE     1050.0

/* Parameters used for SOJ Detection (New Algo)*/
#define DETECT_WINDOW_SIZE      1000
#define DETECT_WINDOW_VALID     0x8000
#define SOJ_THRESHOLD_NUMBER    (0.7 * DETECT_WINDOW_SIZE)

/* Precision multipliers for the force area */
#define AREA_PRECISION                    1000
#define CORNER_EVENT_MINOR_THRESHOLD      500
#define CORNER_EVENT_MODERATE_THRESHOLD   600
#define CORNER_EVENT_MAJOR_THRESHOLD      700

#define CORNER_WARN_NONE                  0
#define CORNER_WARN_MINOR                 1
#define CORNER_WARN_MODERATE              2
#define CORNER_WARN_MAJOR                 3

typedef struct RollingAverage
{
  uint32_t Data[RA_BUFFER_LEN];
  int      WriteIndex;
} RollingAverage_t;


/* Rolling Average Object for 3 seconds Samples */
typedef struct RollingAverage3s
{
  uint32_t Data[RA_BUFFER_LEN_3S];
  int      WriteIndex;
  uint32_t SumOfData;
  uint16_t dataCount;
} RollingAverage3s_t;


typedef struct CornerDetectionToken
{
  bool cornerEventInProcess;
  uint32_t CornerDetectionMs;
  uint8_t warningLevel;

} CornerDetectionToken_t;

/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
/* flag to indicate when impact processing is underway */
static bool ImpactInProcess = false;

static uint32_t PeakImpactMagnitude = 0;
static uint32_t PeakImpactSpeed     = 0;

/* counter to indicated when SoJ detection is in process (motion detected) */
static uint32_t SojTimer = 0;

/* flag to indicate whether to stream calculation results to the console */
static bool StreamCalcs = false;

/* rolling average objects for start and end of journey detection */
static RollingAverage_t RaSoJ;
static RollingAverage_t RaEoJ;

/* rolling average objects for Corner detection, 3 seconds wide */
static RollingAverage3s_t RaCorner;


/*
 **************************************************************************************************
 *                                   PRIVATE FUNCTION PROTOTYPES
 **************************************************************************************************
 */
static int32_t                maxValue                     (int32_t num1, int32_t num2);
static int32_t                minValue                     (int32_t num1, int32_t num2);
static uint32_t               GetMagnitude                 (int32_t x, int32_t y, int32_t z);
static uint32_t               CalculateRollingAverage      (RollingAverage_t * pRa, uint32_t newData);
static uint32_t               CalculateRollingAverage3s    (RollingAverage3s_t * pRa, uint32_t newData);
static uint8_t                updateWarningLevel           (uint32_t rollingMagnitude);
static void                   writeGForceEvent             (uint8_t warningLevel);
static uint32_t               IntegrateForceAreaWithIMUData(ImuData_t* data, uint32_t noOfImuRecords);
static uint32_t               GetCurrentTimeStamp          (void);
static bool                   DetectEventSOJNew            (uint16_t noOfSamples);
static bool                   DetectEventSOJLegacy         (uint16_t noOfSamples);

/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */

/**
 * @brief Dispatcher for the Detect SOJ Event 
 *        with different SOJ detection algorithm -
 * @param  Number of samples Read from IMU, assumed divisible by 6 
 *         (i.e. multiples of Gx Gy Gz XLx XLy XLz)
 * @retval true if SOJ detected
 */
bool DetectEventSOJ(uint16_t noOfSamples)
{
  bool ret = false;
  if (ConfigGetSOJUseNewAlgo())
  {
    ret = DetectEventSOJNew(noOfSamples);
  }
  else
  {
    ret = DetectEventSOJLegacy(noOfSamples);
  }
  return ret;
}


/**
 * @brief Detects SOJ from motion data
 *        SOJ detection algorithm -
 *         1. Perform rolling average of subsequence 50 points
 *         2. If average is > CONFIG_PARAM_SOJ_MAGNITUDE(Threshold B), counter++,
 *         3. else fallback to IMU threshold detection (Threshold A)
 *         4. If counter > CONFIG_PARAM_SOJ_DURATION, ie. enough durations, soj detected
 *         5. counter is in the scale of MM_MS_PER_SAMPLE
 *        NOTE: requires DC removal prior to being called
 * @param  Number of samples Read from IMU, assumed divisible by 6 
 *         (i.e. multiples of Gx Gy Gz XLx XLy XLz)
 * @retval true if SOJ detected
 */
static bool DetectEventSOJLegacy (uint16_t noOfSamples)
{
  bool SojDetected = false;
  int32_t X;
  int32_t Y;
  int32_t Z;
  uint32_t Magnitude;

  /*
   * Scan the motion buffer to detect impact events.
   */
  for (int i = 0; i < noOfSamples; i++)
  {
    X = DetectBuffer[i].accel.X;
    Y = DetectBuffer[i].accel.Y;
    Z = DetectBuffer[i].accel.Z;

    Magnitude = GetMagnitude(X,Y,Z);

    uint32_t Rolling = CalculateRollingAverage(&RaSoJ, Magnitude);

    if (StreamCalcs)
    {
      ConsolePrintf("%d, %d, %d, %d, %d\n\r", X, Y, Z, Magnitude, Rolling);
    }

    if (Rolling > ConfigGetParam(CONFIG_PARAM_SOJ_MAGNITUDE))
    {
      /* calculate time span of the data provided */
      SojTimer += MM_MS_PER_SAMPLE;

      if (SojTimer > ConfigGetParam(CONFIG_PARAM_SOJ_DURATION))
      {
        TRACE_IMU("*****************  SOJ Detected! *******************\n\r");
        TRACE_IMU("XYZ-SOJ X=%dmg, Y=%dmg, Z=%dmg, mag=%d, roll=%d\n\r", X, Y, Z, Magnitude, Rolling);
        SojDetected = true;
        PeakImpactMagnitude = 0;  /* reset Max impact values */
        PeakImpactSpeed = 0;
        SojTimer = 0;
      }
    }
    else
    {
      SojTimer = 0;
    }
  }
  return SojDetected;
}

/* 
 * @brief Increment the roll Index for the flag array 
 *        for the "Threshold B" checking
 *
 * @retval return the new index with the windows valid flag if enough data
 */
static uint16_t increment_rollIdx(uint16_t rollIdx)
 {
    /* Get the current rolling index pointer */
    uint16_t index = rollIdx &~(DETECT_WINDOW_VALID);

    /* Increment the index and clip it by the DETECT_WINDOW_SIZE */
    index = (index + 1) % DETECT_WINDOW_SIZE;

    /* Return the new index with the windows valid flag if wrapping (ever) happened 
       i.e enough data for processing 
    */
    if (index == 0 || (rollIdx & DETECT_WINDOW_VALID))
    {
        index |= DETECT_WINDOW_VALID;
    }

    return index;
}


/**
 * @brief  Detects SOJ from motion data - NEW ALGORITHM
 *         SOJ detection algorithm -
 *         1. Perform rolling average of subsequence 50 points
 *         2. If average is > 0.031g, set flag to '1'
 *         3. if average of previous flags > 70%, soj detected
 *         NOTE: requires DC removal prior to being called
 * @param  Number of samples Read from IMU, assumed divisible by 6 
 *         (i.e. multiples of Gx Gy Gz XLx XLy XLz)
 * @retval true if SOJ detected
 */
static bool DetectEventSOJNew (uint16_t noOfSamples)
{
  bool SojDetected = false;
  int32_t X;
  int32_t Y;
  int32_t Z;
  uint32_t Magnitude;
  static uint8_t flag [DETECT_WINDOW_SIZE];
  static uint16_t flag_idx = 0;

  /*
   * Scan the motion buffer to detect impact events.
   */
  for (int i = 0; i < noOfSamples; i++)
  {
    X = DetectBuffer[i].accel.X;
    Y = DetectBuffer[i].accel.Y;
    Z = DetectBuffer[i].accel.Z;

    Magnitude = GetMagnitude(X,Y,Z);

    uint32_t Rolling = CalculateRollingAverage(&RaSoJ, Magnitude);

    if (StreamCalcs)
    {
      ConsolePrintf("%d, %d, %d, %d, %d\n\r", X, Y, Z, Magnitude, Rolling);
    }

    /* Check if Rolling Average greater than Threshold B, and set the flag */
    uint16_t idx = (flag_idx & ~(DETECT_WINDOW_VALID));
    (Rolling > ConfigGetParam(CONFIG_PARAM_SOJ_MAGNITUDE)) ? (flag[idx] = 1): (flag[idx] = 0);

    /* Update the circular flag rolling index */
    flag_idx = increment_rollIdx(flag_idx);
    
    if (flag_idx & DETECT_WINDOW_VALID)
    {
      /* Enough data, it is valid to check if SojDetected with 70% */
      uint16_t sum = 0;
      for (int j = 0; j < DETECT_WINDOW_SIZE; j++)
      {
          sum += flag[j];
      }

      if (sum > SOJ_THRESHOLD_NUMBER)
      {
        SojDetected = true;

        /* Reset the local statics for the next use */
        memset (flag, 0, DETECT_WINDOW_SIZE);
        flag_idx = 0;
      }
    }

  }
  return SojDetected;
}


/**
 * @brief  Detects EOJ from motion data 
 *         EOJ detection algorithm -
 *         1. Perform rolling average of subsequence 50 points
 *         2. If average is > CONFIG_PARAM_EOJ_MAGNITUDE, counter++, else kept in SOJ
 *         3. If counter > CONFIG_PARAM_EOJ_DURATION, EOJ detected
 *         4. counter is in the scale of MM_MS_PER_SAMPLE
 *         NOTE: requires DC removal prior to being called
 * @param  Number of samples Read from IMU, assumed divisible by 6 
 *         (i.e. multiples of Gx Gy Gz XLx XLy XLz)
 * @retval true if SOJ detected
 */
bool DetectEventEOJ (uint16_t noOfSamples)
{
  static uint32_t EojTimer = 0;
  bool EojDetected = false;
  int32_t X;
  int32_t Y;
  int32_t Z;
  uint32_t Magnitude;

  /*
   * Scan the motion buffer to detect impact events.
   */
  for (int i = 0; i < noOfSamples; i++)
  {
    X = DetectBuffer[i].accel.X;
    Y = DetectBuffer[i].accel.Y;
    Z = DetectBuffer[i].accel.Z;

    Magnitude = GetMagnitude(X,Y,Z);

    uint32_t Rolling = CalculateRollingAverage(&RaEoJ, Magnitude);

    if (StreamCalcs)
    {
      ConsolePrintf("%d, %d, %d, %d, %d\n\r", X, Y, Z, Magnitude, Rolling);
    }
    if (Rolling < ConfigGetParam(CONFIG_PARAM_EOJ_MAGNITUDE))
    {
      /* calculate time span of the data provided */
      EojTimer += MM_MS_PER_SAMPLE;

      if (EojTimer > ConfigGetParam(CONFIG_PARAM_EOJ_DURATION))
      {
        TRACE_IMU("*****************  EOJ Detected! *******************\n\r");
        TRACE_IMU("XYZ-SOJ X=%dmg, Y=%dmg, Z=%dmg, mag=%d, roll=%d\n\r", X, Y, Z, Magnitude, Rolling);
        EojDetected = true;
        EojTimer = 0;
      }
    }
    else
    {
      EojTimer = 0;
    }
  }
  return EojDetected;
}

/**
 * @brief  Compare the current magnitude to the last magnitude to check if there is a freefall
 * @param  magnitude: Current magnitude of the IMU Data sample 
 * @retval true if the freefall criteria met
 */
static bool checkDrop(uint32_t magnitude)
{
  static uint32_t lastMagnitude = 0;
  static uint32_t freefallCount = 0;

  bool hasImpact = false;
  bool isDrop = false;
  uint32_t diffMagnitude = 0;
/* TODO  This doesn't do what it's intended */
  
  diffMagnitude = abs(magnitude - lastMagnitude);

  /* Checks if the difference between the magnitudes is bigger than 0.4G (or 400). */
  if (diffMagnitude >= 400)
  {
    hasImpact = true;
  }
  else if (magnitude < 100)
  {
    freefallCount ++;
  }
  else
  {
    freefallCount = 0;
  }

  // Checks for consecutive freefall before an impact.
  if (freefallCount >= 5 && !hasImpact)
  {
    isDrop = true;
  }

  lastMagnitude = magnitude;

  return isDrop;

}


/**
 * @brief  Detects an Impact from motion data
 *         NOTE: The XYZ offsets have previously been subtracted from the raw accelerometer data
 * @param  Number of samples Read from IMU, assumed divisible by 6 
 *         (i.e. multiples of Gx Gy Gz XLx XLy XLz)
 * @retval true if an impact detected
 */
bool DetectEventImpact (uint16_t noOfSamples)
{
  static EventSummary_t ImpactEventSummary;
  static int ImpulseDurationMs = 0;
  static int CaptureDelayTicks = 0;
  int32_t X;
  int32_t Y;
  int32_t Z;
  uint32_t Magnitude;

  if (false == ImpactInProcess)
  {
    /*
     * Scan the detect buffer to detect impact events.
     */
    for (int i = 0; i < noOfSamples; i++)
    {
      X = DetectBuffer[i].accel.X;
      Y = DetectBuffer[i].accel.Y;
      Z = DetectBuffer[i].accel.Z;

      Magnitude = (uint32_t) (GetMagnitude(X,Y,Z) - EARTH_GRAVITY_VALUE);  /* force a cast to uint32_t */

      if (Magnitude > ConfigGetParam(CONFIG_PARAM_IMPACT_MAGNITUDE))
      {
        /*
         * The measurement must exceed the threshold for a minimum duration
         * Each sample is 10mS
         */
        ImpulseDurationMs += 10;
        if (ImpulseDurationMs > ConfigGetParam(CONFIG_PARAM_IMPACT_DURATION))
        {
          /* Low speed lock-out */
          if (ModemStatus.GnssData.Speed >= ConfigGetParam(CONFIG_PARAM_IMPACT_SPEED))
          {
            TRACE_IMU("*****************  Impact Detected! *******************\n\r");
            TRACE_IMU("XYZ-ABS X=%dmg, Y=%dmg, Z=%dmg, Meas=%dmg, Imp=%dmS\n\r", X, Y, Z, Magnitude, ImpulseDurationMs);
            TRACE_IMU("Max buf[%d] Mag=%d, Speed=%d\n\r", i, PeakImpactMagnitude, PeakImpactSpeed);
            ImpactInProcess = true;
            CaptureDelayTicks = 0;

            /* record the lat / long and timestamp of the impact */
            ImpactEventSummary.Latitude  = ModemStatus.GnssData.Latitude;;
            ImpactEventSummary.Longitude = ModemStatus.GnssData.Longitude;
            ImpactEventSummary.TimeStamp = GetCurrentTimeStamp ();
            /* set init peak values */
            PeakImpactMagnitude = Magnitude;
            PeakImpactSpeed = ModemStatus.GnssData.Speed;
            break;
          }
        }
      }
      else
      {
        ImpulseDurationMs = 0;
      }
    }
  }
  else
  {
    /*
     * Impact previously detected
     * We wait for a defined period to record data after the event before we store it
     */
    if (CaptureDelayTicks++ > MM_IMPACT_CAPTURE_DELAY_TICKS)
    {
      /* store the current motion buffer data to SPI SRAM */
      EventStoreWrite(EVENT_STORE_IMPACT, ImpactEventSummary);

      ImpactInProcess = false;
      return true;  /* this will start the Impact process */
    }
  }
  return false;
}


/**
 * @brief  Detects a mini-event from motion data.  
 *         NOTE: if processing is too slow, could be combined with Impact Detect (above)
 * @param  Number of samples Read from IMU, assumed divisible by 6 
 *         (i.e. multiples of Gx Gy Gz XLx XLy XLz)
 * @retval true if a mini-event detected
 */
bool DetectEventMini (uint16_t noOfSamples)
{
  static EventSummary_t MiniEventSummary;
  static bool MiniEventInProcess = false;
  static int CaptureDelayTicks = 0;
  int32_t X;
  int32_t Y;
  int32_t Z;
  uint32_t Magnitude;

  if (false == ImpactInProcess)
  {
    if (false == MiniEventInProcess)
    {
      /*
       * Scan the motion buffer to detect mini-events.
       */
      for (int i = 0; i < noOfSamples; i++)
      {
        X = DetectBuffer[i].accel.X;
        Y = DetectBuffer[i].accel.Y;
        Z = DetectBuffer[i].accel.Z;

        Magnitude = (uint32_t) (GetMagnitude(X,Y,Z) - EARTH_GRAVITY_VALUE);  /* force a cast to uint32_t */

        if (Magnitude > ConfigGetParam(CONFIG_PARAM_MINIEV_MAGNITUDE))
        {
          TRACE_IMU("***  Mini-event Detected! ***\n\r");
          TRACE_IMU("XYZ-ABS buf[%d], X=%dmg, Y=%dmg, Z=%dmg, Meas=%dmg\n\r", i, X, Y, Z, Magnitude);
          MiniEventInProcess = true;
          CaptureDelayTicks = 0;

          /* record the lat / long and timestamp of the mini-event */
          MiniEventSummary.Latitude  = ModemStatus.GnssData.Latitude;;
          MiniEventSummary.Longitude = ModemStatus.GnssData.Longitude;
          MiniEventSummary.TimeStamp = GetCurrentTimeStamp ();
          break;
        }
      }
    }
    else
    {
      /*
       * Mini-event previously detected
       * We wait for a defined period to record data after the event before we store it
       */
      if (CaptureDelayTicks++ > MM_MINIEV_CAPTURE_DELAY_TICKS)
      {
        /* store the current motion buffer data to SPI SRAM */
        EventStoreWrite(EVENT_STORE_MINI, MiniEventSummary);
        MiniEventInProcess = false;
        return true;
      }
    }
  }
  return false;
}


/**
 * @brief  Detect and log corner event
 * @param  Number of samples Read from IMU, assumed divisible by 6 
 *         (i.e. multiples of Gx Gy Gz XLx XLy XLz)
 * @retval true if a corner-event is in detection
 */
bool DetectEventCorner (uint16_t noOfSamples)
{
  static CornerDetectionToken_t Detection_token;
  uint32_t Rolling = 0;
  uint32_t lockOutSpeed;
  bool qualifiedSpeed = true;;

  lockOutSpeed = ConfigGetParam(CONFIG_PARAM_CORNER_SPEED);

  /* If not in the corner event yet, only start the detection if speed is enough */
  if ((!Detection_token.cornerEventInProcess) )
  {
    qualifiedSpeed = (lockOutSpeed) && (ModemStatus.GnssData.Speed >= lockOutSpeed);
  }

  if ((ImpactInProcess) || !(qualifiedSpeed))
  {
    /* If in Impact detect, or the speed is not qualify yet, skip the corner detection */
    Detection_token.CornerDetectionMs = 0;
    return false;
  }

  /* Calculate the Rolling Average of Y-axis Lateral Force over 3s windows */
  for (int i = 0; i < noOfSamples; i++)
  {
    /* Calculate the rolling average Over 3 seconds windows */
    Rolling = CalculateRollingAverage3s(&RaCorner, abs(DetectBuffer[i].accel.Y));

  }

  /* Check if the Lateral Force is larger than threshold */
  if (Rolling > ConfigGetParam(CONFIG_PARAM_CORNER_MAGNITUDE))
  {
      if (false == Detection_token.cornerEventInProcess)
      {
        /*
         * Check if the lateral Force last for a minimum duration
         * Each sample is 10mS
         */
        Detection_token.CornerDetectionMs += 10;
        if (Detection_token.CornerDetectionMs  > ConfigGetParam(CONFIG_PARAM_CORNER_DURATION))
        {
          /* Low speed lock-out */
          if (ModemStatus.GnssData.Speed >= lockOutSpeed)
          {
            TRACE_IMU("***  Corner turning Detected! with Y=%dmg with speed %d***\n\r", Rolling, ModemStatus.GnssData.Speed);
            Detection_token.cornerEventInProcess = true;
            Detection_token.CornerDetectionMs = 0;
          }
        }
      }
      else
      {
        /* Corner-event in progress, update the warning level with the rolling average */
        uint8_t warningLevel;
        warningLevel = updateWarningLevel(Rolling);

        if (Detection_token.warningLevel != warningLevel)
        {
          TRACE_IMU("Warning Level changed %d -> %d\r\n", Detection_token.warningLevel, warningLevel);
          Detection_token.warningLevel = warningLevel;
          writeGForceEvent(Detection_token.warningLevel);
        }

        /* reset the detection duration for the min duration */
        Detection_token.CornerDetectionMs = 0;

      }
  }
  else
  {
    if (true == Detection_token.cornerEventInProcess)
    {
      /*
       * The measurement must exceed the threshold for a minimum duration
       * Each sample is 10mS
       */
      Detection_token.CornerDetectionMs += 10;
      if (Detection_token.CornerDetectionMs  > ConfigGetParam(CONFIG_PARAM_CORNER_DURATION))
      {
        TRACE_IMU("*** No more turning detected  with Y=%dmg with speed %d***\n\r", Rolling, ModemStatus.GnssData.Speed);
        Detection_token.cornerEventInProcess = false;

        if (Detection_token.warningLevel != CORNER_WARN_NONE)
        {
          TRACE_IMU("Warning Level changed %d -> %d\r\n", Detection_token.warningLevel, CORNER_WARN_NONE);
          Detection_token.warningLevel = CORNER_WARN_NONE;

          /* Record turn back NONE warning level */
          writeGForceEvent(Detection_token.warningLevel);
        }
      }
    }
    else
    {
      Detection_token.CornerDetectionMs = 0;
    }
  }

  return Detection_token.cornerEventInProcess;
}


/**
 * @brief update the GForce record, and return the warning level
 * @param force_array : the array record to be compared
 * @param newRecord   : current new GForce Record
 * @retval patch in the new GForce record and return the warning level
 */
static uint8_t updateWarningLevel(uint32_t rollingMagnitude)
{
  uint8_t warningLevel = CORNER_WARN_NONE;

  if(rollingMagnitude > CORNER_EVENT_MAJOR_THRESHOLD)
  {
    warningLevel = CORNER_WARN_MAJOR;
  }
  else if(rollingMagnitude > CORNER_EVENT_MODERATE_THRESHOLD)
  {
    warningLevel = CORNER_WARN_MODERATE;
  }
  else if(rollingMagnitude > CORNER_EVENT_MINOR_THRESHOLD)
  {
    warningLevel = CORNER_WARN_MINOR;
  }
  else
  {
    warningLevel = CORNER_WARN_NONE;
  }

  return warningLevel;
}


/*
 * @brief  Check whether start of journey detection is in process.  Used to prevent the device from
 *         sleeping if we are in the middle of SoJ detection
 * @param  none
 * @retval true if SoJ detection in process (motion detected), false otherwise
 */
bool DetectEventSojInProcess (void)
{
  if (IsLowPowerSleepFlagUp())
  {
    return (false);  /* In LP sleep mode, this will speed up the Goto Sleep when it is stationary */
  }
  else
  {
    return (SojTimer > 0);
  }
}


/*
 * @brief  Check if impact detection is in process
 * @param  none
 * @retval true if impact detection in process, false otherwise
 */
bool DetectEventImpactInProcess (void)
{
  return ImpactInProcess;
}

/*
 * @brief  return the max impact speed
 *
 * @param  none
 * @retval uint: speed in the unit of m/s
 */
uint8_t GetImpactMaxSpeed (void)
{
  return ((uint8_t) PeakImpactSpeed);
}


/*
 * @brief  return the max impact speed
 *
 * @param  none
 * @retval uint: magnitude in unit of m gravity
 */
uint32_t GetImpactMaxMagnitude (void)
{
  return (PeakImpactMagnitude);
}


/*
 * @brief  Scan the impact data segment to update the peak Magnitude
 *              and peak speed of this impact event
 *
 * @param  ImuData_t *pBuffer, Accel data buffer.
 * @retval none
 */
void ImpactPeakMagnitudeUpdate (ImuData_t *pBuffer)
{
  uint32_t mag = (uint32_t) (GetMagnitude(pBuffer->accel.X, pBuffer->accel.Y, pBuffer->accel.Z) - EARTH_GRAVITY_VALUE) ;
  if (PeakImpactMagnitude < mag )
  {
    PeakImpactMagnitude = mag;
  }
}


/*
 * @brief  Scan the gnss data to fing the matched time stamp and check the update the
 *              peak speed of this impact event upto 10s.
 *
 * @param  uint32_t timeStamp, impact time stamp.
 * @retval none
 */
void ImpactPeakSpeedUpdate (uint32_t timeStamp)
{
  GnssEventMotionData_t DataSet;
  int32_t    ReadingIndex = 0;
  uint32_t   MatchIndex   = 0;
  uint32_t   nloop        = 60;  /* 60s search */
  uint32_t   nRepeat      = 0;   /* repeat read */
  bool       GetEventOK   = true;

  /* find matched Timestamp index, +/- 1s range */
  ReadingIndex = GnssEventWriteIndex();   /* read current write position */
  MatchIndex = ReadingIndex;
  for (nloop = 60; nloop > 0; nloop--)
  {
    GetEventOK = false;
    nRepeat = 3;
    /* Get the GNSS Event with Retry */
    while ((!GetEventOK) && nRepeat)
    {
      GetEventOK = GnssEventGet (ReadingIndex, &DataSet);
      nRepeat --;
    }
    /* If GNSS Event got, check if it is the matched one */
    if (GetEventOK)
    {
      if (DataSet.gnssData.Status & GNSS_STATUS_FLAG_DATA_VALID)  /* valid gnss */
      {
          /* TODO This may not be correct */
          
        if (abs(DataSet.gnssData.TimeStamp - timeStamp) <= 1 )
        {
          MatchIndex = ReadingIndex;
          break;  /* find it */
        }
      }
    }
    ReadingIndex--;
    if (ReadingIndex < 0)
    {
      ReadingIndex = GnssEventGetCountSize();  /*circle buffer */
    }
    sleep(1);
  }

  if ((nloop == 0) || (GnssEventGetRecordCount() == 0))
  {
    ConsolePrintf("Impact speed update failed  RI:%u  TS:[%u-%u]\r\n", MatchIndex, timeStamp, DataSet.gnssData.TimeStamp);
    return;  /* no update found */
  }

  /* read gnss data and update the peak speed if it is higher than already got */
  for (int i = 0; i < minValue (GnssEventGetRecordCount(), SPEED_UPDATE_LEN); i++)
  {
    GetEventOK = false;
    nRepeat = 3;
    while ((!GetEventOK) && nRepeat)  /*read gnss data with retry */
    {
      GetEventOK = GnssEventGet (MatchIndex, &DataSet);
      nRepeat --;
    }

    if (GetEventOK)
    {
      if ((DataSet.gnssData.Status & GNSS_STATUS_FLAG_DATA_VALID) && (DataSet.gnssData.Speed > PeakImpactSpeed))
      {
        PeakImpactSpeed = DataSet.gnssData.Speed;
      }
    }
    MatchIndex--;     /* search backward */
    
    /* TODO This will not work! */
    
    if (MatchIndex < 0) /* circular storage */
    {
      MatchIndex = GnssEventGetCountSize();
    }
  }
}

/**
 * @brief   Get the Impact Force Area in the Impact Event
 * @param   None
 * @return  impactArea : result of the integration of Impact data in the event store
 */
uint32_t DetectEventGetImpactForceArea (void)
{
  ImuData_t data[MESSAGE_IMPACT_RECORDS_FRAG];
  EventSummary_t  EventSummary;
  uint32_t impactArea = 0;
  uint32_t j;

  if (EventStoreGetEventCount(EVENT_STORE_IMPACT) > 0)
  {
    /* Read the number of IMU records in the buffer */
    uint32_t NoOfImuRecords = EventStoreGetRecordCount(EVENT_STORE_IMPACT);
    NoOfImuRecords = (NoOfImuRecords < MESSAGE_IMPACT_RECORDS_TOTAL) ? NoOfImuRecords : MESSAGE_IMPACT_RECORDS_TOTAL;

    EventStoreReadSummary(EVENT_STORE_IMPACT, &EventSummary);

    /* Calculate the Force Area with the IMU Data */
    for (j = 0; j < (NoOfImuRecords / MESSAGE_IMPACT_RECORDS_FRAG); j++)
    {
      /* Read records from SPI SRAM for the force area calculation */
      if (EventStorePeekRecords(EVENT_STORE_IMPACT, (ImuData_t *) data, MESSAGE_IMPACT_RECORDS_FRAG, j*MESSAGE_IMPACT_RECORDS_FRAG, &EventSummary))
      {
        impactArea += IntegrateForceAreaWithIMUData(data, MESSAGE_IMPACT_RECORDS_FRAG);
      }
    }

    if (NoOfImuRecords % MESSAGE_IMPACT_RECORDS_FRAG)
    {
      if (EventStorePeekRecords(EVENT_STORE_IMPACT, (ImuData_t *) data, (NoOfImuRecords % MESSAGE_IMPACT_RECORDS_FRAG), j*MESSAGE_IMPACT_RECORDS_FRAG, &EventSummary))
      {
        impactArea += IntegrateForceAreaWithIMUData(data, NoOfImuRecords % MESSAGE_IMPACT_RECORDS_FRAG);
      }
    }
  }

  return (impactArea/AREA_PRECISION);
}


/**
 * @brief   Check if freefall happen in the Impact Event Store
 * @param   None
 * @return  true if free fall detected, otherwise false
 */
bool DetectFreefallEvent (void)
{
  ImuData_t data;
  EventSummary_t  EventSummary;
  bool isFreeFall = false;  
  uint32_t j;

  if (EventStoreGetEventCount(EVENT_STORE_IMPACT) > 0)
  {
    /* Read the number of IMU records in the buffer */
    uint32_t NoOfImuRecords = EventStoreGetRecordCount(EVENT_STORE_IMPACT);
    NoOfImuRecords = (NoOfImuRecords < MESSAGE_IMPACT_RECORDS_TOTAL) ? NoOfImuRecords : MESSAGE_IMPACT_RECORDS_TOTAL;

    /* Get the Event summary for the address indexing of the later record peeking */
    EventStoreReadSummary(EVENT_STORE_IMPACT, &EventSummary);

    /* Check if any freefall event in the records */
    for (j = 0; j < NoOfImuRecords; j++)
    {
      /* Peek records from SPI SRAM event checking */
      if (EventStorePeekRecords(EVENT_STORE_IMPACT, &data, 1, j, &EventSummary))
      {
        uint32_t magnitude = 0;
        magnitude = GetMagnitude(data.accel.X, data.accel.Y, data.accel.Z);
        isFreeFall = checkDrop(magnitude);
        
        if (isFreeFall)
        {
          TRACE_IMU("*****************  FreeFall Detected! *******************\n\r");
          break;
        }
      }
    }
  }
  return isFreeFall;
}

/**
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */


/*
 * @brief Create a event with warning Level
 * @param  warningLevel    : new warning level
 * @retval none
 */
static void writeGForceEvent(uint8_t warningLevel)
{
  uint32_t eventData = warningLevel;
  StatusLogWriteEventCode(STATUS_IMU_GFORECE_EVENT, eventData);
}

/*
 * @brief  Calculate rolling average across the passed buffer and new data.  Implemented using
 *         a circular buffer.  When buffer is full, new data overwrites the oldest data point.
 * @param  *pRA    : buffer containing the raw data
 * @param  newData : new data to add to the buffer
 * @retval The rolling average across the buffer
 */
static uint32_t CalculateRollingAverage (RollingAverage_t * pRa, uint32_t newData)
{
  uint32_t RollingAve = 0;

  /* add the new data to the buffer */
  pRa->WriteIndex = (pRa->WriteIndex + 1) % RA_BUFFER_LEN;
  pRa->Data[pRa->WriteIndex] = newData;

  /* calculate the rolling average */
  for (int i = 0; i < RA_BUFFER_LEN; i++)
  {
    RollingAve += pRa->Data[i];
  }
  RollingAve = RollingAve / RA_BUFFER_LEN;

  return RollingAve;
}


/*
 * @brief  Calculate rolling average across the passed buffer (3second wide )and new data.  Implemented using
 *         a circular buffer.  When buffer is full, new data overwrites the oldest data point.
 * @param  *pRA    : buffer containing the raw data
 * @param  newData : new data to add to the buffer
 * @retval The rolling average across the buffer
 */
static uint32_t CalculateRollingAverage3s (RollingAverage3s_t * pRa, uint32_t newData)
{
  uint32_t RollingAve = 0;

  /* Get the index position for the new data */
  pRa->WriteIndex = (pRa->WriteIndex + 1) % RA_BUFFER_LEN_3S;

  /* Recalculate the total of the buffer */
  pRa->dataCount++;
  if (pRa->dataCount < RA_BUFFER_LEN_3S)
  {
    /* Buffer not all dirty yet */
    pRa->SumOfData += newData;
  }
  else
  {
    /* Buffer all dirty, set the counter to make it dirty all the way */
    pRa->dataCount = RA_BUFFER_LEN_3S;
    pRa->SumOfData = (pRa->SumOfData - pRa->Data[pRa->WriteIndex]) + newData;
  }

  /* Put the new data in the circular buffer */
  pRa->Data[pRa->WriteIndex] = newData;

  /* calculate the rolling average */
  RollingAve = pRa->SumOfData / RA_BUFFER_LEN_3S;

  return RollingAve;
}

/**
 * Find maximum between two numbers.
 * @param  num1, num2 : the two numbers to compare
 * @retval maxValue   : the maximum value
 */
static inline int32_t maxValue (int32_t num1, int32_t num2)
{
  return (num1 > num2) ? num1 : num2;
}


/**
 * Find minimum between two numbers.
 * @param  num1, num2 : the two numbers to compare
 * @retval maxValue   : the minimum value
 */
static inline int32_t minValue (int32_t num1, int32_t num2)
{
  return (num1 > num2) ? num2 : num1;
}


/**
 * @brief  Integrate the Magnitude of the Imu Data with the precision of AREA_PRECISION
 * @param  data              : pointer to IMU data array
 * @param  noOfImuRecords    : number of records to be integrated
 * @retval impactArea        : result of the integration
 */
static uint32_t IntegrateForceAreaWithIMUData(ImuData_t* data, uint32_t noOfImuRecords)
{
  uint32_t impactArea = 0;

  if (!data)
  {
    return 0;
  }

  for (uint32_t i = 0; i < noOfImuRecords; i++)
  {

    int32_t X;
    int32_t Y;
    int32_t Z;
    uint32_t Magnitude;

    X = data[i].accel.X;
    Y = data[i].accel.Y;
    Z = data[i].accel.Z;

    /* take the square root and normalize it */
    Magnitude = (uint32_t) ((sqrt(X*X + Y*Y + Z*Z) - EARTH_GRAVITY_VALUE ) * AREA_PRECISION);

    /* Calculate the impact Area accordingly */
    impactArea += Magnitude;

  }

  return impactArea;
}


/**
 * @brief  get Magnitude of the vector
 * @param  int32_t x         vector on X axis
 * @param  int32_t y         vector on Y axis
 * @param  int32_t z         vector on Z axis
 * @retval uint32_t vector magnitude
 */
static uint32_t  GetMagnitude (int32_t x, int32_t y, int32_t z)
{
  /* calculate the sum of squares */
  /* return the square root */
 return ((uint32_t)sqrt(x*x + y*y + z*z));
}


/**
 * @brief  get event timestamp, return RTC if GNSS is not available
 * @retval uint32_t unix format time stamp
 */
static uint32_t  GetCurrentTimeStamp (void)
{
  uint32_t gnssTime = GsmGnssGetTimeStamp();
  return ((gnssTime > 0)? gnssTime : RtcGetTimeStamp());
}
/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       Display live output of event calculations
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int DetectEventConsoleCalcs (int argCount, char **pArguments)
{
  switch (argCount)
  {
    case 0:
      ConsolePrintf("Calculation Streaming: %s\n\r", StreamCalcs ? "true" : "false");
      return (CONSOLE_ERROR_OK);
    case 1:
      if (!strcmp(pArguments[0], "true"))
      {
        StreamCalcs = true;
      }
      else if (!strcmp(pArguments[0], "false"))
      {
        StreamCalcs = false;
      }
      else
      {
        /* indicate bad argument */
        return (CONSOLE_ARG_RANGE);
      }
      /* indicate success */
      return (CONSOLE_ERROR_OK);
  }
  /* indicate wrong number of arguments */
  return (CONSOLE_ARG_NO);
}


