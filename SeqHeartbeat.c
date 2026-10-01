/*
 **************************************************************************************************
 * @file   SeqHeartbeat.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   2020-04-16
 *
 * @brief  Event sequencer for the "heartbeat" state.  Handles the sequence of modem events that
 *         are performed when the vehicle is parked and it is time to send a heartbeat message.
 *         These events are:
 *           1. Power up the modem
 *           2. Start GNSS and wait for fix (or timeout)
 *           3. Send the heartbeat message to the cloud
 *           4. Check for any received SMS message and process accordingly
 *           5. Power down the modem
 *
 **************************************************************************************************
 */

#include "global.h"


/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */
/* Timer period used for the timer */
#define SH_TIMER_PERIOD_MS   (1000)

/*
 * Minimum ON time for the modem.
 * Used to make sure modem is on long enough to receive waiting SMS from the network
 * NOTE: We might want to modify this so we only power up this long once per day, rather than every hearbeat
 */
#define MODEM_ON_TIME_SECONDS  (90)

#define HEARTBEAT_FAILED_TIMEOUT_EXTEND_COUNT      (4)
#define HEARTBEAT_SMS_TIMEOUT_IN_SEC               (24*60*60)

/* HeartBeat related bit for the lowpower support  */
#define SMS_SENT_BIT           (1 << 7)          /* SMS sent bit */
#define SMS_NEED_SEND_BIT      (1 << 6)          /* SMS need-to-send status bit */
#define MAX_ATTEMPT_COUNT      0x0F              /* Lower 4 bits for connection attempt count */

/* List of heartbeat sequencer states.  Used in conjunction with the sequencer state table */
typedef enum
{
  SHS_NONE,
  SHS_IDLE,
  SHS_POWER_ON,
  SHS_START_GNSS,
  SHS_GET_POSITION,
  SHS_STOP_GNSS,
  SHS_SEND_DHM,
  SHS_SEND_SMS,
  SHS_CHECK_SMS,
  SHS_FW_CHECK,
  SHS_FW_DOWNLOAD,
  SHS_FW_UPDATE,
  SHS_FW_VERIFY,
  SHS_POWER_OFF,
  SHS_ANY
} eSeqHeartbeatState;

/* Event types that can be sent to the sequencer */
typedef enum
{
  SHS_EV_NONE,
  SHS_EV_ENTRY,
  SHS_EV_EXIT,
  SHS_EV_SIGNAL,
} eSeqEvent;

/* Type of each entry in the sequencer state table */
typedef struct
{
  eSeqHeartbeatState   State;
  eSeqEvent            Event;
  void *               EventParam;
  void                 (*pActionFunc)(void);
  int                  NoOfRetries;
  eSeqHeartbeatState   NextState;
} StateTable_t;

/* Event type used by the sequencer.  Some events may include a parameter */
typedef struct
{
  eSeqEvent   EventType;
  void *      Parameter;
} Event_t;

/* macro for generating a sequencer event */
#define SH_EVENT(_ev, _par)       \
{                                 \
  Event_t Event;                  \
  Event.EventType = _ev;          \
  Event.Parameter = (void *)_par; \
  SeqHeartbeatSequencer(Event);   \
}


/* macro to transition from one state to the next */
#define SH_TRAN(_st)              \
{                                 \
  Event_t Event;                  \
  Event.Parameter = NULL;         \
  Event.EventType = SHS_EV_EXIT;  \
  SeqHeartbeatSequencer(Event);   \
  CurrentState = _st;             \
  Event.EventType = SHS_EV_ENTRY; \
  SeqHeartbeatSequencer(Event);   \
}


/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static void SeqHeartbeatSequencer     (Event_t event);
static void SeqHeartbeatCheckSMS      (void);
static void SeqHeartbeatStartGNSS     (void);
static void SeqHeartbeatGetPosition   (void);
static void SeqHeartbeatStopGNSS      (void);
static void SeqHeartbeatSendDHM       (void);
static void SeqHBSendDHMFailed        (void);
static void SeqHeartbeatAbort         (void);
static void SeqHeartbeatComplete      (void);
static void SeqHeartbeatPowerOn       (void);
static void SeqHeartbeatPowerOff      (void);
static void SeqHeartbeatFwCheck       (void);
static void SeqHeartbeatFwDownload    (void);
static void SeqHeartbeatFwVerify      (void);
static void SeqHeartbeatStartTimer    (void);
static void SeqHeartbeatStopTimer     (void);
static void SeqHeartbeatSendSms       (void);
static void SeqHeartbeatSendSmsOK     (void);
static void SeqHeartbeatTimerCallback (xTimerHandle timerHandle);


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
/* Timer used to trigger timed "heartbeat" events such making sure the modem is powered up long enough to receive SMS */
static TimerHandle_t  ShTimer;

/* Counter to keep track of time since the modem was powered on */
static uint32_t SecondsSincePowerOn = 0;

/* The current state of this sequencer */
static eSeqHeartbeatState CurrentState = SHS_IDLE;

/* Housekeeping parameters for the ITS-121 HeartBeat will extend the timeout and construct SMS when there is 24 hours outage */
static uint8_t seqHBAttempted = 0;            /* The count for the Heartbeat attempted */
static bool isSMSNeedToBeSent = false;        /* The flag to indicate if SMS need to be sent */
static bool isSMSSent = false;                /* The flag to indicate if SMS has been sent */

/*
 * Table to define the sequence of actions to be carried out in the "heart beat" sequence
 */
static StateTable_t StateTable[] =
{
  /* STATE           EVENT            EVENT_PARAM                   ACTION FUNCTION          RETRIES     NEXT STATE     */
  {SHS_IDLE,         SHS_EV_ENTRY,    NULL,                         SeqHeartbeatComplete,    0,          SHS_IDLE         },  // on entry, indicate that we have completed the sequence

  {SHS_ANY,          SHS_EV_SIGNAL,   (void *) SM_HEARTBEAT,        NULL,                    0,          SHS_POWER_ON     },  // on Heartbeat event, change to SHS_POWER_ON state

  {SHS_POWER_ON,     SHS_EV_ENTRY,    NULL,                         SeqHeartbeatPowerOn,     0,          SHS_POWER_ON     },  // on entry, tell modem to power up
  {SHS_POWER_ON,     SHS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   SeqHeartbeatStartTimer,  0,          SHS_START_GNSS   },  // on success, move to the SHS_START_GNSS state
  {SHS_POWER_ON,     SHS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,          SHS_IDLE         },

  {SHS_START_GNSS,   SHS_EV_ENTRY,    NULL,                         SeqHeartbeatStartGNSS,   2,          SHS_START_GNSS   },  // on entry, tell modem to start GNSS
  {SHS_START_GNSS,   SHS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,          SHS_GET_POSITION },  // on success, move to the SHS_GET_POSITION state
  {SHS_START_GNSS,   SHS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,          SHS_STOP_GNSS    },

  {SHS_GET_POSITION, SHS_EV_ENTRY,    NULL,                         SeqHeartbeatGetPosition, 0,          SHS_GET_POSITION },  // on entry, request position from GNSS
  {SHS_GET_POSITION, SHS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,          SHS_STOP_GNSS    },  // on success, move to the SHS_STOP_GNSS state
  {SHS_GET_POSITION, SHS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,          SHS_STOP_GNSS    },

  {SHS_STOP_GNSS,    SHS_EV_ENTRY,    NULL,                         SeqHeartbeatStopGNSS,    2,          SHS_STOP_GNSS    },  // on entry, tell modem to stop GNSS
  {SHS_STOP_GNSS,    SHS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,          SHS_SEND_DHM     },  // on success, move to the SHS_SEND_DHM state
  {SHS_STOP_GNSS,    SHS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,          SHS_SEND_DHM     },

  {SHS_SEND_DHM,     SHS_EV_ENTRY,    NULL,                         SeqHeartbeatSendDHM,     5,          SHS_SEND_DHM     },  // on entry, send DHM message to cloud
  {SHS_SEND_DHM,     SHS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,          SHS_SEND_SMS     },  // on success, return to SHS_SEND_SMS state
  {SHS_SEND_DHM,     SHS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      SeqHBSendDHMFailed,      0,          SHS_SEND_SMS     },  // on failure, increment the seqHBAttempted

  {SHS_SEND_SMS,     SHS_EV_ENTRY,    NULL,                         SeqHeartbeatSendSms,     5,          SHS_SEND_SMS     },  // on entry, Send SMS if seqHBAttempted > GPRS_REGISTER_TIMEOUT_REPORTED_COUNT
  {SHS_SEND_SMS,     SHS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   SeqHeartbeatSendSmsOK,   0,          SHS_CHECK_SMS    },  /// on success, mark isSMSSent as true, move to SHS_CHECK_SMS
  {SHS_SEND_SMS,     SHS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,          SHS_CHECK_SMS    },

  {SHS_CHECK_SMS,    SHS_EV_ENTRY,    NULL,                         SeqHeartbeatCheckSMS,    0,          SHS_CHECK_SMS    },  // on entry, tell modem to check for received SMS
  {SHS_CHECK_SMS,    SHS_EV_SIGNAL,   (void *) SM_TIMER_MODEM_OFF,  SeqHeartbeatCheckSMS,    0,          SHS_CHECK_SMS    },  // on timer, check for received SMS
  {SHS_CHECK_SMS,    SHS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,          SHS_FW_CHECK     },  // on success, move to the SHS_FW_CHECK state
  {SHS_CHECK_SMS,    SHS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,          SHS_FW_CHECK     },

  {SHS_FW_CHECK,     SHS_EV_ENTRY,    NULL,                         SeqHeartbeatFwCheck,     0,          SHS_FW_CHECK     },  // on entry, check for firmware update
  {SHS_FW_CHECK,     SHS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,          SHS_FW_DOWNLOAD  },  // on success, move to the SHS_FW_DOWNLOAD state
  {SHS_FW_CHECK,     SHS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,          SHS_POWER_OFF    },

  {SHS_FW_DOWNLOAD,  SHS_EV_ENTRY,    NULL,                         SeqHeartbeatFwDownload,  0,          SHS_FW_DOWNLOAD  },  // on entry, download the firmware
  {SHS_FW_DOWNLOAD,  SHS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,          SHS_FW_VERIFY    },  // on success, move to the SHS_FW_VERIFY state
  {SHS_FW_DOWNLOAD,  SHS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,          SHS_POWER_OFF    },

  {SHS_FW_VERIFY,    SHS_EV_ENTRY,    NULL,                         SeqHeartbeatFwVerify,    0,          SHS_FW_VERIFY    },  // on entry, Verify the Firmware
  {SHS_FW_VERIFY,    SHS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,          SHS_POWER_OFF    },  // on success, move to the SHS_POWER_OFF state
  {SHS_FW_VERIFY,    SHS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,          SHS_POWER_OFF    },

  {SHS_POWER_OFF,    SHS_EV_ENTRY,    NULL,                         SeqHeartbeatPowerOff,    0,          SHS_POWER_OFF    },  // on entry, tell modem to power down
  {SHS_POWER_OFF,    SHS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,          SHS_IDLE         },  // on success, move to the SHS_IDLE state
  {SHS_POWER_OFF,    SHS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,          SHS_IDLE         },

  /* the table must be terminated with a null entry */
  {SHS_NONE,         SHS_EV_NONE,     NULL,                         NULL,                    0,          SHS_NONE         },
};


/**
 **************************************************************************************************
 *                                     PUBLIC FUNCTIONS / API
 **************************************************************************************************
 */
/*
 * @brief     One time initialisation of the Heartbeat sequence module
 * @param     None
 * @return    None
 */
void SeqHeartbeatInit (void)
{
  static bool FirstCall = true;

  if (FirstCall)
  {
    FirstCall = false;

    /* Create timer used to make sure modem is powered long enough to receive SMS */
    ShTimer = xTimerCreate("ShTimer", SH_TIMER_PERIOD_MS, pdTRUE, (void *)NULL, SeqHeartbeatTimerCallback);

    /* set the initial state */
    CurrentState = SHS_IDLE;
  }
}



/*
 * @brief     Handle messages from other modules (IMU, ButtonMonitor, Modem etc)
 * @param     message  the event message received by the sequencer
 * @return    true if sequencer is idle, false otherwise
 */
bool SeqHeartbeatSM (SequencerMessage_t message)
{
  switch (message.msgId)
  {
  case SM_ABORT:
  case SM_INTERRUPT:
    /* if this is an ABORT/INTERRUPT message, we process it directly */
    SeqHeartbeatAbort();
    break;
  default:
    SH_EVENT(SHS_EV_SIGNAL, message.msgId);
    break;
  }
  return (SHS_IDLE == CurrentState);
}

/**
 * @brief       Get the SeqHeartBeat HouseKeeping Bits
 * @param       none
 * @return      current GSM Connection Attempted tried
 */
uint8_t SeqHeartbeatGetHouseKeep (void)
{
  return (seqHBAttempted & MAX_ATTEMPT_COUNT) |              /* Lower 4 bits */
          (isSMSNeedToBeSent ? SMS_NEED_SEND_BIT : 0) |      /* SMS need-to-send bit */
          (isSMSSent ? SMS_SENT_BIT : 0);                    /* SMS sent bit */
}


/**
 * @brief       Restore the current SeqHeartBeat House Keep Bit from Low power module
 * @param       value - value from PSRAM restored from low power module
 * @return      none
 */
void LPRestoreHeartbeatHouseKeep (uint8_t value)
{
  seqHBAttempted = value & MAX_ATTEMPT_COUNT;                       /* Extract lower 4 bits (0–3) */
  isSMSNeedToBeSent = (value & SMS_NEED_SEND_BIT) ? true : false;   /* SMS need-to-send Bit */
  isSMSSent = (value & SMS_SENT_BIT) ? true : false;                /* SMS sent Bit */
}


/*
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */

/**
 * @brief     The sequencer logic for the Heart Beat state.  Processes incoming events using the
 *            journey sequencer table defined above
 * @param     event - the sequencer event to process
 * @return    None
 * @Warning   Will be called recursively when changing state.  Must be re-entrant
 */
static void SeqHeartbeatSequencer (Event_t event)
{
  int index = 0;

  /* scan through the sequencer table */
  while (SHS_NONE != StateTable[index].State)
  {
    /* check if "current state" and "event" both match */
    if ( ((CurrentState == StateTable[index].State) || (SHS_ANY == StateTable[index].State))
      && (event.EventType == StateTable[index].Event))
    {
      /* if the event type is signal, see if it matches */
      if ( (SHS_EV_SIGNAL != event.EventType)
        || ((SHS_EV_SIGNAL == event.EventType) && (event.Parameter == StateTable[index].EventParam)) )
      {
        /* take the actions specified in the table */
        if (NULL != StateTable[index].pActionFunc)
        {
          /* a function is specified, so call it */
          StateTable[index].pActionFunc();
        }
        if (StateTable[index].NextState != CurrentState)
        {
          /* a state transition is specified so change state now */
          TRACE_SEQ("SH %d->%d\n\r", CurrentState, StateTable[index].NextState);
          SH_TRAN(StateTable[index].NextState);
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
static void SeqHeartbeatPowerOn (void)
{
  TRACE_SEQ("Power up modem\n\r");
  ModemPowerUp();
}


/**
 * @brief     Ask the modem to power down
 * @param     None
 * @return    None
 */
static void SeqHeartbeatPowerOff (void)
{
  TRACE_SEQ("Power down modem\n\r");
  ModemPowerDown();
}


/**
 * @brief     Ask the modem to start the GNSS service
 * @param     None
 * @return    None
 */
static void SeqHeartbeatStartGNSS (void)
{
  TRACE_SEQ("Start GNSS\n\r");
  ModemGnssStart();
}


/**
 * @brief     Get our current GNSS position from the
 * @param     None
 * @return    None
 */
static void SeqHeartbeatGetPosition (void)
{
  TRACE_SEQ("Get GNSS data\n\r");
  ModemGnssGetData(false);
}


/**
 * @brief     Ask the modem to start the GNSS service
 * @param     None
 * @return    None
 */
static void SeqHeartbeatStopGNSS (void)
{
  TRACE_SEQ("Stop GNSS\n\r");
  ModemGnssStop();
}


/**
 * @brief     Ask the modem to send a "Device Heartbeat Message" message to the cloud
 * @param     None
 * @return    None
 */
static void SeqHeartbeatSendDHM (void)
{
  if ((seqHBAttempted + 1) == HEARTBEAT_FAILED_TIMEOUT_EXTEND_COUNT)
  {
    /* Extend the GsmGprs timeout to increase the chance for the HeartBeat Send */
    /* for the (HEARTBEAT_FAILED_TIMEOUT_EXTEND_COUNT)th of send                */
    GsmGprsSetExtraTimeout();

    /* Reset the counting */
    seqHBAttempted = 0;
  }

  /* Build the Device Heartbeat Message */
  MessageEnqueueDhm();

  TRACE_SEQ("Send Device Heartbeat Message to cloud\n\r");

  /* Ask the modem to send queued messages */
  ModemDefaultSendQueued(MESSAGE_PRIORITY_LOW);
}


/**
 * @brief     Handler when Send DHM is Failed, increase the fail count for later action
 * @param     None
 * @return    None
 */
static void SeqHBSendDHMFailed (void)
{  
  seqHBAttempted++;
}


/**
 * @brief     Start a timer used to make sure the modem is on long enought to receive SMS
 * @param     None
 * @return    None
 */
static void SeqHeartbeatStartTimer (void)
{
  /* it's the start of a new journey, so clear the DJM time counter */
  SecondsSincePowerOn = 0;

  /* start the timer */
  xTimerStart(ShTimer, 0);
}


/**
 * @brief     Stop the modem power on timer
 * @param     None
 * @return    None
 */
static void SeqHeartbeatStopTimer (void)
{
  xTimerStop(ShTimer, 0);
}


/**
 * @brief     (ITS121) Send a failed SMS during the DHM process if: 
 *              1. The last online time exceeds HEARTBEAT_SMS_TIMEOUT_IN_SEC. 
 *              2. No SMS has been sent for this non-transmitting event yet.
 *              (i.e., only one SMS per non-transmitting event will be sent.)
 *            
 *            If an SMS was sent or the device is back online, 
 *            isSMSNeedToBeSent will remain false to avoid duplicate SMS
 * 
 *            Once the SMS is sent and the device is online again,
 *            the flag isSMSSent will be reset to allow the next usage.
 * 
 *            ToDo: The operation of this function is to be reviewed and optimized.  

 * @param     None
 * @return    None
 */
static void SeqHeartbeatSendSms (void)
{
  uint8_t * pMessage = NULL;
  int msgSize;
  uint32_t timeSinceLastOnline = 0;
  uint32_t lastOnlineTime = GsmGprsGetLastOnLineTime();
  uint32_t currentTimeStamp = RtcGetTimeStamp();


  /* Only calculate the time difference if the online time is valid (i.e., not in the future) */
  if (lastOnlineTime < currentTimeStamp)
  {    
    timeSinceLastOnline = currentTimeStamp - lastOnlineTime;
  }

  /* Init with no SMS needed to send first */
  isSMSNeedToBeSent = false;

  /* There is more than 24 hours of offline event happened, and no SMS Send out yet */
  if( (timeSinceLastOnline > HEARTBEAT_SMS_TIMEOUT_IN_SEC) && (!isSMSSent))
  {
    TRACE_SEQ("Send Connection Attempt Failed SMS\n\r");

    /* Build the SMS GSM info message */
    MessageEncodeSmsGim(&pMessage, &msgSize);

    /* Flag up SeqHB need to send the SMS, it is needed when handling when Modem report OK */
    isSMSNeedToBeSent = true;

    /* Ask the modem to send it */
    ModemSmsSend((char*) pMessage);
  }
  else
  {
    /* No SMS is needed to be sent here (either within timeout period or SMS already sent) */    

    /* Reset the SMS sent status, if the last online timestamp is reset (online again) to allow the next usage */
    if (isSMSSent && (timeSinceLastOnline < HEARTBEAT_SMS_TIMEOUT_IN_SEC))
    {        
      isSMSSent = false;
    }

    /* send complete and carry on the sequencer */
    SequencerModemComplete();
    return;
  }
}


/**
 * @brief     Mark up the bit to indicate the SMS is sent successfully reported by Modem
 * @param     None
 * @return    None
 */
static void SeqHeartbeatSendSmsOK (void)
{  
  if (isSMSNeedToBeSent)
  {    
    /* Mark the SMS sent success if ever requested 
     * This flag is to stop HB repeatedly SMS for the same Offline timeout Event */
    isSMSSent = true;
  }
}


/**
 * @brief     Check for received SMS messages (e.g. configuration changes etc)
 * @param     None
 * @return    None
 */
static void SeqHeartbeatCheckSMS (void)
{
  if (SecondsSincePowerOn >= MODEM_ON_TIME_SECONDS)
  {
    TRACE_SEQ("Check for received SMS\n\r");
    SeqHeartbeatStopTimer();
    ModemSmsReceive();
  }
  else
  {
    TRACE_SEQ("Wait for modem ON time...\n\r");
  }
}


/**
 * @brief     Check for firmware update request.  Get the manifest file if required.
 * @param     None
 * @return    None
 */
static void SeqHeartbeatFwCheck (void)
{
  TRACE_SEQ("Check for firmware update\n\r");

  /* check if we have received a firmware update request */
  bool result = ConfigGetFlag(CONFIG_FLAG_FW_UPDATE);
  if (!result)
  {
    result = ConfigGetFlag(CONFIG_FLAG_OTA_BL_UPDATE);
  }
  /* check battery level */
  if ((result) && (OTA_BATTERY_MIN_PERCENT > BatteryMonitorGetLevel()))
  {
    TRACE_SEQ("Skipping firmware update - battery too low\n\r");
    result = false;
  }

  if (result)
  {
    /* update available - get the manifest file */
    TRACE_SEQ("Download firmware manifest\n\r");
    OtaHttpGetManifest();
  }
  else
  {
    /* No update available or battery too low - tell sequencer to skip the firmware update */
    SequencerEnqueueMessage(SM_MODEM_ERROR, NULL);
  }
}


/**
 * @brief     Download the new firmware image.
 * @param     None
 * @return    None
 */
static void SeqHeartbeatFwDownload (void)
{
  /* process the manifest file to see what image to download */
  bool result = OtaProcessManifest();

  if (result)
  {
    TRACE_SEQ("Download new firmware image\n\r");
    OtaHttpGetImageFile();
  }
  else
  {
    /* manifest processing error - tell sequencer to skip the firmware update */
    SequencerEnqueueMessage(SM_MODEM_ERROR, NULL);
  }
}


/**
 * @brief     Verify the new firmware image.
 * @param     None
 * @return    None
 */
static void SeqHeartbeatFwVerify (void)
{
  TRACE_SEQ("Verify new firmware image\n\r");

  /* check if we have received a firmware update request */
  if (OtaVerify())
  {
    OtaProgram();  /* no returns */
  }
  else
  {
    /* Nothing to do - tell sequencer to skip the firmware update */
    SequencerEnqueueMessage(SM_MODEM_ERROR, NULL);
  }
}


/**
 * @brief     Abort all activity and return to the IDLE state
 * @param     None
 * @return    None
 */
static void SeqHeartbeatAbort (void)
{
  TRACE_SEQ("SH !! ABORT !!\n\r");

  /* stop timers if they are running */
  SeqHeartbeatStopTimer();

  /* tell the Network Module (Modem/Wifi) to stop whatever it is doing */
  SequencerNetworkAbort();

  /* change back to the idle state */
  CurrentState = SHS_IDLE;
}


/**
 * @brief     Tell the IMU we have finished processing the heartbeat
 * @param     None
 * @return    None
 */
static void SeqHeartbeatComplete (void)
{
  MotionMonitorDhmSequenceFinished();
}


/**
 * @brief     Timer callback.  Used to make sure the modem stays powered up long enough to receive SMS
 * @param     timerHandle - The timer Handle
 * @return    None
 * @Warning   We cannot call the sequencer directly from here (the timer interrupt)
 *            instead we must queue a message for the modem task
 */
static void SeqHeartbeatTimerCallback (xTimerHandle timerHandle)
{
  SecondsSincePowerOn++;

  /* check if it is time to power down the modem */
  if (MODEM_ON_TIME_SECONDS == SecondsSincePowerOn)
  {
    /* it is time to power down the modem */
    SequencerEnqueueMessage(SM_TIMER_MODEM_OFF, NULL);
  }
}
