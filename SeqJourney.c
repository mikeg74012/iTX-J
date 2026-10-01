/*
 **************************************************************************************************
 * @file   SeqJourney.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   2020-04-16
 *
 * @brief  Event sequencer for the "journey" state.  Handles the sequence of modem events that
 *         must be performed when the vehicle is in-journey.
 *         The sequence comprises:
 *           1. Power up the modem
 *           2. Configure and start the GNSS receiver
 *           3. Send a "start of journey" message to cloud
 *           4. Get the GNSS location, velocity and heading once per second
 *           5. Send "in-journey" data to cloud every 20 minutes
 *           6. On "impact" or "red button" event:
 *              a) Stop GNSS
 *              b) transition to "emergency" state
 *           7. On "end of journey" event:
 *              a) Send "end of journey" message to cloud
 *              b) Stop GNSS
 *              c) Power down modem
 *
 **************************************************************************************************
 */
#include "global.h"

/* #define USE_TEST_TIMINGS */

/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */
/* Timer period used for the in journey timer */
#define SJ_TIMER_PERIOD_MS     (1000)

/* Time between DJM messages (may eventually be a config item) */
#ifdef USE_TEST_TIMINGS
  #define DJM_PERIOD_SECONDS   (4 * 60)
#else
  #define DJM_PERIOD_SECONDS   (20 * 60)
#endif

/* radio scan timing (from start of journey) */
#define SCAN_TIME_SECONDS      (2 * 60)

/* List of sequencer states.  Used in conjunction with the sequencer state table */
typedef enum
{
  SJS_NONE,
  SJS_IDLE,
  SJS_POWER_ON,
  SJS_START_GNSS,
  SJS_INTIIAL_FIX,
  SJS_SEND_SOJ,
  SJS_IN_JOURNEY,
  SJS_GET_POSITION,
  SJS_SEND_DJM,
  SJS_SEND_IEM,
  SJS_BLE_SCAN,
  SJS_WIFI_SCAN,
  SJS_WIFI_STOP,
  SJS_JAM_DETECT,
  SJS_STOP_GNSS,
  SJS_SEND_EOJ,
  SJS_CHECK_SMS,
  SJS_POWER_OFF,
  SJS_ANY
} eSeqJourneyState;


/* Event types that can be sent to the sequencer */
typedef enum
{
  SJS_EV_NONE,
  SJS_EV_ENTRY,
  SJS_EV_EXIT,
  SJS_EV_SIGNAL,
} eSeqEvent;


/* Type of each entry in the sequencer state table */
typedef struct
{
  eSeqJourneyState   State;
  eSeqEvent          Event;
  void *             EventParam;
  void               (*pActionFunc)(void);
  int                NoOfRetries;
  eSeqJourneyState   NextState;
} SjStateTable_t;


/* Event type used by the sequencer.  Some events may include a parameter */
typedef struct
{
  eSeqEvent   EventType;
  void *      Parameter;
} SjEvent_t;


/* Event type used by the Seq Journey */
typedef struct
{
  /* Used to uniquely identify each journey.  The timestamp at journey start is used as a unique ID. */
  uint32_t JourneyId;

  /* Counter to keep track of Journey Start */
  uint32_t JourneyTime;

  /* Counter to keep track of time since we sent the last Device Journey Message */
  uint32_t SecondsSinceLastDjm;

  /* Counter to keep track of time since wifi scan start*/
  uint32_t JourneyWifiScanStartTime;

  /* Flag to keep track if radio scan is started */
  bool RadioScanStarted;

  /* GNSS data for the Start of Journey */
  GnssData_t SojData;

  /* GNSS data for the End of Journey */
  GnssData_t EojData;

} SeqJourneyTrip_t;


/* macro for generating a sequencer event */
#define SJ_EVENT(_ev, _par)       \
{                                 \
  SjEvent_t Event;                \
  Event.EventType = _ev;          \
  Event.Parameter = (void *)_par; \
  SeqJourneySequencer(Event);     \
}


/* macro to transition from one state to the next */
#define SJ_TRAN(_st)              \
{                                 \
  SjEvent_t Event;                \
  Event.Parameter = NULL;         \
  Event.EventType = SJS_EV_EXIT;  \
  SeqJourneySequencer(Event);     \
  CurrentState = _st;             \
  Event.EventType = SJS_EV_ENTRY; \
  SeqJourneySequencer(Event);     \
}


/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static void SeqJourneySequencer     (SjEvent_t event);
static void SeqJourneyTimerCallback (xTimerHandle timerHandle);
static void SeqJourneyStartGNSS     (void);
static void SeqJourneyStopGNSS      (void);
static void SeqJourneyCreateTripId  (void);
static void SeqJourneyGetInitialFix (void);
static void SeqJourneyMarkSojData   (void);
static void SeqJourneyMarkEojData   (void);
static void SeqJourneyValidateRtc   (void);
static void SeqJourneyGetPosition   (void);
static void SeqJourneyStartTimers   (void);
static void SeqJourneyStopTimers    (void);
static void SeqJourneySendSOJ       (void);
static void SeqJourneySendPJM       (void);
static void SeqJourneySendEOJ       (void);
static void SeqJourneyCheckSMS      (void);
static void SeqJourneyAbort         (void);
static void SeqJourneyInterrupt     (void);
static void SeqJourneyResume        (void);
static void SeqJourneyComplete      (void);
static void SeqJourneyPowerOn       (void);
static void SeqJourneyPowerOff      (void);
static void SeqJourneyGsmOff        (void);
static void SeqJourneyBleScan       (void);
static void SeqJourneyBleStop       (void);
static void SeqJourneyWifiScan      (void);
static void SeqJourneyWifiStop      (void);
static void SeqJourneyJamStart      (void);
static void SeqJourneyJamScanOn     (void);
static void SeqInstantSendIEM       (void);


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
/* Timer used to trigger "in-journey" events such as requesting GNSS data and sending DJM messages to cloud */
static TimerHandle_t  SjTimer;

/* The current state of this sequencer */
static eSeqJourneyState CurrentState = SJS_IDLE;

static SeqJourneyTrip_t CurrentTrip;

/*
 * Flag to keep track of journey starting (true when SoJ has been sent)
 * Used to determine whether we should resume an ongoing journey after a red-button event
 */
static bool JourneyInProgress = false;


/*
 * Table to define the sequence of actions to be carried out in the "journey" sequence
 */
static SjStateTable_t SjStateTable[] =
{
  /* STATE           EVENT            EVENT_PARAM                   ACTION FUNCTION          RETRIES     NEXT STATE     */
  {SJS_IDLE,         SJS_EV_ENTRY,    NULL,                         SeqJourneyComplete,      0,          SJS_IDLE         },  // on entry, indicate that we have completed the sequence
  {SJS_IDLE,         SJS_EV_SIGNAL,   (void *) SM_START_OF_JOURNEY, SeqJourneyCreateTripId,  0,          SJS_POWER_ON     },  // on SOJ event, change to START SJS_POWER_ON state

  {SJS_POWER_ON,     SJS_EV_ENTRY,    NULL,                         SeqJourneyPowerOn,       0,          SJS_POWER_ON     },  // on entry, tell modem to power up
  {SJS_POWER_ON,     SJS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,          SJS_START_GNSS   },  // on success, move to the SJS_START_GNSS state
  {SJS_POWER_ON,     SJS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,          SJS_START_GNSS   },

  {SJS_START_GNSS,   SJS_EV_ENTRY,    NULL,                         SeqJourneyStartGNSS,     2,          SJS_START_GNSS   },  // on entry, tell modem to start GNSS
  {SJS_START_GNSS,   SJS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   SeqJourneyJamStart,      0,          SJS_JAM_DETECT   },  // on Success, move to Jam detect start state, wait to access jam detect function
  {SJS_START_GNSS,   SJS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,          SJS_JAM_DETECT   },

  {SJS_JAM_DETECT,   SJS_EV_ENTRY,    NULL,                         SeqJourneyJamScanOn,     2,          SJS_JAM_DETECT   },  // on entry, check/enable the Jam detect function
  {SJS_JAM_DETECT,   SJS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,          SJS_INTIIAL_FIX  },  // on success, return to INITIAL_FIX state
  {SJS_JAM_DETECT,   SJS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,          SJS_INTIIAL_FIX  },  // on error, return to INITIAL_FIX state

  {SJS_INTIIAL_FIX,  SJS_EV_ENTRY,    NULL,                         SeqJourneyGetInitialFix, 5,          SJS_INTIIAL_FIX  },  // on entry, get first fix from the GNSS.  May have to try a few times to get location
  {SJS_INTIIAL_FIX,  SJS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   SeqJourneyMarkSojData,   0,          SJS_SEND_SOJ     },  // on Success, move to the SEND_SOJ state
  {SJS_INTIIAL_FIX,  SJS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      SeqJourneyValidateRtc,   0,          SJS_INTIIAL_FIX  },  // On Error, loop on initial fix state

  {SJS_INTIIAL_FIX,  SJS_EV_SIGNAL,   (void *) SM_JOURNEY_SEND_RDY, SeqJourneyMarkSojData,   0,          SJS_SEND_SOJ     },  // Seq ready to send Journey Info

  {SJS_SEND_SOJ,     SJS_EV_ENTRY,    NULL,                         SeqJourneySendSOJ,       5,          SJS_SEND_SOJ     },  // on entry, tell modem to send SOJ message
  {SJS_SEND_SOJ,     SJS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   SeqJourneyStartTimers,   0,          SJS_IN_JOURNEY   },  // on Success, move to the IN_JOURNEY state, start 1 second timer for GNSS
  {SJS_SEND_SOJ,     SJS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      SeqJourneyStartTimers,   0,          SJS_IN_JOURNEY   },

#if !MODEM_SEPARATE_GNSS_UART
  {SJS_IN_JOURNEY,   SJS_EV_SIGNAL,   (void *) SM_TIMER_GNSS,       NULL,                    0,          SJS_GET_POSITION },  // on GNSS timer, move to GET_POSITION state
#endif
  {SJS_IN_JOURNEY,   SJS_EV_ENTRY,    NULL,                         SeqJourneyGsmOff,        0,          SJS_IN_JOURNEY   },  // on entry, switch off the GSM to save power.  Only GNSS needed.
  {SJS_IN_JOURNEY,   SJS_EV_SIGNAL,   (void *) SM_RESUME,           SeqJourneyGsmOff,        0,          SJS_IN_JOURNEY   },  // on resume, continue journey
  {SJS_IN_JOURNEY,   SJS_EV_SIGNAL,   (void *) SM_TIMER_DJM,        NULL,                    0,          SJS_SEND_DJM     },  // on DJM timer, move to SEND_DJM state
  {SJS_IN_JOURNEY,   SJS_EV_SIGNAL,   (void *) SM_INSTANT_REPORT,   NULL,                    0,          SJS_SEND_IEM     },  // send instant event during journey
  {SJS_IN_JOURNEY,   SJS_EV_SIGNAL,   (void *) SM_TIMER_SCAN,       NULL,                    0,          SJS_BLE_SCAN     },  // on scan timer, start the radio scan

  {SJS_SEND_IEM,     SJS_EV_ENTRY,    NULL,                         SeqInstantSendIEM,       0,          SJS_SEND_IEM     },  // on entry, tell modem to send Instant report message
  {SJS_SEND_IEM,     SJS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,          SJS_IN_JOURNEY   },  // on Success, move to the IN_JOURNEY state, start 1 second timer for GNSS
  {SJS_SEND_IEM,     SJS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,          SJS_IN_JOURNEY   },

  {SJS_BLE_SCAN,     SJS_EV_ENTRY,    NULL,                         SeqJourneyBleScan,       0,          SJS_BLE_SCAN     },  // on entry, start a BLE scan
  {SJS_BLE_SCAN,     SJS_EV_SIGNAL,   (void *) SM_INSTANT_REPORT,   SeqJourneyBleStop,       0,          SJS_SEND_IEM     },  // stop ble scan and send IRM
  {SJS_BLE_SCAN,     SJS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,          SJS_WIFI_SCAN    },  // on success, move to the WIFI_SCAN state
  {SJS_BLE_SCAN,     SJS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,          SJS_IN_JOURNEY   },  // on error, return to IN_JOURNEY state

  {SJS_WIFI_SCAN,    SJS_EV_ENTRY,    NULL,                         SeqJourneyWifiScan,      0,          SJS_WIFI_SCAN    },  // on entry, start a WIFI_SCAN, and stay here
  {SJS_WIFI_SCAN,    SJS_EV_SIGNAL,   (void *) SM_TIMER_WIFI_STOP,  NULL,                    0,          SJS_WIFI_STOP    },  // on WIFI_STOP timer, move to the SJS_WIFI_STOP state
  {SJS_WIFI_SCAN,    SJS_EV_SIGNAL,   (void *) SM_INSTANT_REPORT,   SeqJourneyWifiStop,      0,          SJS_SEND_IEM     },  // stop wifi scan, go to send im (edited)
  {SJS_WIFI_SCAN,    SJS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,          SJS_IN_JOURNEY   },  // on error, return to IN_JOURNEY state
  {SJS_WIFI_SCAN,    SJS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,          SJS_IN_JOURNEY   },  // on error, return to IN_JOURNEY state

  {SJS_WIFI_STOP,    SJS_EV_ENTRY,    NULL,                         SeqJourneyWifiStop,      0,          SJS_IN_JOURNEY   },  // on entry, stop the WIFI and go back to IN_JOURNEY

  {SJS_GET_POSITION, SJS_EV_ENTRY,    NULL,                         SeqJourneyGetPosition,   0,          SJS_GET_POSITION },  // on entry, request position from GNSS
  {SJS_GET_POSITION, SJS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,          SJS_IN_JOURNEY   },  // on success, return to IN_JOURNEY state
  {SJS_GET_POSITION, SJS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,          SJS_IN_JOURNEY   },  // on error, return to IN_JOURNEY state. Maybe restart the GNSS?

  {SJS_SEND_DJM,     SJS_EV_ENTRY,    NULL,                         SeqJourneySendPJM,       5,          SJS_SEND_DJM     },  // on entry, send PJM message to cloud
  {SJS_SEND_DJM,     SJS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,          SJS_IN_JOURNEY   },  // on success, return to IN_JOURNEY state
  {SJS_SEND_DJM,     SJS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,          SJS_IN_JOURNEY   },

  {SJS_STOP_GNSS,    SJS_EV_ENTRY,    NULL,                         SeqJourneyStopGNSS,      2,          SJS_STOP_GNSS    },  // on entry, tell modem to stop GNSS
  {SJS_STOP_GNSS,    SJS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,          SJS_SEND_EOJ     },  // on Success, move to the SEND_EOJ state
  {SJS_STOP_GNSS,    SJS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,          SJS_SEND_EOJ     },

  {SJS_SEND_EOJ,     SJS_EV_ENTRY,    NULL,                         SeqJourneySendEOJ,       5,          SJS_SEND_EOJ     },  // on entry, send EOJ message to cloud
  {SJS_SEND_EOJ,     SJS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,          SJS_CHECK_SMS    },  // on error, re-send EOJ message to cloud (max 5 attempts)
  {SJS_SEND_EOJ,     SJS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,          SJS_CHECK_SMS    },

  {SJS_CHECK_SMS,    SJS_EV_ENTRY,    NULL,                         SeqJourneyCheckSMS,      0,          SJS_CHECK_SMS    },  // on entry, tell modem to check for received SMS messages
  {SJS_CHECK_SMS,    SJS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,          SJS_POWER_OFF    },  // on success, move to the SJS_IDLE state
  {SJS_CHECK_SMS,    SJS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,          SJS_POWER_OFF    },

  {SJS_POWER_OFF,    SJS_EV_ENTRY,    NULL,                         SeqJourneyPowerOff,      0,          SJS_POWER_OFF    },  // on entry, tell modem to power down
  {SJS_POWER_OFF,    SJS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,          SJS_IDLE         },  // on success, move to the SHS_IDLE state
  {SJS_POWER_OFF,    SJS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,          SJS_IDLE         },

  {SJS_ANY,          SJS_EV_SIGNAL,   (void *) SM_END_OF_JOURNEY,   SeqJourneyStopTimers,    0,          SJS_STOP_GNSS    },  // on EOJ event, move to SEND_EOJ state
  {SJS_ANY,          SJS_EV_SIGNAL,   (void *) SM_RESUME,           NULL,                    0,          SJS_IN_JOURNEY   },  // on resume, continue journey
  /* the table must be terminated with a null entry */
  {SJS_NONE,         SJS_EV_NONE,     NULL,                         NULL,                    0,          SJS_NONE         },
};


/**
 **************************************************************************************************
 *                                     PUBLIC FUNCTIONS / API
 **************************************************************************************************
 */

/*
 * @brief     One time initialisation of the Journey sequence module
 * @param     None
 * @return    None
 */
void SeqJourneyInit (void)
{
  static bool FirstCall = true;

  if (FirstCall)
  {
    FirstCall = false;
    /* Create timer to trigger in-journey events such as getting GNSS data and sending Device Journey messages */
    SjTimer = xTimerCreate("SjTimer", SJ_TIMER_PERIOD_MS, pdTRUE, (void *)NULL, SeqJourneyTimerCallback);

    /* set the initial state */
    CurrentState = SJS_IDLE;
  }
}


/*
 * @brief     Handle messages from other modules (IMU, ButtonMonitor, Modem etc)
 * @param     message  the event message received by the sequencer
 * @return    true if sequencer is idle, false otherwise
 */
bool SeqJourneySM (SequencerMessage_t message)
{
  switch (message.msgId)
  {
  /* INTERRUPT and ABORT messages are processed directly */
  case SM_INTERRUPT:
    if (JourneyInProgress)
    {
      SeqJourneyInterrupt();
      break;
    }
    /* Deliberate fall-through.  If journey not started we abort instead */
  case SM_ABORT:
    SeqJourneyAbort();
    break;
  case SM_RESUME:
    SeqJourneyResume();
    /* signal the state table for its SM_RESUME process */
    SJ_EVENT(SJS_EV_SIGNAL, message.msgId);
    break;
  default:
    /* All other messages are processed by the state machine */
    SJ_EVENT(SJS_EV_SIGNAL, message.msgId);
  }
  return (SJS_IDLE == CurrentState);
}


/**
 * @brief     Get the SOJ or EOJ GNSS data from SeqJourney
 * @param     isSoj: true for SOJ, false for EOJ
 * @return    SOJ or EOJ GNSS Data
 */
void * SeqJourneyGetGNSSData (bool isSoj)
{
  if (isSoj)
  {
    return ((void*) &CurrentTrip.SojData);
  }
  else
  {
    return ((void*) &CurrentTrip.EojData);
  }
}

/**
 * @brief     Get uniquely identify of this journey.
 *            The timestamp at journey start is used as a unique ID
 * @param     None
 * @return    the unique journey ID of the current trip
 */
uint32_t SeqJourneyGetJourneyId (void)
{
  return CurrentTrip.JourneyId;
}


/*
 * @brief     Return if have Journey started before
 * @param
 * @return    true if sequencer is started before, false otherwise
 */
bool SeqJourneyInProgress (void)
{
  return JourneyInProgress;
}


/*
 * @brief     Check in journey state
 * @param
 * @return    true if it is in SJS_IN_JOURNEY
 */
bool  IsSeqInJourney  (void)
{
  return (CurrentState == SJS_IN_JOURNEY);
}


/*
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */
/**
 * @brief     The sequencer logic for the Journey state.  Processes incoming events using the
 *            journey sequencer table defined above
 * @param     event - the sequencer event to process
 * @return    None
 * @Warning   Will be called recursively when changing state.  Must be re-entrant
 */
static void SeqJourneySequencer (SjEvent_t event)
{
  int index = 0;

  /* scan through the sequencer table */
  while (SJS_NONE != SjStateTable[index].State)
  {
    /* check if "current state" and "event" both match */
    if ( ((CurrentState == SjStateTable[index].State) || (SJS_ANY == SjStateTable[index].State))
      && (event.EventType == SjStateTable[index].Event))
    {
      /* if the event type is signal, see if it matches */
      if ( (SJS_EV_SIGNAL != event.EventType)
        || ((SJS_EV_SIGNAL == event.EventType) && (event.Parameter == SjStateTable[index].EventParam)) )
      {
        /* take the actions specified in the table */
        if (NULL != SjStateTable[index].pActionFunc)
        {
          /* a function is specified, so call it */
          SjStateTable[index].pActionFunc();
        }
        if ((SjStateTable[index].NextState != CurrentState) && (SjStateTable[index].NextState != SJS_NONE))
        {
          /* a state transition is specified so change state now */
          TRACE_SEQ("SJ %d->%d\n\r", CurrentState, SjStateTable[index].NextState);
          SJ_TRAN(SjStateTable[index].NextState);
        }
        break;
      }
    }
    index++;
  }
}


/**
 * @brief     Ask the modem to power up
 * @param     None
 * @return    None
 */
static void SeqJourneyPowerOn (void)
{
  TRACE_SEQ("Power up modem\n\r");
  ModemPowerUp();
}


/**
 * @brief     Ask the modem to power down
 * @param     None
 * @return    None
 */
static void SeqJourneyPowerOff (void)
{
  TRACE_SEQ("Power down modem\n\r");
  ModemPowerDown();
}


/**
 * @brief     Ask the modem to power down just the GSM section (GNSS remains on)
 * @param     None
 * @return    None
 */
static void SeqJourneyGsmOff (void)
{
  if (isModemSupportGSMOffMode())
  {
    TRACE_SEQ("Power down GSM (GNSS remains on)\n\r");
    ModemPowerDownGsm();
  }
}


/**
 * @brief     Ask the modem to start the GNSS service
 * @param     None
 * @return    None
 */
static void SeqJourneyStartGNSS (void)
{
  TRACE_SEQ("Start GNSS\n\r");

  /*
   * There should not be any old GNSS data in the event store,
   * but we reset the store just in case
   */
  GnssEventReset();

  /* start the GNSS receiver */
  ModemGnssStart();
}


/**
 * @brief     Ask the modem to stop the GNSS service
 * @param     None
 * @return    None
 */
static void SeqJourneyStopGNSS (void)
{
  TRACE_SEQ("Stop GNSS\n\r");
  ModemGnssStop();
}


/**
 * @brief     Get our initial GNSS position from the Modem
 * @param     None
 * @return    None
 */
static void SeqJourneyGetInitialFix (void)
{
  TRACE_SEQ("Get GNSS data\n\r");
  ModemGnssGetData(false);
}


/**
 * @brief     Check that the RTC has a valid timestamp
 *            Seqjourney only log journey data and send journey messages
 *            if RTC time is valid (GNSS/NTP fix or Console set before)
 * @param     None
 * @return    None
 */
static void SeqJourneyValidateRtc (void)
{
  if (RtcTimeValid())
  {
    /* RTC clock data is valid, Journey ready to send out */
    SequencerEnqueueMessage(SM_JOURNEY_SEND_RDY, NULL);
  }
  else
  {
    TRACE_SEQ("No valid RTC synced, Loop on GNSS Init fix\n\r");
    SeqJourneyGetInitialFix();
  }
}


/**
 * @brief     Build the Journey ID of this trip
 * @param     None
 * @return    None
 */
static void SeqJourneyCreateTripId (void)
{
  TRACE_SEQ("Build the Journey ID\n\r");
  
  /* A new Joruney, memset the Journey Trip info */
  memset(&CurrentTrip, 0, sizeof(SeqJourneyTrip_t));
  CurrentTrip.JourneyId = RtcGetTimeStamp();
}


/**
 * @brief     Mark the GNSS data for the Start of Journey
 * @param     None
 * @return    None
 */
static void SeqJourneyMarkSojData (void)
{
  TRACE_SEQ("SOJ GNSS data marked\n\r");
  CurrentTrip.SojData = ModemStatus.GnssData;
}


/**
 * @brief     Mark the GNSS data for the End of Journey
 * @param     None
 * @return    None
 */
static void SeqJourneyMarkEojData (void)
{
  TRACE_SEQ("EOJ GNSS data marked\n\r");
  CurrentTrip.EojData = ModemStatus.GnssData;
}


/**
 * @brief     Get our current GNSS position from the Modem
 * @param     None
 * @return    None
 */
static void SeqJourneyGetPosition (void)
{
  TRACE_SEQ("Get / store GNSS data\n\r");
  ModemGnssGetData(true);
}


/**
 * @brief     Start a 1 second timer to trigger "in-journey" activity:
 *              1, once a second, request GNSS position
 *              2, periodically, send a DJM message to the cloud
 * @param     None
 * @return    None
 */
static void SeqJourneyStartTimers (void)
{
  /* it's the start of a new journey, so clear the DJM time counter */
  CurrentTrip.JourneyTime = 0;
  CurrentTrip.SecondsSinceLastDjm = 0;
  CurrentTrip.JourneyWifiScanStartTime = 0;

  /* start the timer */
  xTimerStart(SjTimer, 0);
}


/**
 * @brief     Stop the "in-journey" timer
 * @param     None
 * @return    None
 */
static void SeqJourneyStopTimers (void)
{
  xTimerStop(SjTimer, 0);

  /* Stop the Wifi Moudle if started before */
  if (CurrentTrip.JourneyWifiScanStartTime)
  {
    WifiStop();
  }
  SeqJourneyMarkEojData();
}


/**
 * @brief     Ask the modem to send a "Start of Journey" message to the cloud
 * @param     None
 * @return    None
 */
static void SeqJourneySendSOJ (void)
{
  /* Build the Start of Journey Message */
  MessageEnqueueJourneyMessage(MESSAGE_TYPE_START_OF_JOURNEY);

  TRACE_SEQ("Send Start of Journey Message to cloud\n\r");
  JourneyInProgress = true;

  /* Ask the modem to send queued messages */
  ModemDefaultSendQueued(MESSAGE_PRIORITY_LOW);
}


/**
 * @brief     Ask the modem to send a "Periodic Journey" message to the cloud
 * @param     None
 * @return    None
 */
static void SeqJourneySendPJM (void)
{
  /* Build the Periodic Journey Message */
  MessageEnqueueJourneyMessage(MESSAGE_TYPE_PERIODIC_JOURNEY);

  TRACE_SEQ("Send Periodic Journey Messages to cloud\n\r");

  /* Ask the modem to send queued messages */
  ModemDefaultSendQueued(MESSAGE_PRIORITY_LOW);
}


/**
 * @brief     Ask the modem to send a instant event message to the cloud
 * @param     None
 * @return    None
 */
static void SeqInstantSendIEM (void)
{
  /* Build the Instant Event Message */
  MessageEnqueueJourneyMessage(MESSAGE_TYPE_STATUS);

  TRACE_SEQ("Report events to cloud during the journey\r\n");

  /* Ask the modem to send queued messages */
  ModemDefaultSendQueued(MESSAGE_PRIORITY_LOW);
}


/**
 * @brief     Ask the modem to send a "End of Journey" message to the cloud
 * @param     None
 * @return    None
 */
static void SeqJourneySendEOJ (void)
{
  /* Add any remaining journey data as Periodic Journey Messages */
  MessageEnqueueJourneyMessage(MESSAGE_TYPE_PERIODIC_JOURNEY);

  /* Add the End of Journey Message */
  MessageEnqueueJourneyMessage(MESSAGE_TYPE_END_OF_JOURNEY);

  /* Optionally queue any mini-events (if enabled in config store) */
  if (true == ConfigGetFlag(CONFIG_FLAG_SEND_MEV))
  {
    MessageEnqueueMem();
  }

  /* Add a Status Message to the queue */
  MessageEnqueueDsm();

  TRACE_SEQ("Send End of Journey Messages to cloud\n\r");
  JourneyInProgress = false;

  /* Ask the modem to send queued messages */
  ModemDefaultSendQueued(MESSAGE_PRIORITY_LOW);
}


/**
 * @brief     Check for received SMS messages (e.g. configuration changes etc)
 * @param     None
 * @return    None
 */
static void SeqJourneyCheckSMS (void)
{
  TRACE_SEQ("Check for received SMS\n\r");
  ModemSmsReceive();
}


/**
 * @brief     Start a BLE scan for nearby MAC addresses
 * @param     None
 * @return    None
 */
static void SeqJourneyBleScan (void)
{
  TRACE_SEQ("Start BLE scan\n\r");
  CurrentTrip.RadioScanStarted = true;
  ModemBleScan();
}


/**
 * @brief     Stop a BLE scan for nearby MAC addresses
 * @param     None
 * @return    None
 */
static void SeqJourneyBleStop (void)
{
  TRACE_SEQ("Stop BLE scan\n\r");
  GsmStopBleScan();
}


/**
 * @brief     Start Gnss Jammer detection
 * @param     None
 * @return    None
 */
static void SeqJourneyJamStart (void)
{
  sleep(1500);  /* Jamming detection  function access need at least 3s after gnss is on, a further 1500ms required here */
}


/**
 * @brief     Enable Gnss Jammer detection
 * @param     None
 * @return    None
 */
static void SeqJourneyJamScanOn (void)
{
  TRACE_SEQ("Set jamming detect\n\r");
  ModemGnssJamControl(true);
}


/**
 * @brief     Start a WiFi scan for nearby MAC addresses
 * @param     None
 * @return    None
 */
static void SeqJourneyWifiScan (void)
{
  WifiStart();
  CurrentTrip.JourneyWifiScanStartTime = CurrentTrip.JourneyTime;
}


/**
 * @brief     Stop a WiFi scan for nearby MAC addresses
 * @param     None
 * @return    None
 */
static void SeqJourneyWifiStop (void)
{
  WifiStop();

  /*
   * Add radio scan data to message queue.
   * NOTE that we do not send it now, but queue it for sending later
   */
  MessageEnqueueRsm();
}



/**
 * @brief     Abort all activity and return to the IDLE state.  This will happen on impact.
 * @param     None
 * @return    None
 */
static void SeqJourneyAbort (void)
{
  TRACE_SEQ("SJ !! ABORT !!\n\r");

  if (JourneyInProgress)   /* make sure not to send Eoj again */
  {
    /* Add any remaining journey data as Periodic Journey Messages */
    MessageEnqueueJourneyMessage(MESSAGE_TYPE_PERIODIC_JOURNEY);

    /* Add the End of Journey Message */
    MessageEnqueueJourneyMessage(MESSAGE_TYPE_END_OF_JOURNEY);

    /* stop timers if they are running */
    SeqJourneyStopTimers();
  }

  /* tell the Network Module (Modem/Wifi) to stop whatever it is doing */
  SequencerNetworkAbort();

  /* Stop GNSS scanning */
  SeqJourneyStopGNSS();

  /* change back to the idle state */
  CurrentState = SJS_IDLE;

  /* Journey Abort, reset Started flag */
  JourneyInProgress = false;
}


/**
 * @brief     Interrupt requested. This will happen on red-button event.
 *            Abort the Network activity and stop the timers.
 *            Add any remaining journey data as Periodic Journey Messages
 *            Keep the current state for later resume
 * @param     None
 * @return    None
 */
static void SeqJourneyInterrupt (void)
{
  TRACE_SEQ("SJ !! Interrupt [%d]!!\n\r", CurrentState);

  /* stop timers if they are running */
  SeqJourneyStopTimers();

  /* Add any remaining journey data as Periodic Journey Messages */
  MessageEnqueueJourneyMessage(MESSAGE_TYPE_PERIODIC_JOURNEY);

  /* tell the Modem and Wifi to stop whatever they are doing */
  SequencerNetworkAbort();

  /*
   * Notify the Modem Module to retain the power after the
   * emergency sequence finishes. SeqJourney still need the modem
   * when the journey resumes.
   */
  ModemMarkPowerRetainNeeded(true);
}


/**
 * @brief     Resume the last state, power on the modem if needed
 * @param     None
 * @return    None
 */
static void SeqJourneyResume (void)
{
  TRACE_SEQ("SJ !! Resumed [%d] !!\n\r", CurrentState);

  /* Release the flag such that request of Power Down of Modem is allowed */
  ModemMarkPowerRetainNeeded(false);

  /* Resume the timer */
  xTimerStart(SjTimer, 0);

  if (MotionMonitorGetState() == ENDING_JOURNEY )
  {
    /* We missed the EoJ while processing the Red Button event, so repost it here */
    SequencerEndOfJourney();
  }
}


/**
 * @brief     Tell the IMU we have finished processing the end of journey sequence
 * @param     None
 * @return    None
 */
static void SeqJourneyComplete (void)
{
  MotionMonitorEojSequenceFinished();
}

/**
 * @brief     Check if the device is qualified for the radio scan.
 * @param     Pointer to the current trip.
 * @return    True if it is qualified to perform the radio scan;
 *            false if the device is not yet qualified for the radio scan.
 *            It is defined by the following criteria:
 *            1. 2 minutes after journey start, AND
 *            2. xx kph (adjustable in config parameters).
 */
static bool SeqQualifyForRadioScan(SeqJourneyTrip_t* currentTrip)
{
  bool ret = false;
  uint32_t radioScanSpeed = 0;

  if (!currentTrip)
  {
    /* sanity check for the input parameter*/
    return false;
  }

  radioScanSpeed = ConfigGetParam(CONFIG_PARAM_RADIO_SCAN_SPEED);

  ret = (ConfigGetFlag(CONFIG_FLAG_RADIO_SCAN)      /* System configed need radio scan */
        && !(currentTrip->RadioScanStarted)      /* No radio scan ever started */
        && (currentTrip->JourneyTime > SCAN_TIME_SECONDS)   /* If the Journey started more than SCAN_TIME_SECONDS */
        && (radioScanSpeed) && (ModemStatus.GnssData.Speed >= radioScanSpeed) /* If the speed of device is above threshold */
  );

  return ret;

}


/**
 * @brief     In-journey timer callback
 * @param     timerHandle - The timer Handle
 * @return    None
 * @Warning   We cannot call the sequencer directly from here (the timer interrupt)
 *            instead we must queue a message for the modem task
 */
static void SeqJourneyTimerCallback (xTimerHandle timerHandle)
{
  CurrentTrip.JourneyTime++;
  CurrentTrip.SecondsSinceLastDjm++;

  /* check if it is time to perform a radio scan */
  if (SeqQualifyForRadioScan(&CurrentTrip))
  {
    /* Send radio scan timer event to the sequencer */
    SequencerEnqueueMessage(SM_TIMER_SCAN, NULL);
  }

  if (CurrentTrip.JourneyWifiScanStartTime) // Wifi Scan started at sometime
  {
    if ((CurrentTrip.JourneyTime - CurrentTrip.JourneyWifiScanStartTime) >= WIFI_SCAN_DURATION_SECS)
    {
      CurrentTrip.JourneyWifiScanStartTime = 0;
      /* Send radio scan timer event to the sequencer */
      SequencerEnqueueMessage(SM_TIMER_WIFI_STOP, NULL);
    }
  }

  /* check if it is time to send a DJM message to the cloud */
  if (DJM_PERIOD_SECONDS <= CurrentTrip.SecondsSinceLastDjm)
  {
    /* it is time to send the next DJM message */
    CurrentTrip.SecondsSinceLastDjm = 0;

    /* Send DJM timer event to the sequencer */
    SequencerEnqueueMessage(SM_TIMER_DJM, NULL);
  }

#if !MODEM_SEPARATE_GNSS_UART
  else
  {
    /* Send GNSS timer event to the sequencer */
    SequencerEnqueueMessage(SM_TIMER_GNSS, NULL);
  }
#endif
}

