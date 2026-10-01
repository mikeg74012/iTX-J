/*
 **************************************************************************************************
 * @file   SeqEmergency.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   2020-04-16
 *
 * @brief  Event sequencer for the "emergency" state.  Handles the sequence of communications that
 *         must happen when and impact or red button event is detected.
 *         The sequence comprises:
 *           1. Power up the modem (if necessary)
 *           2. Send the emergency event data to the cloud via GPRS
 *           3. Send SMS message containing a short summary the emergency event
 *           4. Make a voice call to the emergency call centre
 *
 **************************************************************************************************
 */
#include "global.h"


/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */
/* List of emergency sequencer states.  Used in conjunction with the sequencer state table */
typedef enum
{
  SES_NONE,              // 0
  SES_IDLE,              // 1
  SES_POWER_ON,          // 2
  SES_PLAY_TONES,        // 3
  SES_SEND_SMS,          // 4
  SES_GSM_CHK,           // 5
  SES_VOICE_CALL,        // 6
  SES_SEND_IEM,          // 7
  SES_POWER_OFF,         // 8
  SES_ANY                // 9
} eSeqEmergencyState;

/* Event types that can be sent to the sequencer */
typedef enum
{
  SES_EV_NONE,
  SES_EV_ENTRY,
  SES_EV_EXIT,
  SES_EV_SIGNAL,
} eSeqEvent;

/* Type of each entry in the sequencer state table */
typedef struct
{
  eSeqEmergencyState   State;
  eSeqEvent            Event;
  void *               EventParam;
  void                 (*pActionFunc)(void);
  int                  NoOfRetries;
  eSeqEmergencyState   NextState;
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
  SeqEmergencySequencer(Event);   \
}


/* macro to transition from one state to the next */
#define SH_TRAN(_st)              \
{                                 \
  Event_t Event;                  \
  Event.Parameter = NULL;         \
  Event.EventType = SES_EV_EXIT;  \
  SeqEmergencySequencer(Event);   \
  CurrentState = _st;             \
  Event.EventType = SES_EV_ENTRY; \
  SeqEmergencySequencer(Event);   \
}



/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static void SeqEmergencySequencer     (Event_t event);
static void SeqEmergencyPlayTones     (void);
static void SeqEmergencyCheckGSMInfo  (void);
static void SeqEmergencySendIemRbm    (void);
static void SeqEmergencySendSMS       (void);
static void SeqEmergencyVoiceCall     (void);
static void SeqEmergencyAbort         (void);
static void SeqEmergencyComplete      (void);
static void SeqEmergencyTypeImpact    (void);
static void SeqEmergencyTypeFreefallImpact (void);
static void SeqEmergencyTypeButton    (void);
static void SeqEmergencySentIemRbm    (void);
static void SeqEmergencyFailedIemRbm  (void);
static void SeqEmergencyInitiated     (void);
static void SeqEmergencyPowerOn       (void);
static void SeqEmergencyPowerOff      (void);
static void SeqEmergencyRBCancel      (void);
static void SeqEmergencyRBCallAgain       (void);
static void SeqEmergencyImpactCallAgain   (void);
static bool SeqEmergencyIsRedButton       (void);
static bool SeqEmergencyIsVoiceCallInProgress  (void);     /* Indicate if Em Seq is in voice call state */



/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
/* The current state of this sequencer */
static eSeqEmergencyState CurrentState = SES_IDLE;

/* Emergency type in process (impact / button press) */
static eEmergencyType     EmergencyType = ET_NONE;

/* flag to indicate whether impact / button  press emergency message was transmitted successfully */
static bool EmergencyCloudMessageSent  = false;

/* flag to indicate that the emergency call was cancelled */
static uint8_t RedButtonCanceledFlag  = 0;

/* flag to tell if the freefall exist in the impact data */
static bool IsFreeFallImpact = false;

/*
 * Table to define the sequence of actions to be carried out in the "emergency" sequence
 */
static StateTable_t StateTable[] =
{
  /* STATE           EVENT            EVENT_PARAM                   ACTION FUNCTION           RETRIES     NEXT STATE     */
  {SES_IDLE,         SES_EV_ENTRY,    NULL,                         SeqEmergencyComplete,     0,          SES_IDLE         },  // on entry, indicate that we have completed the sequence
  {SES_IDLE,         SES_EV_SIGNAL,   (void *) SM_IMPACT,           SeqEmergencyTypeImpact,   0,          SES_POWER_ON     },  // on Impact event, move to SES_POWER_ON state
  {SES_IDLE,         SES_EV_SIGNAL,   (void *) SM_BUTTON_PRESS,     SeqEmergencyTypeButton,   0,          SES_POWER_ON     },  // on Button Press event, move to SES_POWER_ON state

  {SES_IDLE,         SES_EV_SIGNAL,   (void *) SM_FREEFALL_IMAPCT,  SeqEmergencyTypeFreefallImpact,   0,          SES_SEND_IEM     },  // on Impact event with freefall, skip call, and just send the Impact data only

  {SES_POWER_ON,     SES_EV_ENTRY,    NULL,                         SeqEmergencyPowerOn,      0,          SES_POWER_ON     },  // on entry, tell modem to power up
  {SES_POWER_ON,     SES_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                     0,          SES_PLAY_TONES   },  // on success, move to the SES_PLAY_TONES state
  {SES_POWER_ON,     SES_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                     0,          SES_PLAY_TONES   },

  {SES_PLAY_TONES,   SES_EV_ENTRY,    NULL,                         SeqEmergencyPlayTones,    5,          SES_PLAY_TONES   },  // on entry, play emergency tones from speaker
  {SES_PLAY_TONES,   SES_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                     0,          SES_GSM_CHK      },
  {SES_PLAY_TONES,   SES_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                     0,          SES_GSM_CHK      },

  {SES_GSM_CHK,      SES_EV_ENTRY,    NULL,                         SeqEmergencyCheckGSMInfo, 5,          SES_GSM_CHK      },  // on entry, play emergency tones from speaker
  {SES_GSM_CHK,      SES_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                     0,          SES_SEND_SMS     },
  {SES_GSM_CHK,      SES_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                     0,          SES_SEND_SMS     },

  {SES_SEND_SMS,     SES_EV_ENTRY,    NULL,                         SeqEmergencySendSMS,      5,          SES_SEND_SMS     },  // on entry, send summary SMS
  {SES_SEND_SMS,     SES_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                     0,          SES_VOICE_CALL   },  // on success, move to the SES_VOICE_CALL state
  {SES_SEND_SMS,     SES_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                     0,          SES_VOICE_CALL   },

  {SES_VOICE_CALL,   SES_EV_ENTRY,    NULL,                         SeqEmergencyVoiceCall,    5,          SES_VOICE_CALL   },  // on entry, make voice call
  {SES_VOICE_CALL,   SES_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                     0,          SES_SEND_IEM     },  // wait call completion
  {SES_VOICE_CALL,   SES_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                     0,          SES_SEND_IEM     },

  {SES_SEND_IEM,     SES_EV_ENTRY,    NULL,                         SeqEmergencySendIemRbm,   5,          SES_SEND_IEM     },  // on entry, send Emergency Event Message to cloud
  {SES_SEND_IEM,     SES_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   SeqEmergencySentIemRbm,   0,          SES_POWER_OFF    },
  {SES_SEND_IEM,     SES_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      SeqEmergencyFailedIemRbm, 0,          SES_POWER_OFF    },
  {SES_SEND_IEM,     SES_EV_SIGNAL,   (void *) SM_BUTTON_PRESS,     SeqEmergencyRBCallAgain,  0,          SES_POWER_ON     },  // another long RB press during message send will bring Em Seq from the start
  {SES_SEND_IEM,     SES_EV_SIGNAL,   (void *) SM_IMPACT,           SeqEmergencyImpactCallAgain,    0,          SES_POWER_ON     },  // an impact during message send will bring Em Seq from the start

  {SES_POWER_OFF,    SES_EV_ENTRY,    NULL,                         SeqEmergencyPowerOff,     0,          SES_POWER_OFF    },  // on entry, tell modem to power down
  {SES_POWER_OFF,    SES_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                     0,          SES_IDLE         },  // on success, move to the SES_IDLE state
  {SES_POWER_OFF,    SES_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                     0,          SES_IDLE         },

  {SES_ANY,          SES_EV_SIGNAL,   (void *) SM_BUTTON_PRESS,     SeqEmergencyRBCancel,     0,          SES_NONE         },  // perform red button cancel and stay in same state

  /* the table must be terminated with a null entry */
  {SES_NONE,         SES_EV_NONE,     NULL,                         NULL,                     0,          SES_NONE         },
};


/**
 **************************************************************************************************
 *                                     PUBLIC FUNCTIONS / API
 **************************************************************************************************
 */

/*
 * @brief     Handle messages from other modules (IMU, ButtonMonitor, Modem etc)
 * @param     message  the event message received by the sequencer
 * @return    true if sequencer is idle, false otherwise
 */
bool SeqEmergencySM (SequencerMessage_t message)
{
  /* if this is an ABORT message, we process it directly */
  if (SM_ABORT == message.msgId)
  {
    SeqEmergencyAbort();
  }
  else
  {
    SH_EVENT(SES_EV_SIGNAL, message.msgId);
  }
  return (SES_IDLE == CurrentState);
}


/*
 * @brief     Check if Emergency sequencer allow the the call being cancelled
 * @param     None
 * @return    true if the Emergency sequencer allow the call being cancelled
 */
bool SeqEmergencyAllowCallCancel(void)
{
  return (SeqEmergencyIsRedButton() && SeqEmergencyIsVoiceCallInProgress());
}


/*
 * @brief     return true if any Voice Call is initiating or in progress
 * @param     None
 * @return    true if Voice Call is initiating or in progress, otherwise false
 */
bool SeqEmergencyIsInVoiceCallStage (void)
{
  return (CurrentState > SES_IDLE) && (CurrentState <= SES_VOICE_CALL);
}


/*
 * @brief     return true if current sequence is for reporting impact with free-fall (only used for the Impact Seq)
 * @param     None
 * @return    true if impact with free-fall data, otherwise false
 */
bool SeqEmergencyImpactWithFreeFall (void)
{
  return ((EmergencyType == ET_IMPACT) && (IsFreeFallImpact));
}


/*
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */
/**
 * @brief     The sequencer logic for the Emergency state.  Processes incoming events using the
 *            journey sequencer table defined above
 * @param     event - the sequencer event to process
 * @return    None
 * @Warning   Will be called recursively when changing state.  Must be re-entrant
 */
static void SeqEmergencySequencer (Event_t event)
{
  int index = 0;

  /* scan through the sequencer table */
  while (SES_NONE != StateTable[index].State)
  {
    /* check if "current state" and "event" both match */
    if ( ((CurrentState == StateTable[index].State) || (SES_ANY == StateTable[index].State))
      && (event.EventType == StateTable[index].Event))
    {
      /* if the event type is signal, see if it matches */
      if ( (SES_EV_SIGNAL != event.EventType)
        || ((SES_EV_SIGNAL == event.EventType) && (event.Parameter == StateTable[index].EventParam)) )
      {
        /* take the actions specified in the table */
        if (NULL != StateTable[index].pActionFunc)
        {
          /* a function is specified, so call it */
          StateTable[index].pActionFunc();
        }
        if ((StateTable[index].NextState != CurrentState) && (StateTable[index].NextState != SES_NONE))
        {
          /* a state transition is specified so change state now */
          TRACE_SEQ("SE %d->%d\n\r", CurrentState, StateTable[index].NextState);
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
static void SeqEmergencyPowerOn (void)
{
  TRACE_SEQ("Power up modem\n\r");
  ModemPowerUp();
}


/**
 * @brief     Ask the modem to power down
 * @param     None
 * @return    None
 */
static void SeqEmergencyPowerOff (void)
{
  TRACE_SEQ("Power down modem\n\r");
  ModemPowerDown();
}


/**
 * @brief     Function call when SeqEmergency when Initiated. 
 *            Reset Last Seq old status
 * @param     None
 * @return    None
 */
static void SeqEmergencyInitiated (void)
{
  /* reset the Red Button flag */
  RedButtonCanceledFlag = 0;
  
  /* reset the voice call status */
  GsmVoiceStatusReset();
}


/**
 * @brief     Ask the modem to Get GSM Network Info
 * @param     None
 * @return    None
 */
static void SeqEmergencyCheckGSMInfo (void)
{
  ModemCheckGSMInfo();
}


/**
 * @brief     Ask the modem to play the emergency tones
 * @param     None
 * @return    None
 */
static void SeqEmergencyPlayTones (void)
{
  if (RedButtonCanceledFlag == 0)
  {
    TRACE_SEQ("Play emergency tones\n\r");
    /*Get the Config here to check which tone to play */
    if (ConfigGetFlag(CONFIG_FLAG_CUSTOM_TONE))
    {
      ModemAudioPlayTone(MODEM_TONE_CUSTOM_EMERGENCY_START);
    }
    else
    {
      ModemAudioPlayTone(MODEM_TONE_EMERGENCY);
    }
  }
  else
  {
    TRACE_SEQ("Emergency tones cancelled by user %d\n\r",  RedButtonCanceledFlag);
    SequencerEnqueueMessage(SM_MODEM_COMPLETE, NULL);
  }
}


/**
 * @brief     Ask the modem to send an Impact or Red Button Event Message message to the cloud
 * @param     None
 * @return    None
 * @Note      In future we may want to pass the message type as parameter so we can tell if it was an impact or button press
 */
static void SeqEmergencySendIemRbm (void)
{
  if (ET_IMPACT == EmergencyType)
  {
    MessageEnqueueIem();
    TRACE_SEQ("Send Impact Event Message to cloud\n\r");
  }
  else
  {
    /* queue the Red button and mini event messages */
    MessageEnqueueRbm();
    MessageEnqueueMem();
    TRACE_SEQ("Send Red Button & Mini-Event Messages to cloud\n\r");
  }

  /* Ask the modem to send queued emergency data */
  ModemDefaultSendQueued(MESSAGE_PRIORITY_HIGH);
}


/**
 * @brief     Ask the modem to send an emergency summary message via SMS
 * @param     None
 * @return    None
 */
static void SeqEmergencySendSMS (void)
{
  TRACE_SEQ("Send SMS Impact summary\n\r");

  /* Build the summary SMS message */
  uint8_t * pMessage = NULL;
  int MessageSize;

  /* Build the SMS emergency summary message*/
  MessageEncodeSmsEIM(&pMessage, &MessageSize, EmergencyType, EmergencyCloudMessageSent);

  if (MessageSize <= MAX_SMS_LEN)
  {
    /* Ask the modem to send it */
    ModemSmsSend((char*) pMessage);
  }
  else
  {
    TRACE_SEQ("SMS aborted! message size %d above MAX_SMS_LEN of %d \n\r", MessageSize, MAX_SMS_LEN);
    SequencerEnqueueMessage(SM_MODEM_ERROR, NULL);
  }
}


/**
 * @brief     Ask the modem to make a voice call to the emergency contact number
 * @param     None
 * @return    None
 */
static void SeqEmergencyVoiceCall (void)
{
  if (RedButtonCanceledFlag == 0)
  {
    TRACE_SEQ("Make voice call\n\r");
    ModemMakeVoiceCall(ConfigGetVoiceNumber());
  }
  else
  {
    TRACE_SEQ("Voice call cancelled by user %d\n\r",  RedButtonCanceledFlag);
    SequencerEnqueueMessage(SM_MODEM_COMPLETE, NULL);
  }
}


/**
 * @brief     Red button cancelled by user.
 * @param     None
 * @return    None
 * @Note      Press red button again to cancel the red button event will cancel the play tone and voice call
 *              the MSG and Event will send to cloud as normal.
 */
static void SeqEmergencyRBCancel (void)
{
  if (!SeqEmergencyAllowCallCancel())
  {
    TRACE_SEQ("Call cancel not allowed, ignore the RB\r\n");
    return;
  }
  RedButtonCanceledFlag++;
  TRACE_SEQ("Red Button Cancelled by user %d\r\n", RedButtonCanceledFlag);

  /* Log the RB cancellation to the event log */
  if (RedButtonCanceledFlag == 1 )  /* only add status once */
  {
    StatusLogWriteEventCode(STATUS_RED_BUTTON_CANCELLED, STATUS_DATA_LOAD_NONE);
  }

  /* if we are currently making a call, ask modem to hang up */
  if (CurrentState == SES_VOICE_CALL )
  {
    TRACE_SEQ("Cancel & hang voice call %d\n\r",  RedButtonCanceledFlag);
    GsmVoiceRequestHangUp();
  }

  /* if we are playing tones, stop them */
  if (CurrentState >= SES_PLAY_TONES )
  {
    TRACE_SEQ("Cancel Emergency tones %d\n\r",  RedButtonCanceledFlag);
    /* Only have action if it is the first cancel, for the later press, just ignore that */
    if (RedButtonCanceledFlag == 1)
    {
      if (ConfigGetFlag(CONFIG_FLAG_CUSTOM_TONE))
      {
        ModemAudioPlayTone(MODEM_TONE_CUSTOM_EMERGENCY_CANCEL);
      }
      else
      {
        ModemAudioPlayToneStop(MODEM_TONE_EMERGENCY);
      }
    }
  }
}


/*
 * @brief     Initiate the Emergency Call again by Red Button
 * @param     None
 * @return    None
 */
static void SeqEmergencyRBCallAgain (void)
{
  ModemAbort();
  SeqEmergencyTypeButton();
}


/*
 * @brief     Initiate the Emergency Call again by Impact
 * @param     None
 * @return    None
 */
static void SeqEmergencyImpactCallAgain (void)
{
  ModemAbort();
  SeqEmergencyTypeImpact();
}


/*
 * @brief     return if in voice call.
 * @param     None
 * @return    true if voice call in progress, otherwise false
 */
static bool SeqEmergencyIsVoiceCallInProgress (void)
{
  return (CurrentState == SES_VOICE_CALL);
}


/*
 * @brief     return if EmergencyType is for Red Button
 * @param     None
 * @return    true if it is Red button, otherwise false
 */
static bool SeqEmergencyIsRedButton (void)
{
  return (EmergencyType == ET_BUTTON)? true: false;
}


/**
 * @brief     Abort all activity and return to the IDLE state
 * @param     None
 * @return    None
 */
static void SeqEmergencyAbort (void)
{
  TRACE_SEQ("SE !! ABORT !!\n\r");

  /* tell the Network Module (Modem/Wifi) to stop whatever it is doing */
  SequencerNetworkAbort();

  /* change back to the idle state */
  CurrentState = SES_IDLE;
}


/**
 * @brief     Tell the IMU we have finished processing the emergency sequence
 * @param     None
 * @return    None
 */
static void SeqEmergencyComplete (void)
{
  EmergencyType = ET_NONE;
  MotionMonitorImpactSequenceFinished();
}



/**
 * @brief     Set the current emergency type to IMPACT
 * @param     None
 * @return    None
 */
static void SeqEmergencyTypeImpact    (void)
{
  EmergencyType = ET_IMPACT;
  IsFreeFallImpact = false;
  SeqEmergencyInitiated();
}


/**
 * @brief     Set the current emergency type to IMPACT, and indicate there is freefall in the data
 * @param     None
 * @return    None
 */
static void SeqEmergencyTypeFreefallImpact (void)
{
  EmergencyType = ET_IMPACT;
  IsFreeFallImpact = true;
  SeqEmergencyInitiated();
}


/**
 * @brief     Set the current emergency type to BUTTON press
 * @param     None
 * @return    None
 */
static void SeqEmergencyTypeButton    (void)
{
  EmergencyType = ET_BUTTON;
  SeqEmergencyInitiated();
}


/**
 * @brief     Set the flag when impact / button  press emergency message was transmitted successfully
 * @param     None
 * @return    None
 */
static void SeqEmergencySentIemRbm    (void)
{
  EmergencyCloudMessageSent = true;
}


/**
 * @brief     Set the flag when impact / button  press emergency message was not transmitted successfully
 * @param     None
 * @return    None
 */
static void SeqEmergencyFailedIemRbm    (void)
{
  EmergencyCloudMessageSent = false;
}



