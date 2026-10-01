/**
 **************************************************************************************************
 * @file   motionmonitor.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   29/10/2019
 *
 * @brief  Motion Monitor Thread main file.
 *
 **************************************************************************************************
 */
#include "global.h"


/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */

/* Timeout value for sending impact Event */
#define MM_IMPACT_WAIT_TIMEOUT_TICKS  ((1*3600*1000)/MM_TIMER_PERIOD_MS)  // 1 hour in MM timer ticks

/*
 * motion monitor timing values
 */
/* Time to stay awake after power up */
#define MM_WAKEUP_POWERON_TIMEOUT     ((15 * 1000) / MM_TIMER_PERIOD_MS)  // 15 seconds in MM timer ticks
/* Time to stay awake waiting for SOJ after an IMU wakeup */
#define MM_WAKEUP_IMU_TIMEOUT         ((30 * 1000) / MM_TIMER_PERIOD_MS)  // 30 seconds in MM timer ticks
/* Minimum time to stay awake after RTC wake-up */
#define MM_WAKEUP_RTC_TIMEOUT         (( 200     ) / MM_TIMER_PERIOD_MS)  //  200 miliseconds in MM timer ticks
/* Minimum time to stay awake after USB wake-up */
#define MM_WAKEUP_USB_TIMEOUT         (( 2 * 1000) / MM_TIMER_PERIOD_MS)  //  2 seconds in MM timer ticks
/* Minimum time to stay awake after an event is detected, to allow time for sequencer to start running */
#define MM_WAKEUP_EVENT_TIMEOUT       (( 5 * 1000) / MM_TIMER_PERIOD_MS)  //  5 seconds in MM timer ticks
/* Time to wait for response from GM that it has sent/stored the EOJ */
#define MM_EOJ_FINISHED_TIMEOUT       ((60 * 1000) / MM_TIMER_PERIOD_MS)  // 60 seconds in MM timer ticks
/* Time to sleep between DHMs */
#define MM_RTC_SLEEP_IN_SECONDS       (20)                                // 20 seconds

#define MM_SURVEY_INTERVAL_DELAY      ((FITMENT_SURVEY_INTERVAL * 1000) / MM_TIMER_PERIOD_MS)  /* 10 seconds */

#define MM_HEARTBEAT_PERIOD_SECONDS   (4 * 60 * 60)                       // 4 hours in seconds
//#define MM_HEARTBEAT_PERIOD_SECONDS   (8 * 60)                          // !!!!! ACCELERATED TESTING SCHEDULE !!!!

/* Time to stay awake after the last user input at the console */
#define MM_CONSOLE_STAY_AWAKE_SECONDS 20

#define MM_XYZ_VALUE_OFFSET2000       2000              /* convert x,y int to uint */
#define MM_XYZ_MOVE_THRESHOLD         100               /* helper to detect sleep mode movement */
#define MM_SECOND_IN_HOUR             (3600)

/* Limit the number of samples to be read in one go. This is a slightly
 * arbitrary number, as we should never get a significant backlog
 * building up in the FIFO. But if we did, and we read a very large amount of
 * data all at once, our event detection window might not be long enough to
 * detect significant events in the data just fetched. It's a multiple of 6.
 */
#define MAX_FIFO_READ_SAMPLES         240

#define DC_FILTER_LENGTH              10

typedef enum MM_MessageId
{
  IMPACT_DATA_FINISHED = 0,
  EOJ_FINISHED,
  DHM_FINISHED
} eMMessageId;

typedef struct MM_Message
{
  uint8_t msgId;
} mmMessage_t;

/* Define the queue parameters. */
#define MM_QUEUE_LENGTH 3
#define MM_QUEUE_ITEM_SIZE sizeof( mmMessage_t )


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
/* store for latest IMU samples, which are used for event detection */
ImuData_t DetectBuffer[MM_DETECT_BUFFER_ELEMENTS] = { 0 };

static TimerHandle_t mmTimer;
static SemaphoreHandle_t timerSemaphore;

//static AccelerationData_t accelDataOffset = {0};
static eMmState motionMonitorState;
static uint32_t WakeupCount                = 0;
static uint32_t MountTmerCount             = 0;
static bool     sojDetected                = false;
static bool     eojDetected                = false;
static bool     isSojEverStarted           = false;           /* Flag used to determine cause of IMU wake up (journey / false wake up)*/
static bool     impactDataFinishedReceived = false;
static bool     dhmFinishedReceived        = false;           //TOOD - do we need these anymore?  Looks like sequencer busy check is enough
static bool     dstmFinishedReceived       = false;
static bool     eojFinishedReceived        = false;
static bool     hibernateRequested         = false;
static bool     spoofStartOfJourney        = false;
static bool     spoofEndOfJourney          = false;
static bool     spoofImpact                = false;
static uint8_t  FakeMiniEvent              = 0;                /* console test - sample count to inject mini-event data */
static bool     batteryTooLowForJournery   = false;
static bool     streamXYZ                  = false;
/* HP filter defines */
static ImuData_t         Filter[DC_FILTER_LENGTH];
static int               FilterIndex       = 0;


/* temporary storage of RawData used in factory test */
static ImuData_t RawData;

#ifdef DEBUG
  static bool sleepAllowed = true;
#else
  static bool sleepAllowed = true;
#endif

/* Last Heartbeat time - define here to allow debugging */
static uint32_t LastDhmTime = 0;


/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static void     MmTimerCallback             (xTimerHandle timer);
static void     DetectEvents                (uint16_t noOfSamples);
static bool     MotionMonitorHandleDHM      (void);
static void     StateTransitionCheck        (void);
static void     GoToSleep                   (void);
static eMmState MotionMonitorGetWakeupCause (void);
static void     MotionMonitorHpFilter       (ImuData_t *pBuffer, uint16_t noOfElements);
static bool     ReadyToSleep                (bool printDetails);
static void     SaveValuesBeforeSleep       (void);
static eMmState GetStandByWakeupCause       (void);
static void     MountSurveyFlagCheck        (void);
static void     MotionMonitorProcessDHMAndInstantEvents   (void);

/*
 **************************************************************************************************
 *                                   PUBLIC SECTION / INTERFACE
 **************************************************************************************************
 */
/**
 * @brief     The Motion Monitor Task function
 * @param     (not used)
 * @return    None
 */
void MotionMonitorTask (void const *argument)
{
  spoofImpact = false;
  spoofStartOfJourney = false;
  spoofEndOfJourney = false;

  motionMonitorState = WAKEUP_POWERUP;
  WakeupCount = MM_WAKEUP_POWERON_TIMEOUT;

  timerSemaphore = xSemaphoreCreateBinary();

  MotionBufferResetBuffer();  /*reset IMU buffer */

  mmTimer = xTimerCreate("MotionMonitorTimer", MM_TIMER_PERIOD_MS, pdTRUE, (void*) NULL, MmTimerCallback);
  if (mmTimer == NULL)
  {
    TRACE_IMU("MM Timer not created\n\r");
  }

  /* Initialise Accelerometer */
  spichipselect_t ChipSelect;
  ChipSelect.Port = ACCEL_CS_PORT;
  ChipSelect.Pin = ACCEL_CS_PIN;
  AccelDriverInit(ACCEL_SPI, ChipSelect);

  /* Carry out Wake-Up Self-Test if required */
  PostWakeUpSelfTest(IMU);

  xTimerStart(mmTimer, portMAX_DELAY);

  /* restore standby sleep states */
  if (IsLowPowerSleepFlagUp())
  {
    /* restore the previous values, Instead of reset */
    AccelDriverActiveConfig();    /* wake up process */
    motionMonitorState = GetStandByWakeupCause();

    sleep(3);  /* to remove the FIFO Status Error */
    if (motionMonitorState == WAKEUP_RTC)
    {
      xSemaphoreGive(timerSemaphore);  /* not wait for the cycle */
    }
  }
  else
  {
    EventStoreInitialize ();
    SetSleepCount (0);     /* Reset sleep Count after hibernate */
    FitmentDataHandler(MOUNT_SURVEY_DATA_RESET);  /* Reset survey values in case restore failed or from hibernate */
    ConsolePrintf("--->Boot up Normal Mode! %d\r\n", IsLowPowerSleepFlagUp());
  }

  /* Infinite loop */
  for (;;)
  {
    WatchdogSetWaiting(MOTIONMONITORTASK);

    /* Reload the hardware counter */
    SystemFreeWatchDogReload();

    /* Only proceed if the timer has fired.*/
    if (pdTRUE == xSemaphoreTake(timerSemaphore, portMAX_DELAY))
    {
      WatchdogSetActive(MOTIONMONITORTASK);

      /*
       * Read from the accelerometer
       * See how many samples are available to read in the FIFO
       * TODO - this section is too implementation specific.  The motion monitor should NOT know / care about the accelerometer implementation!!
       */
      uint16_t SamplesAvailable;
      if (!AccelDriverGetFifoStatus(&SamplesAvailable))
      {
        /* Error getting FIFO status - reset the FIFO */
        AccelDriverResetFifo();
        SamplesAvailable = 0;
        ConsolePrintf("***FIFO STATUS ERROR ****\n\r");
      }

      /*
       * Samples Available is expressed in terms of 16-bit words available -
       * this will be the number of bytes we get from each of the FIFO
       * registers (one low bytes, the other high bytes).
       */
      /* Limit the number of samples to be read in one go. */
      if (SamplesAvailable > MAX_FIFO_READ_SAMPLES)
      {
        SamplesAvailable = MAX_FIFO_READ_SAMPLES;
      }

      /*
       * The number of samples must be divisible by 6 (i.e. multiples of
       * Gx Gy Gz XLx XLy XLz). That always appears to be the case, but
       * we must make sure as otherwise we might lose track of whereabouts
       * in the datastream we are.
       */
      SamplesAvailable = (SamplesAvailable / 6) * 6;

      // For most of our calculations we need to know how many sets of XYZPRT we have, not individual samples
      uint16_t SamplesRead = SamplesAvailable / 6;
      //TRACE_IMU("SA= %d\n\r", SamplesRead);

      //XyzGetFilteredData(&accelDataOffset);

      while (SamplesAvailable > 0)
      {
        ImuData_t ImuData;

        AccelDriverGetFifoDataPair(&ImuData.gyro.X);
        AccelDriverGetFifoDataPair(&ImuData.gyro.Y);
        AccelDriverGetFifoDataPair(&ImuData.gyro.Z);

        AccelDriverGetFifoDataPair(&ImuData.accel.X);
        AccelDriverGetFifoDataPair(&ImuData.accel.Y);
        AccelDriverGetFifoDataPair(&ImuData.accel.Z);

        //TODO It would be better to read from the accelDriver in mG to begin with!  We should not know about the specific accelerometer raw output format here.
        uint32_t intermediateValue = ((int32_t)ImuData.accel.X) * 1000;
        ImuData.accel.X = (int16_t)(intermediateValue / 2048);

        intermediateValue = ((int32_t)ImuData.accel.Y) * 1000;
        ImuData.accel.Y = (int16_t)(intermediateValue / 2048);

        intermediateValue = ((int32_t)ImuData.accel.Z) * 1000;
        ImuData.accel.Z = (int16_t)(intermediateValue / 2048);

        /* If we are in Fake Mini-Event mode, overwrite with fake data here */
        if (FakeMiniEvent)
        {
          /* Each time we fake a mini-event make the X value of the accel data very slightly
           * bigger, this helps us to identify the mini-event during debug.
           */
          static uint16_t increment = 0;
          /* Replace real IMU data with fake data */
          ImuData.accel.X = 2500 + increment;
          ImuData.accel.Y = 2000;
          ImuData.accel.Z = 2000;
          FakeMiniEvent--;
          if (FakeMiniEvent == 0)
          {
            /* Last data replacement for this mini-event. Increase the increment
             * for the next one.
             */
            increment++;
          }
        }

        /* Raw Data needed for Factory Test */
        memcpy(&RawData, &ImuData, sizeof(ImuData));

        /* ITS-077 packets report absolute XYZ values that are not affected by the XYZ normalization setting. 
         * Therefore, IMU data should be passed to the filter before normalization.
         */
        XyzFilterAddToFifoMean(&ImuData.accel);

        /* Fitment Process */
        if (IsFitmentNormalizationOn() || (IsMountCheckActive()))
        {  /* matrix rotation and mount position process start */

          int pitch, roll;
          CalculateOrientation((float)ImuData.accel.X, (float)ImuData.accel.Y, (float)ImuData.accel.Z, &pitch, &roll);

          /* Update the Mount Survey with IMU Data*/
          FitmentMountSurveyUpdate((const ImuData_t*) &ImuData, pitch, roll);

          /* Apply Fitment Matrix to the IMU Data */
          /* !! IMU Data will be altered !!*/
          FitmentApplyRotationToIMU(&ImuData, pitch, roll);  
        }
        /*
         * Subtract XYZ offsets to zero out the "DC" effect of gravity, and
         * give us acceleration data that is referenced to zero in each axis
         */
//        ImuData.accel.X -= accelDataOffset.X;
//        ImuData.accel.Y -= accelDataOffset.Y;
//        ImuData.accel.Z -= accelDataOffset.Z;

        if (streamXYZ)
        {
          TRACE_IMU("XYZ= %4d, %4d, %4d  GYRO= %4d, %4d, %4d \n\r", ImuData.accel.X, ImuData.accel.Y, ImuData.accel.Z,
                      ImuData.gyro.X, ImuData.gyro.Y, ImuData.gyro.Z);
        }

        MotionBufferWriteSingle(&ImuData);

        SamplesAvailable -= 6;
      }

      XyzFilterAddFifoMeanIntoFilter();

      /*
       * See if there were any events in the Accelerometer data we have just read.
       * Note: We do not respond to events in factory test mode
       */
      if (!TestModeGet())
      {
        DetectEvents(SamplesRead);
      }

      /* If the RTC woke us up it may be time to send a heartbeat and instant Event */
      if (WAKEUP_RTC == motionMonitorState)
      {
        MotionMonitorProcessDHMAndInstantEvents();
      }

      /* Notify Button Module for the time elapsed of key hold */
      ButtonMonitorHoldCountInc();

      /*
       * See if we should change state.
       */
      StateTransitionCheck();
    }

    if (hibernateRequested)
    {
      hibernateRequested = false;

      /* Hibernate the MCU - will only return if USB power present */
      PowerStartHibernation();
    }
    /* debug print */
#ifdef DEBUG
    if (WakeupCount > 0)
    {
      TRACE_IMU("Wake up in MM SC=%d  W-Cause=%d Wake_CN=%d\n\r",  GetSleepCount(),  motionMonitorState, WakeupCount);
    }
#endif
  }
}


/**
 * @brief     Allow/disable Sleep
 * @param     Whether to allow sleep
 * @return    None
 */
void MotionMonitorSleepAllowed (bool allowed)
{
  sleepAllowed = allowed;
}


/**
 * @brief     get the last heart beat time stamp
 * @param     Whether to allow sleep
 * @return    last time stamp value
 */
uint32_t GetLastDHMTime (void)
{
  return LastDhmTime;
}


/**
 * @brief     Set the last heart beat time stamp before LP sleep
 * @param     ts: Time stamp value set to the current static variable
 * @return    None
 */
void SetLastDHMTime (uint32_t ts)
{
  LastDhmTime = ts;
}


/**
 * @brief Request Hibernation (MCU Standby)
 * @param None
 * @retval None
 */
void MotionMonitorHibernate (void)
{
  /*
   * The request has been made and the Motion Monitor will start
   *  hibernation in its main loop
   */
  hibernateRequested = true;
}


/**
 * @brief     Get current XYZ Raw values
 * @param     pAccelData  : Pointer to AccelData structure
 * @return    None
 */
void MotionMonitorGetRawAccelData (AccelerationData_t *pAccelData)
{  
  /* Get the Raw Data from the Accelerator */
  *pAccelData = RawData.accel;
}


/**
 * @brief  Called by the sequencer when it has finished the "impact" sequence
 * @param  None
 * @retval None
 */
void MotionMonitorImpactSequenceFinished (void)
{
  impactDataFinishedReceived = true;
}



/**
 * @brief  Called by the sequencer when it has finished the "end of journey" sequence
 * @param  None
 * @retval None
 */
void MotionMonitorEojSequenceFinished (void)
{
  eojFinishedReceived = true;
}


/**
 * @brief  Called by the sequencer when it has finished the "heart beat" sequence
 * @param  None
 * @retval None
 */
void MotionMonitorDhmSequenceFinished (void)
{
  dhmFinishedReceived = true;
}


/**
 * @brief  Called by the sequencer when it has finished the "self test" sequence
 * @param  None
 * @retval None
 */
void MotionMonitorDstmSequenceFinished (void)
{
  dstmFinishedReceived = true;
}


/**
 * @brief  Get the current motion monitor state
 * @param  None
 * @retval eMmState motionMonitorState
 */
eMmState MotionMonitorGetState (void)
{
  return motionMonitorState;
}


/**
 * @brief   Get the HP filter index
 * @param   none
 * @retval  current hpf index
 */
int GetLastHPfilterIndex (void)
{
  return (FilterIndex);
}


/**
 * @brief   restore HP filter  from PSRAM
 * @param   hpfIndex: index before sleep
 * @retval  none
 */
void RestoreLastHPfilterIndex (int hpfIndex)
{
  FilterIndex = hpfIndex;
}


/**
 * @brief   return address pointer to HP filter buffer
 * @param   none
 * @retval  pointer to HP filter buffer
 */
uint8_t * GetHPFilerBufferPointer (void)
{
  return ((uint8_t *) &Filter);
}


/**
 * @brief   return size of  HP filter
 * @param   none
 * @retval  size of  HP filter.
 */
uint16_t GetHPFilerBufferSize(void)
{
  return ((uint16_t) (sizeof(ImuData_t) * DC_FILTER_LENGTH));
}


/**
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */
/**
 * @brief     Timer Callback Function
 * @param     Timer Handle
 * @return    None
 */
static void MmTimerCallback (xTimerHandle timer)
{
  if (timer == mmTimer)
  {
    xSemaphoreGiveFromISR(timerSemaphore, NULL);
    MountSurveyFlagCheck();  /* set mount checking when wake up */
  }
}


/**
 * @brief    Set the mount survey checking flag
 * @param     None
 * @return    None
 */
static void MountSurveyFlagCheck (void)
{
  MountTmerCount ++;
  if (MountTmerCount >= MM_SURVEY_INTERVAL_DELAY)  /* 5Hz timer, 10s interval */
  {
    FitmentActivateMountCheck();    /* set mounting survey flag */
    MountTmerCount = 0;
  }
}


/**
 * @brief     Check if we must change Motion Monitor State
 * @param     None
 * @return    None
 */
static void StateTransitionCheck (void)
{
  /* if SOJ is detected we always switch to the IN_JOURNEY state */
  if (sojDetected)
  {
    motionMonitorState = IN_JOURNEY;
  }

  /* check for end of journey */
  if ((IN_JOURNEY == motionMonitorState) && (eojDetected))
  {
    motionMonitorState = ENDING_JOURNEY;
  }

  /* Check if it is time to go back to sleep */
  if (ReadyToSleep(false))
  {
    dhmFinishedReceived = false;
    eojFinishedReceived = false;

    /* checking mounting state before sleep */
    if (WAKEUP_RTC == motionMonitorState)
    {
      FitmentActivateMountCheck();  /* set to check mount position once before each sleep */
    }
    else if (WAKEUP_IMU == motionMonitorState) 
    {      
      if (isSojEverStarted == false)
      {
        /* woke up by IMU, but no Joruney detected */
        StatusLogWriteEventCode(STATUS_IMU_WOKE_WITH_NO_SOJ, STATUS_DATA_LOAD_NONE);
      }
    }
    GoToSleep();
  }
  else
  {
    ResetLPSleepStatus(RTC_BKUP_STANDBYSLEEP_FLAG);  /* clear the flag now, it will set again before next LP sleep */
  }

  /* update the "stay awake" counter */
  if (WakeupCount)
  {
    WakeupCount--;
  }
}


/**
 * @brief     Check if the system is in a state where sleep is possible
 * @param     None
 * @return    true if system can sleep, false otherwise
 */
static bool ReadyToSleep (bool printDetails)
{
  bool ReadyToSleepFlag = false;

  /* gather system state information */
  bool SeqBusy = SequencerBusy();
  bool ConsoleBusy = (RtcGetTimeStamp() < (ConsoleGetLastInputTime() + MM_CONSOLE_STAY_AWAKE_SECONDS));
  bool SojInProcess = (IN_JOURNEY != motionMonitorState) && (ENDING_JOURNEY != motionMonitorState) && (DetectEventSojInProcess());
  bool ImpactInProcess = DetectEventImpactInProcess();
  bool TestMode =   TestModeGet();
  bool RBDown = ButtonMonitorIsButtonDown();

  /* check if all systems are in a state where they can sleep */
  if ( (sleepAllowed)
    && (!WakeupCount)
    && (!SeqBusy)
    && (!ConsoleBusy)
    && (!SojInProcess)
    && (!ImpactInProcess)
    && (!RBDown)
    && (!TestMode) )
  {
    ReadyToSleepFlag = true;
    SetupLPSleepModeFlag();
  }

  if (printDetails)
  {
    /* print system state information affecting sleep */

    TRACE_IMU("ReadytoSleep = %d:  SleepAllowed=%d, WakeupCount=%d, SequencerBusy=%d, SojInProcess=%d, TestMode=%d, ConsoleBusy=%d, motionMonitorState=%d, RBinHold=%d, SleepCT=%d\n\r",
        ReadyToSleepFlag, sleepAllowed, WakeupCount, SeqBusy, SojInProcess, TestMode, ConsoleBusy, motionMonitorState, RBDown, GetSleepCount());
    TRACE_IMU("flags=0x%X, WU_cause=0x%x WU_count=%d\r\n\r\n", GetLPSleepStatus(), motionMonitorState, WakeupCount);
    sleep(5);

  /*  TRACE_IMU("ReadytoSleep = %d:  SleepAllowed=%d, WakeupCount=%d, SequencerBusy=%d, SojInProcess=%d, ImpactInProcess=%d, TestMode=%d, ConsoleBusy=%d, motionMonitorState=%d, RBinHold=%d\n\r",
    		ReadyToSleep, sleepAllowed, WakeupCount, SeqBusy, SojInProcess, ImpactInProcess, TestMode, ConsoleBusy, motionMonitorState, RBDown);
   */
  }
  return ReadyToSleepFlag;
}


/**
 * @brief     Back up system states to RTC bkup registers
 * @param     none
 * @return    none
 */
static void SaveValuesBeforeSleep (void)
{
   RtcBkupWriteRegister(RTC_BACKUP_TIMESTAMP, RtcGetTimeStamp() % MM_SECOND_IN_HOUR);
   RtcBkupWriteRegister(RTC_BACKUP_BATTERY_VOLTAGE, BatteryMonitorGetVoltage());
  /* WKUP pin state */
  (GpioReadPin(CHARGER_USB_DETECT_PORT, CHARGER_USB_DETECT_PIN) == IO_PIN_SET)?
      SetLPSleepStatus(RTC_BKUP_USB_CABLE_FLAG): ResetLPSleepStatus(RTC_BKUP_USB_CABLE_FLAG);
  (GpioReadPin(RED_BUTTON_PORT, RED_BUTTON_PIN) == IO_PIN_SET)?
      SetLPSleepStatus(RTC_BKUP_REDBUTTON_FLAG): ResetLPSleepStatus(RTC_BKUP_REDBUTTON_FLAG);
  (GpioReadPin(ACCEL_INT2_PORT, ACCEL_INT2_PIN) == IO_PIN_SET)?
      SetLPSleepStatus(RTC_BKUP_ACCEL_INT2_FLAG): ResetLPSleepStatus(RTC_BKUP_ACCEL_INT2_FLAG);
  (BatteryGetChargerState())? SetLPSleepStatus(RTC_BKUP_CHARGER_FLAG): ResetLPSleepStatus(RTC_BKUP_CHARGER_FLAG);
}


/**
 * @brief     Detect events in the accelerometer data
 * @param     Number of samples Read from IMU, must be divisible by 6 
 *            (i.e. multiples of Gx Gy Gz XLx XLy XLz)
 * @return    None
 */
static void DetectEvents (uint16_t noOfSamples)
{
  static bool ImpactProcessing = false;
  static int  ImpactSendTimeout = 0;
  eojDetected = false;

  /*
   * Clip the number of samples to make sure we do not overrun the buffer when reading.  This should
   * never occur, but is here for safety
   */
  if (noOfSamples > MM_DETECT_BUFFER_ELEMENTS)
  {
    noOfSamples = MM_DETECT_BUFFER_ELEMENTS;
  }

  /*
   * The event detection algorithms are run on the incoming data in batches.  We use the exact number of samples
   * read from the IMU in each batch to make sure there are gaps or repetitions in the data
   */
  MotionBufferGetLatestData(&DetectBuffer[0], noOfSamples);

  /* if we are not already processing an impact event, look for events */
  if (!ImpactProcessing)
  {
    /* Check the motion buffer for detected impacts */
    if (DetectEventImpact(noOfSamples) || spoofImpact)
    {
      /* its-072, default with unconditional report impact event, override this by set flag 12 1*/
      bool useImpactFilter = ConfigGetFlag(CONFIG_FLAG_IMPACT_FILTER);

      /*
      * Impact Event Filtering (useImpactFilter set with true) when Screen Mount/Normalization enabled.
      * We will only stop the reporting if:
      *     1. XYZ Normailzation is enabled, AND
      *     2. screen is not mounted 
      * 
      *  Conditions:
      *   XYZ Normalized | Screen Mounted | Result
      *   ---------------|----------------|----------------
      *        true      |      true      | Report
      *        true      |      false     | Do NOT report
      *        false     |      true      | Report
      *        false     |      false     | Report
      */

      bool shouldReportImpactEvent = useImpactFilter && ((!ConfigGetFlag(CONFIG_FLAG_XYZ_NORMALISTATION))|| (IsScreenMountedPreviously()) ); 

      if (!useImpactFilter || shouldReportImpactEvent)
      {
        /* Notify the sequencer to start the emergency sequence */
        SequencerImpact(DetectFreefallEvent());
        ImpactSendTimeout = 0;
        ImpactProcessing = true;
      }
      /* clear the spoof impact flag */
      spoofImpact = false;
    }
    else
    {
      DetectEventMini(noOfSamples);

      /* Detect Corner event if in Journey and normalization is on */
      if ((motionMonitorState == IN_JOURNEY) &&  IsFitmentNormalizationOn())
      {
        DetectEventCorner(noOfSamples);
      }

    }
  }
  else
  {
    /*
     * We are already processing an impact
     * Check if the sequencer has finished processing it, or we have timed out
     */
    if ((++ImpactSendTimeout >= MM_IMPACT_WAIT_TIMEOUT_TICKS) || (impactDataFinishedReceived))
    {
      ImpactProcessing = false;
      impactDataFinishedReceived = false;

      /* end of impact always indicates end of journey too */
      if (motionMonitorState == IN_JOURNEY)
      {
        spoofStartOfJourney = false;
        eojDetected = true;
      }
    }
  }

  /*
   * Check for Start of Journey/End of Journey regardless of Impact
   * or Mini-Event detection.
   */
#ifndef DISABLESOJEOJDETECTION

  /*
   * High pass filter the data to remove DC and low frequency offsets, which we do not want for
   * start and end of journey detection.
   * NOTE: detect buffer is separate from main motionbuffer, so the original data is unaffected
   */
  MotionMonitorHpFilter(&DetectBuffer[0], noOfSamples);

  if (!batteryTooLowForJournery)
  {
    if (motionMonitorState == IN_JOURNEY)
    {
      /*
       * Look for End of Journey
       * NOTE: journeys started with a spoof SOJ can only be ended with a spoof EOJ
       */
      if (false == spoofStartOfJourney)
      {
        if (!eojDetected && DetectEventEOJ(noOfSamples))
        {
          /* End of Journey Detected, get ready for next round of SOJ Detection */
          eojDetected = true;
          sojDetected = false;
        }
      }
    }
    else
    {
      /* Look for Start of Journey, if never detected */
      if (!sojDetected && DetectEventSOJ(noOfSamples))
      {
        /* New Journey Detected, get ready for EOJ Detection */
        sojDetected = true;
        eojDetected = false;

        /* Mark there is SOJ happened*/
        isSojEverStarted = true;
      }
    }
  }

  /* Update EOJ or SOJ accordingly if Low battery and not on USB power */
  if (POWER_STATE_NORMAL != BatteryMonitorGetPowerState())
  {
    if (motionMonitorState == IN_JOURNEY)
    {
      batteryTooLowForJournery = true;
      eojDetected = true;
      TRACE_IMU("*****************  EOJ Detected! (Low Battery) *******************\n\r");
    }
    // Do not attempt to start journey
    if (sojDetected == true)
    {
      batteryTooLowForJournery = true;
      sojDetected = false;
      TRACE_IMU("*****************  SOJ Rejected! (Low Battery) *******************\n\r");
    }
    spoofEndOfJourney = false;
    spoofStartOfJourney = false;
  }
  else
  {
    batteryTooLowForJournery = false;
  }
#endif
  /* Handle spoofing of SOJ/EOJ */
  if (motionMonitorState == IN_JOURNEY)
  {
    if (spoofEndOfJourney)
    {
      eojDetected = true;
      sojDetected = false;
      spoofEndOfJourney = false;
      spoofStartOfJourney = false;
    }
  }
  else
  {
    if (spoofStartOfJourney)
    {
      sojDetected = true;
      eojDetected = false;

      /* Mark there is SOJ happened*/
      isSojEverStarted = true;
    }
  }

  /*
   * Send messages to GSM Manager until SeqJourney acknowledged
   */
  if (sojDetected && (motionMonitorState != IN_JOURNEY) && !SeqJourneyInProgress())
  {
    /* Make sure we cant go to sleep before sequencer kicks in */
    if (WakeupCount < MM_WAKEUP_EVENT_TIMEOUT)
    {
      WakeupCount = MM_WAKEUP_EVENT_TIMEOUT;
    }

    /* Send message to GSM Manager */
    SequencerStartOfJourney();
  }

  if (eojDetected)
  {
    /* Send message to GSM Manager */
    SequencerEndOfJourney();
  }
}


/**
 * @brief     Goto Sleep (STM32 Stop Mode)
 * @param     Whether to sleep for the full duration
 * @return    None
 */
static void GoToSleep (void)
{
  /* Set the new wakeup time in the RTC */
  RtcSetWakeupTimer(MM_RTC_SLEEP_IN_SECONDS);
  /*
   * Now put the device to sleep
   */
  if (IsLowPowerSleepFlagUp())
  {
    UpdateResetDataToRAM(PSRAM_LP_MCU_STANDBY_SLEEP_MARK);  /* save current data to RAM */
    TRACE_IMU("Going into Sleep (MCU Reset), Sleep_Count=%d\n\r",GetSleepCount());
  }
  else
  {
    TRACE_IMU("Going into Sleep (MCU Stop)\n\r");
  }
  /* 171 osDelay(50);*/
  sleep(2);
  SaveValuesBeforeSleep ();
  PowerPutPeripheralsToSleep();
  AccelDriverSleepConfig();
  UsbWakeUp = false;
  AccelWakeUp = false;
  isSojEverStarted = false;

  SystemSleep();

  /**********************************************************/
  /*                        SLEEPING                        */
  /**********************************************************/

  /* In Standby sleep mode, MCU reset the MCU. not wake up to here but from 0X00000  */
  /*
   * On wake in deep sleep mode, make sure the device starts up properly
   */
  SystemClockInit();
  PowerWakeupPeripherals();
  AccelDriverActiveConfig();   /* 15ms FIFO reset delay */
  MotionBufferResetBuffer();

  /* Determine the cause of wake-up */
  motionMonitorState = MotionMonitorGetWakeupCause();

  /* Clear RTC wake up interrupt flags and deactiveate the timer in case we have been woken by something else */
  RtcClearWakeupTimer();
  sleep(100);  /* for bus to stable to avoid FIFO error */
  if (motionMonitorState == WAKEUP_RTC)
  {
    xSemaphoreGive(timerSemaphore);  /* not wait for the cycle */
  }
}


/**
 * @brief     Determine the cause of wake up from standby sleep
 * @param     None
 * @return    The wake up cause
 */
static eMmState GetStandByWakeupCause (void)
{
  eMmState WakeupCause;
  int16_t dt;
  bool usb_cable_state;
  bool red_buttone_state;

  /* get current status */
  dt = (RtcGetTimeStamp() % MM_SECOND_IN_HOUR) - RtcBkupReadRegister(RTC_BACKUP_TIMESTAMP);
  if (dt <= 0)
  {
    dt += MM_SECOND_IN_HOUR;   /* hour roll over case correction */
  }
  usb_cable_state = GpioReadPin(CHARGER_USB_DETECT_PORT, CHARGER_USB_DETECT_PIN);
  red_buttone_state = GpioReadPin(RED_BUTTON_PORT, RED_BUTTON_PIN);
  #ifdef GD32F30X_HD
  if ((PMU_CS & PMU_FLAG_WAKEUP) == PMU_FLAG_WAKEUP)
  #else
  if(__HAL_PWR_GET_FLAG(PWR_FLAG_WU) != RESET)
  #endif
  {
    TRACE_IMU("Wake-up (IMU)\r\n");
    WakeupCount = MM_WAKEUP_IMU_TIMEOUT;
    WakeupCause = WAKEUP_IMU;
  }

  if (dt >= MM_RTC_SLEEP_IN_SECONDS)
  {
    TRACE_IMU("Wake-up (RTC)\n\r");
    WakeupCount = 0;          /*no wait for RTC cause wake up, go to sleep again at once, no waiting */
    WakeupCause = WAKEUP_RTC;
  }
  else if ((bool)(GetLPSleepStatus() & RTC_BKUP_USB_CABLE_FLAG) != usb_cable_state)
  {
    (usb_cable_state == IO_PIN_SET)? TRACE_IMU("Wake-up (USB ON)\n\r"): TRACE_IMU("Wake-up (USB OFF)\n\r");
    WakeupCount = MM_WAKEUP_USB_TIMEOUT;
    WakeupCause = WAKEUP_USB;
  }
  else
  {
    if (red_buttone_state == false)  /* still hold */
    {
      /* hold red button from the standby sleep mode, it need to hold longer than about 200ms to register the action. Then
       the button handler will took over to process for the following actions */

      SetButtonDownFlag (true);    /* set button down flag for the button handler */

      WakeupCount = MM_WAKEUP_RTC_TIMEOUT * 10;  /* delay for button press */
      WakeupCause = WAKEUP_REDBUTTON;
    }
  }
  TRACE_IMU(" --> WakeUp: Cause=%d dt=%d\r\n", WakeupCause, dt);

  return (WakeupCause);
}


/**
 * @brief     Determine the cause of wake up from sleep
 * @param     None
 * @return    The wake up cause
 */
static eMmState MotionMonitorGetWakeupCause (void)
{
  eMmState WakeupCause;

  if (RtcGetWakeupFlag())
  {
    TRACE_IMU("Wake-up (RTC)\n\r");
    WakeupCount = MM_WAKEUP_RTC_TIMEOUT;
    WakeupCause = WAKEUP_RTC;
  }
  else if (AccelWakeUp)
  {
    AccelWakeUp = false;
    TRACE_IMU("Wake-up (IMU)\n\r");
    WakeupCount = MM_WAKEUP_IMU_TIMEOUT;
    WakeupCause = WAKEUP_IMU;
  }
  else if (UsbWakeUp)
  {
    UsbWakeUp = false;
    TRACE_IMU("Wake-up (USB)\n\r");
    WakeupCount = MM_WAKEUP_USB_TIMEOUT;
    WakeupCause = WAKEUP_USB;
  }
  else
  {
    TRACE_IMU("Wake-up (RED-BUTTON / DEBUGGER)\n\r");
    WakeupCount = MM_WAKEUP_RTC_TIMEOUT;
    WakeupCause = WAKEUP_REDBUTTON;
  }
  return (WakeupCause);
}


/**
 * @brief  Process if DHM or instant message need to be send 
 * @param   None
 * @retval  None
 */
static void MotionMonitorProcessDHMAndInstantEvents (void)
{
  bool msgEnqueued = false;
  
  if (!SequencerBusy())
  {
    /* Check if it is time to send DHM if sequencer is not busy */
    msgEnqueued = MotionMonitorHandleDHM();
  }

  /* Check if any Instant Event is matured */
  if (StatusLogInstantEventMatureCheck())
  {
    /* Enqueued the report event message for sending */
    msgEnqueued |= SequencerProcessInstantEventRequest();
  }

  /* Extend the sleep time if any msg is enqueued for sequencer */
  if (msgEnqueued)
  {
    /* make sure we don't go to sleep before sequencer kicks in */
    if (WakeupCount < MM_WAKEUP_EVENT_TIMEOUT)
    {
      WakeupCount = MM_WAKEUP_EVENT_TIMEOUT;
    }
  }

}


/**
 * @brief Send a heartbeat message if enough time has expired
 * @param   None
 * @retval  true if DHM message is enqueued, false otherwise
 */
static bool MotionMonitorHandleDHM (void)
{
  bool msgEnqueued = false;
  /* get the current time */
  uint32_t CurrentTimeStamp = RtcGetTimeStamp();

  /* sanity checks */
  if ( (0 == LastDhmTime) || (LastDhmTime > CurrentTimeStamp))
  {
    if (LastDhmTime)
    {
      ConsolePrintf("ERROR - LastDhmTime has bad value %u\n\r", LastDhmTime);
    }
    LastDhmTime = CurrentTimeStamp;
  }
  /* check if it is time to send DHM */
  if (CurrentTimeStamp >= (LastDhmTime + MM_HEARTBEAT_PERIOD_SECONDS))
  {
    /* Send Heart beat message to the sequencer */
    dhmFinishedReceived = false;
    SequencerHeartBeat();
    msgEnqueued = true;
    /* its110, Run WiFi patch to keep WiFi chips in low power mode */
    WifiMemCEFix();
    ConsolePrintf("DHM process C[%u] L[%u]\n\r", CurrentTimeStamp, LastDhmTime);

    /* Update the last DHM time */
    LastDhmTime = CurrentTimeStamp;
  }

  return msgEnqueued;
}


/*
 * @brief     Perform a basic high pass filter of the data
 * @param     pBuffer      : pointer to a buffer to containing the data
 * @param     noOfElements : number of elements in the buffer
 * @return    None
 */
static void MotionMonitorHpFilter (ImuData_t *pBuffer, uint16_t noOfElements)
{
  ImuData_t RollingAverage;

  for (int i = 0; i < noOfElements; i++)
  {
    /* add the raw data to the filter */
    Filter[FilterIndex].accel.X = pBuffer[i].accel.X;
    Filter[FilterIndex].accel.Y = pBuffer[i].accel.Y;
    Filter[FilterIndex].accel.Z = pBuffer[i].accel.Z;
    FilterIndex = (FilterIndex + 1) % DC_FILTER_LENGTH;

    /* calculate the rolling average (low pass filter) */
    RollingAverage.accel.X = 0;
    RollingAverage.accel.Y = 0;
    RollingAverage.accel.Z = 0;
    for (int z=0; z<DC_FILTER_LENGTH; z++)
    {
      RollingAverage.accel.X += Filter[z].accel.X;
      RollingAverage.accel.Y += Filter[z].accel.Y;
      RollingAverage.accel.Z += Filter[z].accel.Z;
    }
    RollingAverage.accel.X /= DC_FILTER_LENGTH;
    RollingAverage.accel.Y /= DC_FILTER_LENGTH;
    RollingAverage.accel.Z /= DC_FILTER_LENGTH;

    /* subtract the rolling average from the raw data to leave high frequency only (high pass filter) */
    pBuffer[i].accel.X -= RollingAverage.accel.X;
    pBuffer[i].accel.Y -= RollingAverage.accel.Y;
    pBuffer[i].accel.Z -= RollingAverage.accel.Z;
  }
}



/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       Spoof a device event (Heartbeat, Start of Journey, End of Journey, Impact)
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int MotionMonitorConsoleSpoof (int argCount, char **pArguments)
{
  if (1 == argCount)
  {
    if (!strcmp(pArguments[0], "dhm"))
    {
      SequencerHeartBeat();
    }
    else if (!strcmp(pArguments[0], "soj"))
    {
      spoofStartOfJourney = true;
    }
    else if (!strcmp(pArguments[0], "eoj"))
    {
      spoofEndOfJourney = true;
    }
    else if (!strcmp(pArguments[0], "impact"))
    {
      spoofImpact = true;
    }
    else if (!strcmp(pArguments[0], "stest"))
    {
      SequencerSelfTest();
    }
    else if (!strcmp(pArguments[0], "sinstant"))
    {
      SequencerProcessInstantEventRequest();
    }
    else if (!strcmp(pArguments[0], "sleep"))
    {
      GoToSleep();
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


/**
 * @brief       Show current IMU data
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int MotionMonitorConsoleShowData (int argCount, char **pArguments)
{
  GnssEventMotionData_t testGnssEvent;
  int pitch;
  int roll;

  ConsolePrintf("Accel = %d %d %d\n\r",
                RawData.accel.X,
                RawData.accel.Y,
                RawData.accel.Z);

  ConsolePrintf("Gyro = %d %d %d\n\r",
                RawData.gyro.X,
                RawData.gyro.Y,
                RawData.gyro.Z);

  /* get the orientation */
  CalculateOrientation((float)RawData.accel.X, (float)RawData.accel.Y, (float)RawData.accel.Z, &pitch, &roll);
  ConsolePrintf("Pitch=%d, roll=%d\n\r", pitch, roll);

  if (GnssEventGetRecordCount() > 0)
  {
    GnssEventReadLatest(&testGnssEvent);
    ConsolePrintf("Gnss = %ld %ld %d %u, time = %d, status = %d X=%d, Y=%d, Z=%d\n\r",
        testGnssEvent.gnssData.Latitude,
        testGnssEvent.gnssData.Longitude,
        testGnssEvent.gnssData.Heading,
        testGnssEvent.gnssData.Speed,
        testGnssEvent.gnssData.TimeStamp,
        testGnssEvent.gnssData.Status,
        testGnssEvent.accelData.X,
        testGnssEvent.accelData.Y,
        testGnssEvent.accelData.Z);
  }
  else
  {
    ConsolePrintf("Gnss = No Data\n\r");
  }

  /* indicate success */
  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       enable or disable sleep
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int MotionMonitorConsoleAllowSleep (int argCount, char **pArguments)
{
  switch (argCount)
  {
    case 0:
      ConsolePrintf("Sleep (CPU Stop): %s\n\r", sleepAllowed ? "true" : "false");
      return (CONSOLE_ERROR_OK);
    case 1:
      if (!strcmp(pArguments[0], "true"))
      {
        MotionMonitorSleepAllowed(true);
        ConsolePrintf("Sleep (CPU Stop) enabled\n\r");
      }
      else if (!strcmp(pArguments[0], "false"))
      {
        MotionMonitorSleepAllowed(false);
        ConsolePrintf("Sleep (CPU Stop) disabled\n\r");
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


/**
 * @brief       Put the device into hibernation
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int MotionMonitorConsoleHibernate (int argCount, char **pArguments)
{
  ConsolePrintf("Going into hibernation (MCU Standby)\n\r");
  MotionMonitorHibernate();

  /* indicate success */
  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       Turn streaming of IMU XYZ data on or off
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int MotionMonitorConsoleStream (int argCount, char **pArguments)
{
  switch (argCount)
  {
    case 0:
      ConsolePrintf("XYZ Streaming: %s\n\r", streamXYZ ? "true" : "false");
      return (CONSOLE_ERROR_OK);
    case 1:
      if (!strcmp(pArguments[0], "true"))
      {
        streamXYZ = true;
      }
      else if (!strcmp(pArguments[0], "false"))
      {
        streamXYZ = false;
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


/**
 * @brief       Print the value of LastDhmTime (for debugging)
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int MotionMonitorConsoleDhmTime (int argCount, char **pArguments)
{
  ConsolePrintf("Last DHM time: ");
  RtcPrintTimestamp(LastDhmTime);
  ConsolePrintf("\n\r");

  /* indicate success */
  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       Check if system is ready to sleep and print result
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int MotionMonitorConsoleReadyToSleep (int argCount, char **pArguments)
{
  ReadyToSleep(true);

  /* indicate success */
  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       Fake a mini-event.  Injects peak into accelerometer data, which then
 *              flows through the entire event detection and packet creation processes
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int MotionMonitorConsoleFakeMiniEvent (int argCount, char **pArguments)
{
  int lengthOfEvent = 0;

  /* check for correct number of arguments */
  if (1 == argCount)
  {
    if (1 == sscanf(pArguments[0], "%d", &lengthOfEvent))
    {
      if ((lengthOfEvent > 0) && (lengthOfEvent <= 100))
      {
        /* Cause some fake mini-event samples to be injected into the IMU data */
        FakeMiniEvent = lengthOfEvent;
      }
      else
      {
        return(CONSOLE_ARG_RANGE);
      }
    }
    else
    {
      return(CONSOLE_ARG_RANGE);
    }
  }
  else if (0 == argCount)
  {
    /* Default length of mini-event = 10 samples*/
    FakeMiniEvent = 10;
  }

  /* indicate success */
  return (CONSOLE_ERROR_OK);
}



