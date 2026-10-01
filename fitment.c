/**
 **************************************************************************************************
 * @file   fitment.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   
 *
 * @brief  Functions for performing fitment survey and calculating the mounting angle of the device.
 *         This file contains the logic required to determine the optimal fitment and mounting angle
 *         based on specific survey data and calculations
 * 
 **************************************************************************************************
 */

#include "global.h"


/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */

/* Debug trace for mounting survey */
#if TRACE_MOUNT_ENABLE
  #define TRACE_MOUNT(...) TRACE_COLOUR_(FG_CYAN, BG_DEFAULT, "MOUNT", __VA_ARGS__)
#else
  #define TRACE_MOUNT(...)
#endif

/* For ITS-143 we switch to using a calculation that produces positive pitch values */
#define POSITIVE_PITCH_VALUES        1

/* Fitment Process related flag */
#define RMP_LAST_MOUNTED_STATUS      0x80           /* Cached last know mounted status */
#define RMP_NORMALISATION_ON         0x40           /* Apply rmp to accel output data enabled */
#define RMP_DEBUG_MOUNT_SET          0x20           /* set rmp matrix using current P/R, debug use only */
#define RMP_MOUNT_CHECK              0x10           /* set to activiate the fitment checking*/

#define RMP_POSITIVE_Z               0x08
#define RMP_ACTIVE_MOUNT_FLAG        0x04           /* Current status flag indicating whether the device is mounted */
#define RMP_MOUNT_FACEUP             0x02
#define RMP_MOUNT_FACEDOWN           0x01

/* Mount Survey Pocket Bin Flag  */
#define XYZ_MOUNT_SURVEY_POCKET         101          /* Pitch and Roll Bin Count (P: -90, 10 R: -50, 50) */
#define PITCH_INDEX                     0            /* Journey Histogram Pitch Index*/
#define ROLL_INDEX                      1            /* Journey Histogram Roll Index */
#ifdef POSITIVE_PITCH_VALUES
#define PITCH_START                     0           /* 0 to 100 degrees */
#else
#define PITCH_START                    -90           /* -90 to 10 degrees */
#endif
#define ROLL_START                     -50           /* -50 to 50 degrees */
#define SHELF_PITCH                     0            /* Pitch value in degree used to set the shelf face up/down flag */
#define SHELF_ROLL                      0            /* Roll value in degree used to set the shelf face up/down flag */

#define MOUNT_CONFIDENCE_PERCENT        10           /* Min Percentage confidence threshold Peak method  */
#define MOUNT_CONFIDENCE_SAMPLE         8            /* Min Sample count for confidence check */
                                                     /* 1m20s drive (MOUNT_CONFIDENCE_SAMPLE*FITMENT_SURVEY_INTERVAL) */

/* Mount Survey PSRAM related define */
#define MOUNT_SURVEY_COMPLETE           10           /* Back survey data, save data to the page selected */
#define MOUNT_CONSOLE_CMD_SHIFT         MOUNT_SURVEY_COMPLETE     /* Console command base value for page selection */

#define MOUNT_SURVEY_CURRENT_PAGE       0             /* Page number for the current (active) survey data*/
#define MOUNT_SURVEY_DEBUG_PAGES        7             /* Number of pages for saving data for debug use*/


/* Mount Survey result related internval/range parameters */
#define FACE_UP_DOWN_RANGE              6            /* Default face up/down range allowed if not specified in config para*/
#define MOUNT_SURVEY_DATA_CAP           2            /* survey angle delta upper limit in unit of degree */
#define MOUNT_OFF_REPORT_INTERVAL       300          /* Min interval to report mount off event */

typedef enum
{
  ZYX,
  ZYZ,
  XYZ  /* used for the rmp */
}AxisSequence;

typedef struct
{

    /* --- Current Mounting Data, init from flash, could be updated to surveyed one if conditions met --- */
    int16_t           m_pitch;         /* currently mount pitch */
    int16_t           m_roll;          /* currently mount roll */

    /* ---  Survey Data --- */
    uint32_t          scountp;         /* Survey pitch sample counts */
    uint32_t          scountr;         /* Survey roll sample counts */
    int16_t           surveyed_pitch;  /* Surveyed Pitch, derivied from method specified */
    int16_t           surveyed_roll;   /* Surveyed Roll, derivied from method specified */

    uint32_t          lastMntEventReportTStamp; /* Previous mount off event log stamp */
    uint32_t          surveyQualifiedSamples;   /* Counter to track the number of samples that meet the required conditions (e.g., speed).*/
    bool              isSurveyResultConfident ; /* Surveyed confident to tell if it is ready to patch in the mount angle config*/

    /* ---  Process Control Flags and States --- */
    uint8_t           PC_FLAG;         /* process control flag */
    uint8_t           debugHistoryPage;   /* PSRAM page for saving for debug use */
    
    /* --- Rotated Data --- */
    float             RX;              /* rotated x */
    float             RY;              /* rotated y */
    float             RZ;              /* rotated z */
    int16_t           RP;              /* rotated pitch */
    int16_t           RR;              /* rotated roll */

} RotationParameter_t;

/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
static RotationParameter_t  rmp;
static bool          debugIgnoreSpeed                  = false;
static bool          streamTest                        = false;

/*
 **************************************************************************************************
 *                                   PRIVATE FUNCTION PROTOTYPES
 **************************************************************************************************
 */
static void            XyzApplyMatrix                  (int xi, int yi, int zi, int pitch, int roll);

static void            FitmentHistogramCollect         (int16_t pitch, int16_t roll);
static void            DisplayInJourneyHistogram       (void);

#if DEBUG
static void            RmpXYZSetCurrentEnable          (uint8_t nc);
#endif
static void            UpdateSurveyResultsWithJourneyData     (void);
static void            GetPeakPitchRollSurveyPercentage       (uint8_t *peakPitchPercentage, uint8_t *peakRollPercentage);
static void            GetAvgPitchRollSurvey                  (int16_t *avgPitch, int16_t *avgRoll);


static uint16_t               mSurveyPR[XYZ_MOUNT_SURVEY_POCKET][2]; /* record pitch and roll */
static double                 RMatrix[3][3];   /* rotation matrix */
static double                 DRFactor; /* degree to radiant converter*/

static bool                   WriteSurveyDataToSpiRAM        (uint8_t n);
static bool                   ReadSurveyDataFromSpiRAM       (uint8_t n);
static int16_t                ExtractSignedValue             (uint8_t byte);
static void                   GetMountAngleFromConfig        (int16_t *pitch, int16_t *roll);
static void                   SetMountAngleToConfig          (int16_t pitch, int16_t roll);
static void                   UpdateMountAngleFrmSurvey      (void);
static uint16_t               CombinePitchRoll               (int16_t pitch, int16_t roll);
static void                   MountSurveySendEventlog        (void);
static void                   CheckMountStatusWithConfig     (int16_t pitch, int16_t roll);
static void                   EvaluateZAxisOrientation       (int16_t Zvalue);
static void                   UpdateLastKnownMntStatus       (bool currentMountStatus);
static void                   FitmentSurveyCompleteAndBackup (void);
static void                   FitmentSurveyDataReset         (void);

/* Mount status related functios */
static void                   FitmentCheckMountStatusAgainstConfig(int16_t pitch, int16_t roll);

/*
 **************************************************************************************************
 *                                   PUBLIC SECTION / INTERFACE
 **************************************************************************************************
 */


/**
 * @brief    Helper function Import/backup/reset the fitment related data (Survey and rmp struct)
 *
 * @param     mode_control  : mode control. 21 read, 20 write, 0 reset
 * @return    True          : if successful, false otherwise
 */
bool FitmentDataHandler (uint8_t mode_control)
{
   bool success = false;
   
   switch (mode_control)
   {
      case MOUNT_SURVEY_IMPORT:
        success = ReadSurveyDataFromSpiRAM(MOUNT_SURVEY_CURRENT_PAGE);  /* Read current data (page 0)*/
        TRACE_MOUNT("Read histogram data page 0 back \r\n");
        break;

      case MOUNT_SURVEY_BACKUP:
        success = WriteSurveyDataToSpiRAM(MOUNT_SURVEY_CURRENT_PAGE); /* Write to current data (page 0)*/
        TRACE_MOUNT("Save survey histogram data to page 0 \r\n");
        break;
        
      case MOUNT_SURVEY_COMPLETE:
        /* update the survey data before backup */
        UpdateSurveyResultsWithJourneyData();
        if  ((rmp.debugHistoryPage>= 1) && (rmp.debugHistoryPage<= MOUNT_SURVEY_DEBUG_PAGES))
        {
          success = WriteSurveyDataToSpiRAM(rmp.debugHistoryPage);    /* muliti-page save for last MOUNT_SURVEY_DEBUG_PAGES Joruney */
          TRACE_MOUNT("Survey data saved, Count=%d, Page=%d \r\n", rmp.scountp, rmp.debugHistoryPage);
        }
        // Backup done, rotate through pages (1-MOUNT_SURVEY_DEBUG_PAGES)
        rmp.debugHistoryPage = (rmp.debugHistoryPage % MOUNT_SURVEY_DEBUG_PAGES) + 1;  
        break;

      case MOUNT_SURVEY_DATA_RESET:
        /* Reset the histogram survey data */
        FitmentSurveyDataReset();
        break;
      default:
        uint8_t page = mode_control - MOUNT_CONSOLE_CMD_SHIFT;
        if (page >= 0 && page <= MOUNT_SURVEY_DEBUG_PAGES)
        {
          success = ReadSurveyDataFromSpiRAM(page);   /* Console CMD, read saved page of the day, range 0-7 */
          if (success)
          {
            TRACE_MOUNT("Read histogram day %d data back \r\n", mode_control - MOUNT_CONSOLE_CMD_SHIFT);
          }
        }
        else
        {
          TRACE_MOUNT("Invalid page number (out of range).\r\n");
          success = false;
        }
        break;
    }
   return (success);
}


/**
 * @brief     Processes the mount check and survey histogram update
 * @param     ImuData  : pointer to live IMU data
 * @param     pitch    : live pitch angle
 * @param     roll     : live roll angle
 * @return    none
 */
void FitmentMountSurveyUpdate(const ImuData_t *ImuData, int pitch, int roll)
{
  if (IsMountCheckActive())
  {
    /* Evaluate the device's orientation using the Z-axis for face up/down determination */
    EvaluateZAxisOrientation(ImuData->accel.Z);

    /* Collect the live pitch and roll to histogram */
    FitmentHistogramCollect(pitch, roll);  

    /* Check the Mount Status with the live pitch and roll */
    FitmentCheckMountStatusAgainstConfig(pitch, roll);
    
    /* Acknowledge the active check and reset the flag for the next check */
    rmp.PC_FLAG &= (~RMP_MOUNT_CHECK);  /* only do it once every wake up before sleep again */
  }
}

/**
 * @brief     Applies the rotation matrix or performs a debug stream test
 * @param     ImuData  : pointer to IMU data
 * @param     pitch    : pitch angle
 * @param     roll     : roll angle
 * @return    none
 */
void FitmentApplyRotationToIMU(ImuData_t *ImuData, int pitch, int roll)
{
  if (rmp.PC_FLAG & RMP_NORMALISATION_ON)  
  {
    /* apply current matrix rotation */
    XyzApplyMatrix (ImuData->accel.X, ImuData->accel.Y, ImuData->accel.Z, pitch, roll);
    ImuData->accel.X = rmp.RX; /* update the rotated values back to Accel output */
    ImuData->accel.Y = rmp.RY;
    ImuData->accel.Z = rmp.RZ;
  }
  else /* matrix test process, display only */
  { /* stream test for debug displays */
    if (streamTest)
    {
      static int16_t displayCnt = 0;
      if (displayCnt%10 == 0)  /* test speed control for stream only */
      {
        XyzApplyMatrix (ImuData->accel.X, ImuData->accel.Y, ImuData->accel.Z, pitch, roll); /* apply matrix rotation */
        ConsolePrintf ("--> XYZ= [%4d %4d %4d]->[%4d %4d %4d] [%d %d]->[%d %d]\r\n", ImuData->accel.X, ImuData->accel.Y, ImuData->accel.Z, (int16_t)rmp.RX, (int16_t)rmp.RY,(int16_t)rmp.RZ,
                      pitch, roll, rmp.RP, rmp.RR);
      }
    }
  }
}

/**
 * @brief    Enable or disble the normalization
 *           The Fitment Matrix will be re-init 
 * @param    enabled  : true if turn on Normalization, otherwise, disable it
 * @return   none
 */
void FitmentNormalisationEnable (bool enabled)
{
  if (enabled)
  {
    ReadSurveyDataFromSpiRAM (MOUNT_SURVEY_CURRENT_PAGE);  /* restore saved data */
    rmp.PC_FLAG |= RMP_NORMALISATION_ON;
  }
  else
  {
    rmp.PC_FLAG &= (~RMP_NORMALISATION_ON);
    WriteSurveyDataToSpiRAM (MOUNT_SURVEY_CURRENT_PAGE); /* save current data */
  }
  /* Init the Fitment matrix */
  FitmentMatrixInit();
}



/**
 * @brief     set the mount check flag to activiate the fitment checking
 * @param     none
 * @return    none
 */
void FitmentActivateMountCheck (void)
{
  rmp.PC_FLAG |= RMP_MOUNT_CHECK;
}


/**
 * @brief  Return if Fitment Normalization is in progress
 * @param  None
 * @retval true if normalization is in progress, otherwise false
 */
bool IsFitmentNormalizationOn (void)
{
  return (rmp.PC_FLAG & RMP_NORMALISATION_ON);
}


/**
 * @brief  Return the last known mounted status to the windscreen
 * @param  None
 * @retval true if the device was mounted previously, otherwise false
 */
bool IsScreenMountedPreviously (void)
{
  return (rmp.PC_FLAG & RMP_LAST_MOUNTED_STATUS) != 0;
}


/**
 * @brief     Return if device is mounted to the windscreen
 * @param     None
 * @return    true if the device is currently mounted (RMP_ACTIVE_MOUNT_FLAG is set), false otherwise.
 */
bool IsScreenMounted(void)
{
    return (rmp.PC_FLAG & RMP_ACTIVE_MOUNT_FLAG) != 0;
}

/**
 * @brief  Return if Fitment Mount Checking is in progress
 * @param  None
 * @retval true if Mount Checking is progress, otherwise false
 */
bool IsMountCheckActive (void)
{
  return (rmp.PC_FLAG & RMP_MOUNT_CHECK);
}

/**
 * @brief     build the rotation matrix from the system mounting parameters
 * @param     none
 * @return    rotation sequence mode
 *
 */
uint8_t FitmentMatrixInit (void)
{
  double ct[3];
  double st[3];
  double eulerAngles[3];
  DRFactor = M_PI/180.0f;

  /* Set up the pitch and roll for the usage from config */
  GetMountAngleFromConfig(&(rmp.m_pitch), &(rmp.m_roll));

  if (!IsFitmentNormalizationOn() && rmp.isSurveyResultConfident)
  {  /* console update for test purpose, display suggestions if config param 9 need to be updated permanently */
    TRACE_MOUNT("Surveyed data config > set param 9 %u to update system mounting \r\n", 
                  CombinePitchRoll(rmp.surveyed_pitch, rmp.surveyed_roll));
  }

  eulerAngles[0] = rmp.m_roll * DRFactor;
  eulerAngles[1] = rmp.m_pitch * DRFactor;
  eulerAngles[2] = 0;                         /* default car travel direction */

  AxisSequence sequence = XYZ;    /* the routine of unit */
  /* calculate sin and cos of euler angles  */
  for (int i = 0; i < 3; i++)
  {
    ct[i] =  cos(eulerAngles[i]);   /* in radius */
#ifdef POSITIVE_PITCH_VALUES
    st[i] = sin(eulerAngles[i]);   /* value no longer negated when using positive pitch values */
#else
    st[i] = -sin(eulerAngles[i]);   /* force the sign as unit mounted face down */
#endif
  }

  switch (sequence)
  {
    case ZYX:
      RMatrix[0][0] = ct[1] * ct[0];
      RMatrix[0][1] = st[2] * st[1] * ct[0] - ct[2] * st[0];
      RMatrix[0][2] = ct[2] * st[1] * ct[0] + st[2] * st[0];
      RMatrix[1][0] = ct[1] * st[0];
      RMatrix[1][1] = st[2] * st[1] * st[0] + ct[2] * ct[0];
      RMatrix[1][2] = ct[2] * st[1] * st[0] - st[2] * ct[0];
      RMatrix[2][0] = -st[1];
      RMatrix[2][1] = st[2] * ct[1];
      RMatrix[2][2] = ct[2] * ct[1];
      break;
    case XYZ:
      RMatrix[0][0] = ct[1] * ct[2];
      RMatrix[0][1] = -ct[1] * st[2];
      RMatrix[0][2] = st[1];
      RMatrix[1][0] = ct[0] * st[2] + ct[2] * st[0] * st[1];
      RMatrix[1][1] = ct[0] * ct[2] - st[0] * st[1] * st[2];
      RMatrix[1][2] = -ct[1] * st[0];
      RMatrix[2][0] = st[0] * st[2] - ct[0] * ct[2] * st[1];
      RMatrix[2][1] = ct[2] * st[0] + ct[0] * st[1] * st[2];
      RMatrix[2][2] = ct[0] * ct[1];
      break;
    case ZYZ:
      RMatrix[0][0] = ct[0] * ct[2] * ct[1] - st[0] * st[2];
      RMatrix[0][1] = -ct[0] * ct[1] * st[2] - st[0] * ct[2];
      RMatrix[0][2] = ct[0] * st[1];
      RMatrix[1][0] = st[0] * ct[2] * ct[1] + ct[0] * st[2];
      RMatrix[1][1] = -st[0] * ct[1] * st[2] + ct[0] * ct[2];
      RMatrix[1][2] = st[0] * st[1];
      RMatrix[2][0] = -st[1] * ct[2];
      RMatrix[2][1] = st[1] * st[2];
      RMatrix[2][2] = ct[1];
      break;
    default:
      break;
  }

  return (sequence);
}



/**
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */


/**
 * @brief     Apply the rotation matrix to the data set and kept in the new set
 * @param     x,y,z  : Raw vector data
 * @param     pitch   : current pitch in degree from the sensor
 * @param     roll    : current roll in degree from the sensor
 * @return    none
 */
static void XyzApplyMatrix (int xi, int yi,int zi, int pitch, int roll)
{
  rmp.RX = RMatrix[0][0]* (double) xi + RMatrix[0][1]* (double) yi  + RMatrix[0][2]* (double) zi;
  rmp.RY = RMatrix[1][0]* (double) xi + RMatrix[1][1]* (double) yi  + RMatrix[1][2]* (double) zi;
  rmp.RZ = RMatrix[2][0]* (double) xi + RMatrix[2][1]* (double) yi  + RMatrix[2][2]* (double) zi;
  
  rmp.RX *= -1;                  /* Accel forward is positive,  ISO 8855 */
  rmp.RP  = rmp.m_pitch - pitch;  /* make pitch + with ISO 8855 */
  rmp.RR  = roll - rmp.m_roll;
}


/**
 * @brief     Check if Survey Collection condition met (speed above threshold, within duration)
 *            If the speed condition is satisfied, the surveyQualifiedSamples counter is incremented.
 * @param     surveyQualifiedSamples Pointer to the counter tracking the number of qualified samples.
 * @return    true if conditions are satisfied, false otherwise
 */
static bool IsCollectionConditionsMet(uint32_t* surveyQualifiedSamples)
{
  /* Get the Monunt Survey Setting */
  uint32_t mountSurveySetting = ConfigGetParam(CONFIG_PARAM_MNTSURVEY_SETTING);

  if (!ConfigGetFlag(CONFIG_FLAG_SURVEY_DATA_ENABLED))
  {
    return false;
  }
  /* Check if journey is in progress */
  if (!SequencerInJourney())
  {
    return false;
  }

  /* Check if the speed is above a threshold */
  if (debugIgnoreSpeed || ModemStatus.GnssData.Speed >= CONFIG_PARAM_GET_BYTE0(mountSurveySetting))
  {
    (*surveyQualifiedSamples)++;
    /* Check if we have enough qualifed samples for required duration (e.g., 10 minutes) */
    if (((*surveyQualifiedSamples) * FITMENT_SURVEY_INTERVAL) >= (CONFIG_PARAM_GET_BYTE1(mountSurveySetting) * 60))
    {
      TRACE_MOUNT("Survey Collection Stopped with # of samples: %d\r\n", *surveyQualifiedSamples);
      /* Survey Done, reset the survey starting flag */
      ConfigSetFlag(CONFIG_FLAG_SURVEY_DATA_ENABLED, false);

      /* Survey Complete, take the complete action and backup the data */
      FitmentSurveyCompleteAndBackup();
      return false;
    }
    return true;
  }
  return false;
}

/**
 * @brief     collect the pitch and roll of the device and put into the histogram.
 * @param     pitch   : current pitch values in degree
 * @param     roll    : current roll in degree
 * @return    none
 */
static void FitmentHistogramCollect (int16_t pitch, int16_t roll)
{
  /* Normalize the value to the bin index */
  int currentPitchIdx = pitch - PITCH_START;
  int currentRollIdx = roll - ROLL_START;

  /* Only update the pitch and roll within the bin range */
  if ((currentPitchIdx < 0) || (currentPitchIdx > (XYZ_MOUNT_SURVEY_POCKET-1)) 
      || (currentRollIdx < 0) || (currentRollIdx > (XYZ_MOUNT_SURVEY_POCKET-1))
  )
  { 
    TRACE_MOUNT("Pitch or Roll out of range %d %d (Ignored)\r\n", pitch, roll);
    return;
  }
  
  if ( IsCollectionConditionsMet(&rmp.surveyQualifiedSamples) ) /* only put survey data if conditions met */
  {
    TRACE_MOUNT("**Survey Collection in progress [%d]\r\n", rmp.surveyQualifiedSamples);
    if ((pitch >= PITCH_START) && (pitch <= (PITCH_START + XYZ_MOUNT_SURVEY_POCKET-1))) /* valid range 0 to 100 */
    {
      if (mSurveyPR[currentPitchIdx][PITCH_INDEX] < UINT16_MAX) 
      {
        mSurveyPR[currentPitchIdx][PITCH_INDEX]++;
        rmp.scountp++;
      }
    }
    if ((roll >= ROLL_START) && (roll <= (ROLL_START + XYZ_MOUNT_SURVEY_POCKET-1)))  /* valid roll -50, 50 */
    {
      if (mSurveyPR[currentRollIdx][ROLL_INDEX] < UINT16_MAX) {
        mSurveyPR[currentRollIdx][ROLL_INDEX]++;
        rmp.scountr++;
      }
    }
  }
  else
  {
    TRACE_MOUNT("Survey Collection conditions not met\r\n");
  }
}

/**
 * @brief     Check the Device Mount Status with the config defined
 * @param     pitch   : current pitch values in degree
 * @param     roll    : current roll in degree
 * @return    none
 */
static void FitmentCheckMountStatusAgainstConfig(int16_t pitch, int16_t roll)
{
  CheckMountStatusWithConfig(pitch, roll); 

  /* Screen is not currently mounted, but was mounted previously */
  if (!IsScreenMounted() && IsScreenMountedPreviously())
  {
    TRACE_MOUNT("Device Unmounted from Mount State, Write the unmount status [%d][%d], [%d][%d]\r\n", 
            rmp.m_pitch, pitch, rmp.m_roll, roll);
    MountSurveySendEventlog();
  }
  /* Unmount Event acknowledged, update the last known mount status */
  UpdateLastKnownMntStatus(IsScreenMounted());
}

/**
 * @brief     Calculate survey pitch and roll based on the provided histogram and method
 * @param     selected_pitch: pointer to store the selected pitch
 * @param     selected_roll: pointer to store the selected rollf
 * @return    true if the result is confident, false otherwise (Criteria depends on method)
 */
static bool CalculateAndEvaluateSurveyPitchRoll(int16_t *selected_pitch, int16_t *selected_roll)
{
  int16_t pitch_index = PITCH_INDEX;
  int16_t roll_index = ROLL_INDEX;

  int16_t peak_pitch = 0, peak_roll = 0;
  int32_t sum_pitch = 0, sum_roll = 0;
  uint16_t pcount = 0, rcount = 0;            /* The sample count for the Peak pitch and roll in the survey */
  uint16_t totalpCount = 0, totalrCount = 0;  /* The total count for the peak pitch and roll in the survey */

  bool confident = false;

  /* survey method (SURVEY_METHOD_PEAK or SURVEY_METHOD_AVERAGE) */
  SurveyMethod_t method = CONFIG_PARAM_GET_BYTE2(ConfigGetParam(CONFIG_PARAM_MNTSURVEY_SETTING));

  uint32_t mountParam = ConfigGetParam(CONFIG_PARAM_MOUNT_PARAM);

  /* Extract necessary parameters from mountParam */
  uint8_t allowedDeviation = CONFIG_PARAM_GET_BYTE0(mountParam);
  int16_t expectedPitch = ExtractSignedValue(CONFIG_PARAM_GET_BYTE3(mountParam));
  int16_t expectedRoll = ExtractSignedValue(CONFIG_PARAM_GET_BYTE2(mountParam));

  // Iterate over the survey data to calculate peak and sum for averages
  for (int16_t i = 0; i < XYZ_MOUNT_SURVEY_POCKET; i++)
  {
    int16_t pocket_pitch, pocket_roll;
    pocket_pitch = i + PITCH_START;
    pocket_roll = i + ROLL_START;
    
    // Find the peak
    if (mSurveyPR[i][pitch_index] > pcount)
    {
      pcount = mSurveyPR[i][pitch_index];
      peak_pitch = pocket_pitch;
    }
    if (mSurveyPR[i][roll_index] > rcount)
    {
      rcount = mSurveyPR[i][roll_index];
      peak_roll = pocket_roll;
    }
    // Accumulate for average calculation
    sum_pitch += pocket_pitch * mSurveyPR[i][pitch_index];
    sum_roll += pocket_roll * mSurveyPR[i][roll_index];

    totalpCount += mSurveyPR[i][pitch_index];
    totalrCount += mSurveyPR[i][roll_index];
  }

  /* Select pitch and roll based on the method, and return the conident level */
  if (method == SURVEY_METHOD_PEAK)
  {
    /* Calculate if the current histogram is confident to patch to the mount Angle*/
    float peakPitchPercentage = totalpCount? (float)(pcount *100.0) / (float) totalpCount:0;
    /* ToDo: Do we need to use the rollPercentage also? */

    /* Assign the selected pitch and roll based on the peak values */
    *selected_pitch = peak_pitch;
    *selected_roll = peak_roll;

    /* Check if the survey result is confident enough to use */
    confident = (peakPitchPercentage > MOUNT_CONFIDENCE_PERCENT);
  }
  else if (method == SURVEY_METHOD_AVERAGE)
  {
    /* Calculate average pitch and roll */
    int16_t avg_pitch = totalpCount? (int16_t)(sum_pitch / totalpCount):0;
    int16_t avg_roll = totalrCount? (int16_t)(sum_roll / totalrCount):0;

    *selected_pitch = avg_pitch;
    *selected_roll = avg_roll;

    /* when use average, will only check if the average is in range*/
    confident = true;
  }
     
  /* Check confidence: within range of expected values +/- allowedDeviation */
  /* If it is away from the allowed deviation, probably the device is already unmounted
      so the surveyed value is not a confident one */
  bool pitchInRange = abs(*selected_pitch  - expectedPitch) <= allowedDeviation;
  bool rollInRange = abs(*selected_roll - expectedRoll) <= allowedDeviation;

  confident &= (pitchInRange && rollInRange) && (pcount > MOUNT_CONFIDENCE_SAMPLE);
  if (!confident)
  {
    TRACE_MOUNT("InRange[%d %d] dev[%d] expected[%d %d] selected[%d %d]\r\n", 
                pitchInRange, rollInRange, allowedDeviation, expectedPitch, expectedRoll,
                *selected_pitch, *selected_roll);
  }

  return confident;
}


/**
 * @brief     Calculate the peak percentages for pitch and roll from the survey histogram
 * @param     peakPitchPercentage: pointer to store the peak pitch percentage
 * @param     peakRollPercentage: pointer to store the peak roll percentage
 * @return    none
 */
static void GetPeakPitchRollSurveyPercentage(uint8_t *peakPitchPercentage, uint8_t *peakRollPercentage)
{
  int16_t pitch_index = PITCH_INDEX;
  int16_t roll_index = ROLL_INDEX;
  uint16_t pcount = 0, rcount = 0;
  uint16_t totalpCount = 0, totalrCount = 0;

  // Iterate through the survey histogram to find the peak counts
  for (int16_t i = 0; i < XYZ_MOUNT_SURVEY_POCKET; i++)
  {
    if (mSurveyPR[i][pitch_index] > pcount)
    {
      pcount = mSurveyPR[i][pitch_index];
    }
    if (mSurveyPR[i][roll_index] > rcount)
    {
      rcount = mSurveyPR[i][roll_index];
    }
    totalpCount += mSurveyPR[i][pitch_index];
    totalrCount += mSurveyPR[i][roll_index];
  }

  // Calculate the percentage for the peak Pitch and Roll
  float p1 = totalpCount? ((float)(pcount *100.0) / (float) totalpCount):0;
  float p2 = totalrCount? ((float)(rcount *100.0) / (float) totalrCount):0;

  *peakPitchPercentage = (uint8_t) p1;
  *peakRollPercentage = (uint8_t) p2;
}


/**
 * @brief     Calculate the average pitch and roll from the survey histogram
 * @param     avgPitch: pointer to store the average pitch
 * @param     avgRoll : pointer to store the average rollfs
 * @return    none
 */
static void GetAvgPitchRollSurvey(int16_t *avgPitch, int16_t *avgRoll)
{
  int16_t pitch_index = PITCH_INDEX;
  int16_t roll_index = ROLL_INDEX;
  int32_t sum_pitch = 0, sum_roll = 0;
  uint16_t totalpCount = 0, totalrCount = 0;

  // Iterate through the survey histogram to find the average counts
  for (int16_t i = 0; i < XYZ_MOUNT_SURVEY_POCKET; i++)
  {
    int16_t pocket_pitch, pocket_roll;
    pocket_pitch = i + PITCH_START;
    pocket_roll = i + ROLL_START;

    // Accumulate for average calculation
    sum_pitch += pocket_pitch * mSurveyPR[i][pitch_index];
    sum_roll += pocket_roll * mSurveyPR[i][roll_index];

    totalpCount += mSurveyPR[i][pitch_index];
    totalrCount += mSurveyPR[i][roll_index];
  }

  /* Calculate average pitch and roll */
  *avgPitch = totalpCount? (int16_t)(sum_pitch / totalpCount):0;
  *avgRoll = totalrCount? (int16_t)(sum_roll / totalrCount):0;
}


/**
 * @brief     Update the survey results for pitch and roll based from the journey histogram
 * @param     none
 * @return    none
 */
static void UpdateSurveyResultsWithJourneyData (void)
{
  int16_t selected_pitch = 0, selected_roll = 0;

  /* Calculate the survey pitch and roll using InJourney Survey */
  /* The Confident level will be used for the Mouting Angle set */
  rmp.isSurveyResultConfident = CalculateAndEvaluateSurveyPitchRoll(&selected_pitch, &selected_roll);

  // Store the selected survey pitch and roll
  rmp.surveyed_pitch = selected_pitch;
  rmp.surveyed_roll = selected_roll;
}


/**
 * @brief     Display the survey data histogram for pitch or roll
 * @param     histogramStart: starting value for the histogram range
 * @param     totalSamples: total sample count for percentage calculation
 * @param     axisIndex: index of the axis histogram (e.g., PITCH_INDEX, ROLL_INDEX, etc.)
 * @param     axisLabel: character label for visualization ('P' for pitch, 'R' for roll)
 * @return    none
 */
static void DisplayHistogram(int16_t histogram_start, uint16_t count, int16_t index, char label)
{
  uint8_t bar[XYZ_MOUNT_SURVEY_POCKET];
  for (int16_t bucketIndex  = 0; bucketIndex  < XYZ_MOUNT_SURVEY_POCKET; bucketIndex ++)
  {
    int16_t histogramValue = histogram_start + bucketIndex;
    uint16_t bucketCount = (mSurveyPR[bucketIndex ][index]) & 0xffff;
    float percentage = (float)(bucketCount  * 100.0) / (float)count;
    memset(bar, '\0', XYZ_MOUNT_SURVEY_POCKET);
    memset(bar, label, (int)percentage);

    /* Only display if there is count*/
    if (bucketCount)
    {
      ConsolePrintf("(%03d) %s>(%u)%d%%\r\n", histogramValue, bar, bucketCount, (int)percentage);
    }
    sleep(1);
  }
}


/**
 * @brief     List the Journey pitch and roll survey data histogram
 * @param     none
 * @return    none
 */
static void DisplayInJourneyHistogram (void)
{
  ConsolePrintf("\r\n");
  DisplayHistogram(PITCH_START, rmp.scountp, PITCH_INDEX, 'P');
  ConsolePrintf("\r\n\r\n");
  DisplayHistogram(ROLL_START, rmp.scountr, ROLL_INDEX, 'R');
  ConsolePrintf("\r\n\r\nJourney pitch roll count: [%d, %d]\r\n\r\n", rmp.scountp, rmp.scountr);
}


#ifdef DEBUG  /* Marked with build with DEBUG as it will affect IMU Data, function are for debug use only */
/**
 * @brief     Set/reset the flag, to update the matrix angle to current measured ones
 * @param     nflag      :set the flag up if nflag = 1
 * @return    none
 */
static void RmpXYZSetCurrentEnable (uint8_t nflag)
{
  if (nflag >= 1)
  {
    rmp.PC_FLAG |= RMP_DEBUG_MOUNT_SET;
  }
  else if (nflag == 0)
  {
    rmp.PC_FLAG &= (~RMP_DEBUG_MOUNT_SET);
  }
}
#endif


/**
 * @brief     Write a survey histogram data to SPI RAM
 * @param     npage   : History page to be saved up to MOUNT_SURVEY_DEBUG_PAGES, 0 for current used data set
 * @return    true if successful, false otherwise
 */
static bool WriteSurveyDataToSpiRAM (uint8_t npage)
{
  bool success = false;

  if ((npage >= 0) && (npage <= MOUNT_SURVEY_DEBUG_PAGES))
  {
    uint32_t startAddress = SERIAL_RAM_MOUNT_SURVEY_AREA_START + SERIAL_RAM_MOUNT_SURVEY_PAGE_SIZE * npage;
    /* Write histogram and control data block  */
    int nl = sizeof (mSurveyPR);
    if (SerialRamWrite(startAddress, (uint8_t*)mSurveyPR, nl)) /* histogram */
    {
      #ifdef TRACE_MOUNT_ENABLE
        uint8_t pitchPercentage, rollPercentage;
        GetPeakPitchRollSurveyPercentage(&pitchPercentage, &rollPercentage);
        ConsolePrintf("->Write histogram data (page %d) P[%d][%d%%]  R[%d][%d%%]\r\n", npage, rmp.surveyed_pitch, pitchPercentage, rmp.surveyed_roll, rollPercentage);
      #endif
      success = SerialRamWrite(startAddress + nl, (uint8_t*)&rmp, sizeof(RotationParameter_t)); /*control structure */
    }
    if (!success)
    {
      ConsolePrintf("Failed backup survey histogram data to SPI RAM\r\n");
    }
  }
  return success;
}

/**
 * @brief     Read mount survey histogram data from SPI RAM page
 * @param     npage    : MOUNT_SURVEY_CURRENT_PAGE for current survey data page, up to MOUNT_SURVEY_DEBUG_PAGES
 *
 * @return    True     : if successful, false otherwise
 */
static bool ReadSurveyDataFromSpiRAM (uint8_t npage)
{
  bool success = false;

  if ((npage >= MOUNT_SURVEY_CURRENT_PAGE) && (npage <= MOUNT_SURVEY_DEBUG_PAGES))
  {
    uint32_t startAddress = SERIAL_RAM_MOUNT_SURVEY_AREA_START+ SERIAL_RAM_MOUNT_SURVEY_PAGE_SIZE * npage;
    /* Read the histogram data from the npage provided */
    int nl = sizeof (mSurveyPR);
    if (SerialRamRead(startAddress, (uint8_t*)mSurveyPR, nl))
    {
      /* Restore the rmp parameters */
      int mm = sizeof(RotationParameter_t);
      success = SerialRamRead(startAddress + nl, (uint8_t*)&rmp, mm);
      TRACE_MOUNT("Read survey histogram data (page %d:%d bytes) from SPI RAM \r\n",npage, nl+mm);
      if (!success)
      {
        TRACE_MOUNT("-->Data for page %d is invalid and reset the \r\n", npage);
        FitmentSurveyDataReset();
        success = false;
      }
    }
    else
    {
      ConsolePrintf("Failed to read survey histogram data from SPI RAM \r\n");
    }
  }
  return success;
}

/**
 * @brief     Extracts a signed 16-bit value from an unsigned 8-bit input.
 *            The function interprets the most significant bit (MSB) as the sign bit,
 *            converts the value to a signed integer
 * @param     byte The unsigned 8-bit value to be converted.
 * @return    The signed 16-bit value derived from the input byte.
 */
static int16_t ExtractSignedValue(uint8_t byte)
{
  // Check if the most significant bit (sign bit) is set
  if (byte & 0x80)
  {
    return (int16_t)((byte & 0x7F) * -1);
  }
  return (int16_t)byte;
}


/**
 * @brief     Get mounting angle from Config parameter CONFIG_PARAM_MOUNT_ANGLES
 *              restore the two angles to one uint32_t type
 * @param     pitch [out] Pointer to store the extracted pitch angle.
 * @param     roll  [out] Pointer to store the extracted roll angle.
 * @return    none
 */
static void GetMountAngleFromConfig (int16_t *pitch, int16_t *roll)
{
  if (!pitch || !roll)
  {
    /* Sanity checking */
    return;
  }
    
  uint32_t mountParameter = ConfigGetParam(CONFIG_PARAM_MOUNT_ANGLES);
  uint8_t pitchByte = (mountParameter & 0xFF00) >> 8; // Extract upper byte (pitch)
  uint8_t rollByte  = (mountParameter & 0xFF);        // Extract lower byte (roll)

  *pitch = ExtractSignedValue(pitchByte);
  *roll = ExtractSignedValue(rollByte);
}


/**
 * @brief    Update mounting angle of config parameter CONFIG_PARAM_MOUNT_ANGLES
 *              combine the two angles to one uint32_t type
* @param     pitch        The pitch angle to be stored.
 * @param    roll         The roll angle to be stored.
 * @return   none
 */
static void SetMountAngleToConfig(int16_t pitch, int16_t roll)
{
  uint32_t currentParameter = 0;
  uint32_t newParameter = CombinePitchRoll(pitch, roll);

  /* Get the current configuration parameter */
  currentParameter = ConfigGetParam(CONFIG_PARAM_MOUNT_ANGLES);

  if (currentParameter != newParameter) 
  {
    ConfigSetParam(CONFIG_PARAM_MOUNT_ANGLES, newParameter);
    ConsolePrintf("Config param 9 has been updated to new value %u \r\n", newParameter);
  } 
  else 
  {
    TRACE_MOUNT("Config param 9 is unchanged (%u). No update required.\r\n", currentParameter);
  }
}


/**
 * @brief     Help function to combine the pitch and roll into one uint16 value
 *
 * @param     pitch    : in degree
 * @param     roll     : in degree
 * @return    uint16_t, combine value;
 */
static uint16_t CombinePitchRoll (int16_t pitch, int16_t roll)
{
  uint16_t mountPR = 0;
  uint8_t  mountP = abs (pitch);
  uint8_t  mountR = abs (roll);
  if (pitch < 0)
  {
    mountPR = (mountP | 0x80) << 8; /* add minus sign bit */
  }
  else
  {
    mountPR = (mountP) << 8;
  }

  if (roll < 0)
  {
    mountPR |= (mountR | 0x80);  /* add minus sign bit */
  }
  else
  {
    mountPR |= (mountR);
  }
  return (mountPR);
}


/**
 * @brief     Mounting survey data reset
 * @param     none
 * @return    none
 */
static void FitmentSurveyDataReset (void)
{
  uint8_t i;
  for (i = 0; i<XYZ_MOUNT_SURVEY_POCKET; i++)
  {
    mSurveyPR[i][PITCH_INDEX]            = 0;
    mSurveyPR[i][ROLL_INDEX]             = 0;
  }
  
  rmp.scountp           = 0;  
  rmp.scountr           = 0;
  rmp.surveyed_pitch    = 0;
  rmp.surveyed_roll     = 0;
  rmp.surveyQualifiedSamples       = 0;
  rmp.lastMntEventReportTStamp     = RtcGetTimeStamp ();
}


/**
 * @brief     Flushes the mount survey data to PSRAM and configs
 *            for the update of Mount Angle 
 *            This function is intended to be called when the whole survey is completed ONLY
 * @param     none
 * @return    none
 */
static void FitmentSurveyCompleteAndBackup (void)
{
  /* backup current histogram data for debug use*/
  FitmentDataHandler(MOUNT_SURVEY_COMPLETE);

  /* Mounting conditional update according survey */
  UpdateMountAngleFrmSurvey();

  /* Reset the Histogram Data for the next survey*/
  FitmentDataHandler(MOUNT_SURVEY_DATA_RESET);
}


/**
 * @brief     Check Survey mounting data, whether update is needed
 * @param     mone
 * @return    none
 */
static void UpdateMountAngleFrmSurvey (void)
{
  bool  isDeltaWithinUpdateRange = false;
  uint16_t   delta = 0;
  uint32_t mountParam = ConfigGetParam(CONFIG_PARAM_MOUNT_PARAM);

  /* Default value with MOUNT_SURVEY_DATA_CAP if cloud cap is not set */
  uint8_t updateCap = CONFIG_PARAM_GET_BYTE1(mountParam) ? CONFIG_PARAM_GET_BYTE1(mountParam): MOUNT_SURVEY_DATA_CAP;


  /* Mount survey result looks good, check if it is within the update range */
  if (rmp.isSurveyResultConfident)
  {
    /* Check if the pitch update is within range */
    delta = abs(rmp.m_pitch - rmp.surveyed_pitch);
    if (delta <= updateCap)
    {
      rmp.m_pitch = rmp.surveyed_pitch;
      isDeltaWithinUpdateRange = true;
    }
    /* Check if the roll update is within range */
    delta = abs(rmp.m_roll - rmp.surveyed_roll);
    if (delta <= updateCap)
    {
      rmp.m_roll = rmp.surveyed_roll;
      isDeltaWithinUpdateRange = true;
    }
  }

  /*  Survey result good, pitch or roll was within update Range */
  if (isDeltaWithinUpdateRange && rmp.isSurveyResultConfident)
  {
    TRACE_MOUNT("Survey Result looks good, save Mount Angle [%d %d]\r\n", rmp.m_pitch, rmp.m_roll);
    /* Only update to config if Delta is in range, and the survyed result is confident enough */
    SetMountAngleToConfig(rmp.m_pitch, rmp.m_roll);

    /* Init the Fitment matrix */
    FitmentMatrixInit();
  }
  else
  {
    TRACE_MOUNT("Survey Not good enough: confident[%d] current [%d %d] vs surveyed [%d %d], DeltaCap[%d]\r\n", 
          rmp.isSurveyResultConfident, rmp.m_pitch, rmp.m_roll, rmp.surveyed_pitch, rmp.surveyed_roll, updateCap);
  }
  /* log update to event 71, with the current mount pitch and roll that in used */
  StatusLogWriteEventCode(STATUS_CONFIG_MOUNT_STATE, CombinePitchRoll(rmp.m_pitch, rmp.m_roll));
}


/**
 * @brief     Send event log when the fitment is changed
 * @param     none
 * @return    none
 */
static void MountSurveySendEventlog (void)
{
  if ((RtcGetTimeStamp () - rmp.lastMntEventReportTStamp ) > MOUNT_OFF_REPORT_INTERVAL)
  {
    rmp.lastMntEventReportTStamp = RtcGetTimeStamp ();
    StatusLogWriteEventCode(STATUS_CONFIG_OFF_MOUNT, STATUS_DATA_LOAD_NONE);
  }
}


/**
 * @brief     Check with the live pitch and roll to tell if device mounted or unmounted
 *            Also determine if the device is face up or face down
 * @param     none
 * @return    none
 */
static void CheckMountStatusWithConfig (int16_t pitch, int16_t roll)
{
  /* Chech the pitch and roll aganist the config 9 settings */
  uint32_t mountParam = ConfigGetParam(CONFIG_PARAM_MOUNT_PARAM);
  uint8_t allowedDeviation = CONFIG_PARAM_GET_BYTE0(mountParam);

  /*  reset the current mount flags and evalute them
      keep RMP_LAST_MOUNTED_STATUS for unmount event generating */
  rmp.PC_FLAG &= (~(RMP_ACTIVE_MOUNT_FLAG|RMP_MOUNT_FACEUP|RMP_MOUNT_FACEDOWN));
  if (allowedDeviation == 0)
  {
    /* Always mounted if no deviation defined */
    rmp.PC_FLAG |= RMP_ACTIVE_MOUNT_FLAG;
  }
  else if ((abs(pitch -rmp.m_pitch) < allowedDeviation) && (abs(roll - rmp.m_roll) < allowedDeviation))
  {
    /* Pitch and roll in range, set device mounted*/
    rmp.PC_FLAG |= RMP_ACTIVE_MOUNT_FLAG;
  }

  /*Evaluate if device faceup or face down vs shelf. Use FACE_UP_DOWN_RANGE if deviations is not defined*/
  allowedDeviation = allowedDeviation? allowedDeviation:FACE_UP_DOWN_RANGE;

  if ((abs(pitch - SHELF_PITCH ) < allowedDeviation) && (abs(roll - SHELF_ROLL) < allowedDeviation))
  {
    if (rmp.PC_FLAG & RMP_POSITIVE_Z)     /* using the Z flag here for direction */
    {
      rmp.PC_FLAG |= RMP_MOUNT_FACEUP;
    }
    else
    {
      rmp.PC_FLAG |= RMP_MOUNT_FACEDOWN;
    }
  }
}


/**
 * @brief     Updates the device's Z-axis flag to indicate orientation.
 *            The flag is used to determine whether the device is facing up or down 
 *            based on the Z-axis value and its angle deviation from predefined 
 *            thresholds (SHELF_PITCH and SHELF_ROLL).
 * 
 * @param     Zvalue  The Z-axis component value (int16_t) to evaluate.
 * @return    None
 */
static void EvaluateZAxisOrientation (int16_t Zvalue)
{
  if (Zvalue > 0)
  {
    rmp.PC_FLAG |= RMP_POSITIVE_Z;
  }
  else
  {
    rmp.PC_FLAG &= (~RMP_POSITIVE_Z);
  }

}

/**
 * @brief     Update previous mounted flag before renew the current mounted falg.
 *            it set 10s window to keep the mounted flag status til next update.
 *
 *            This is used to stop the accidental impact call when the unit is
 *            not mounted on screen by the parameter 13 settings..
 *            This delayed window also helps an impact to have time make the calls
 *            when it is correctly mounted as the impact could affect the mount
 *            flag status before the call is made if there is no delay.
 *
 * @param     none
 * @return    none
 */
static void UpdateLastKnownMntStatus(bool currentMountStatus)
{
  if (currentMountStatus == true)
  {
    rmp.PC_FLAG |= RMP_LAST_MOUNTED_STATUS;
  }
  else
  {
    rmp.PC_FLAG &= (~RMP_LAST_MOUNTED_STATUS);
  }
}


/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       command line entries for get the Rmp related Info
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int FitmentConsoleRmpConfig (int argCount, char **pArguments)
{
  /* check for the correct number of arguments */
  if (1 == argCount)
  {
    if (!strcmp(pArguments[0], "state") || !strcmp(pArguments[0], "jdata"))
    {
      uint8_t pitchPercentage = 0, rollPercentage = 0;
      int16_t avgPitch = 0, avgRoll = 0;

      uint32_t mountParam = ConfigGetParam(CONFIG_PARAM_MOUNT_PARAM);
      uint32_t collectionParam = ConfigGetParam(CONFIG_PARAM_MNTSURVEY_SETTING);

      /* update survey data before display*/
      UpdateSurveyResultsWithJourneyData ();  
      ConsolePrintf("Screen is mounted? %d\r\n", IsScreenMounted());
      ConsolePrintf("Config Mount Angle: [%d][%d]\r\n", rmp.m_pitch, rmp.m_roll);
      ConsolePrintf("Surveyed Angle: [%d][%d], Method:[%s], Confident[%s], Samples Count [%d]\r\n",
            rmp.surveyed_pitch, rmp.surveyed_roll, 
            CONFIG_PARAM_GET_BYTE2(collectionParam) == SURVEY_METHOD_PEAK? "peak":"average",
            rmp.isSurveyResultConfident? "True":"False",
            rmp.surveyQualifiedSamples);
      ConsolePrintf("Mount Param: Dev[%d] upCap[%d] Ref[%d %d]\r\n",
                    CONFIG_PARAM_GET_BYTE0(mountParam),
                    CONFIG_PARAM_GET_BYTE1(mountParam),
                    ExtractSignedValue(CONFIG_PARAM_GET_BYTE3(mountParam)),
                    ExtractSignedValue(CONFIG_PARAM_GET_BYTE2(mountParam)));
      ConsolePrintf("Collect Param: Speed[%d] duration[%d] method[%d]\r\n",
                    CONFIG_PARAM_GET_BYTE0(collectionParam),
                    CONFIG_PARAM_GET_BYTE1(collectionParam),
                    CONFIG_PARAM_GET_BYTE2(collectionParam));

      /* InJourney Related data*/
      GetPeakPitchRollSurveyPercentage(&pitchPercentage, &rollPercentage);
      GetAvgPitchRollSurvey(&avgPitch, &avgRoll);
      ConsolePrintf(" Current survey value[InJourney]: Peak:[%d%%, %d%%] - Avg:[%d, %d]\r\n",
              pitchPercentage, rollPercentage, avgPitch, avgRoll);
      ConsolePrintf(" InJourney pitch roll count: [%d, %d]\r\n\r\n", rmp.scountp, rmp.scountr);

      if (!strcmp(pArguments[0], "jdata"))
      {
        DisplayInJourneyHistogram();      /* list InJourney histogram */
      }
    }
    else if (!strcmp(pArguments[0], "complete"))
    {
      FitmentSurveyCompleteAndBackup();
    }    
    else if (!strcmp(pArguments[0], "backup"))
    {
      FitmentDataHandler(MOUNT_SURVEY_BACKUP);
    }
    else if (!strcmp(pArguments[0], "import"))
    {
      if(!FitmentDataHandler(MOUNT_SURVEY_IMPORT))
      {
        FitmentDataHandler(MOUNT_SURVEY_DATA_RESET);  /* reset if read back is invalid */
      }
    }
    else if (!strcmp(pArguments[0], "reset"))
    {
      FitmentDataHandler(MOUNT_SURVEY_DATA_RESET);
    }
#ifdef DEBUG
    else if (!strcmp(pArguments[0], "set"))
    {
      RmpXYZSetCurrentEnable(1);       /* update the matrix angle to current measured one */
    }
#endif
    else if (!strcmp(pArguments[0], "xyzon"))
    {
      rmp.PC_FLAG |= RMP_NORMALISATION_ON;;       /* matrix apply enabled to system xyz values  */
    }
    else if (!strcmp(pArguments[0], "xyzoff"))
    {
      rmp.PC_FLAG &= (~RMP_NORMALISATION_ON);      /* matrix apply disabled to system xyz values  */
    }
    else
    {
      ConsolePrintf ("Get/Control current mount state.  Usage: rmp <state/jdata/complete/backup/import/reset/refresh/xyzon/xyzoff ...>\r\n");
      return(CONSOLE_CMD_UNKNOWN);
    }
    return(CONSOLE_ERROR_OK);
  }
  if (2 == argCount)
  {
    if (!strcmp(pArguments[0], "collect"))
    {
      if (!strcmp(pArguments[1], "on"))
      {
        debugIgnoreSpeed = true; /* Set to true will ignore the speed check. Debug use only*/
      }
      else if (!strcmp(pArguments[1], "off"))
      {
        debugIgnoreSpeed = false;
      }
      else
      {
        ConsolePrintf("debugIgnoreSpeed set as %d\r\n", debugIgnoreSpeed);
        return(CONSOLE_ARG_RANGE);
      }
    }
    else if (!strcmp(pArguments[0], "debug"))
    {
      if (!strcmp(pArguments[1], "on"))
      {
        streamTest = true; /* Set to true will ignore the speed check. Debug use only*/
      }
      else if (!strcmp(pArguments[1], "off"))
      {
        streamTest = false;
      }
      else
      {
        ConsolePrintf("streamTest set as %d\r\n", streamTest);
        return(CONSOLE_ARG_RANGE);
      }
    }
    else if (!strcmp(pArguments[0], "method"))
    {
      int method;
      sscanf(pArguments[1],  "%d", &method);
      if ((method == SURVEY_METHOD_PEAK) || method == SURVEY_METHOD_AVERAGE)
      {
        uint32_t mountPara = ConfigGetParam(CONFIG_PARAM_MNTSURVEY_SETTING);
        mountPara = (mountPara & 0xFF00FFFF) | (method << 16);  
        ConfigSetParam(CONFIG_PARAM_MNTSURVEY_SETTING, mountPara);
        ConsolePrintf("Method updated as %s\r\n", (method == SURVEY_METHOD_PEAK)? "peak":"average");
      }
      else
      {
        return(CONSOLE_ARG_RANGE);
      }
    }
    else if (!strcmp(pArguments[0], "speed"))
    {
      int speed;
      sscanf(pArguments[1],  "%d", &speed);
      uint32_t mountPara = ConfigGetParam(CONFIG_PARAM_MNTSURVEY_SETTING);
      mountPara = (mountPara & 0xFFFFFF00) | speed;  
      ConfigSetParam(CONFIG_PARAM_MNTSURVEY_SETTING, mountPara);
      ConsolePrintf("Survey Speed Limit set to %d\r\n", CONFIG_PARAM_GET_BYTE0(ConfigGetParam(CONFIG_PARAM_MNTSURVEY_SETTING)));
    }
    else if (!strcmp(pArguments[0], "duration"))
    {
      int duration;
      sscanf(pArguments[1],  "%d", &duration);
      uint32_t mountPara = ConfigGetParam(CONFIG_PARAM_MNTSURVEY_SETTING);
      mountPara = (mountPara & 0xFFFF00FF) | (duration << 8);  
      ConfigSetParam(CONFIG_PARAM_MNTSURVEY_SETTING, mountPara);
      ConsolePrintf("Survey Collection Duration set to %d\r\n", CONFIG_PARAM_GET_BYTE1(ConfigGetParam(CONFIG_PARAM_MNTSURVEY_SETTING)));
    }
    else
    {
      ConsolePrintf ("Config Mounting related param.  Usage: rmp <collect(on/off)/method/speed/duration>\r\n");
      return(CONSOLE_CMD_UNKNOWN);
    }

    return(CONSOLE_ERROR_OK);
   }
  return(CONSOLE_ARG_NO);
}
