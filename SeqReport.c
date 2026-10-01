/*
 **************************************************************************************************
 * @file   SeqReport.c
 **************************************************************************************************
 *
 * @author Ripcord Designs - UA
 * @date   2020-05-24
 *
 * @brief  Event sequencer for the Report sending.  Handles the sequence of modem events that
 *         are performed when the device wants to send the Report to the cloud.
 *         These events are:
 *           1. Power up the modem
 *           2. Send the Report  to the cloud, possible report could be
 *              a. wake-up self test message 
 *              b. Instant Event Report
 *           3. Power down the modem
 *
 **************************************************************************************************
 */
#include "global.h"



/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */
/* List of Report sequencer states.  Used in conjunction with the sequencer state table */
typedef enum
{
  SRS_NONE,
  SRS_IDLE,
  SRS_POWER_ON,
  SRS_START_GNSS,
  SRS_GET_POSITION,
  SRS_STOP_GNSS,
  SRS_SEND_REPORT,
  SRS_SEND_SMS,
  SRS_POWER_OFF,
  SRS_ANY
} eSeqReportState;

/* Event types that can be sent to the sequencer */
typedef enum
{
  SRS_EV_NONE,
  SRS_EV_ENTRY,
  SRS_EV_EXIT,
  SRS_EV_SIGNAL,
} eSeqEvent;

/* Type of each entry in the sequencer state table */
typedef struct
{
  eSeqReportState     State;
  eSeqEvent           Event;
  void *              EventParam;
  void                (*pActionFunc)(void);
  int                 NoOfRetries;
  eSeqReportState     NextState;
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
  SeqReportSequencer(Event);      \
}


/* macro to transition from one state to the next */
#define SH_TRAN(_st)              \
{                                 \
  Event_t Event;                  \
  Event.Parameter = NULL;         \
  Event.EventType = SRS_EV_EXIT;  \
  SeqReportSequencer(Event);      \
  CurrentState = _st;             \
  Event.EventType = SRS_EV_ENTRY; \
  SeqReportSequencer(Event);      \
}


/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static void SeqReportSequencer     (Event_t event);
static void SeqReportSendReport    (void);
static void SeqReportStartGNSS     (void);
static void SeqReportAbort         (void);
static void SeqReportComplete      (void);
static void SeqReportGetPosition   (void);
static void SeqReportStopGNSS      (void);
static void SeqReportSendSms       (void);
static void SeqReportPowerOn       (void);
static void SeqReportPowerOff      (void);

/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
/* The current state of this sequencer */
static eSeqReportState CurrentState = SRS_IDLE;
static eSeqReportType  *reportType;


/*
 * Table to define the sequence of actions to be carried out in the "heart beat" sequence
 */
static StateTable_t StateTable[] =
{
  /* STATE           EVENT            EVENT_PARAM                   ACTION FUNCTION          RETRIES     NEXT STATE     */
  {SRS_IDLE,         SRS_EV_ENTRY,    NULL,                         SeqReportComplete,       0,        SRS_IDLE         },  // on entry, indicate that we have completed the sequence
  {SRS_IDLE,         SRS_EV_SIGNAL,   (void *) SM_REPORT,           NULL,                    0,        SRS_POWER_ON     },  // on SelfTest event, move to SRS_START_GNSS state

  {SRS_POWER_ON,     SRS_EV_ENTRY,    NULL,                         SeqReportPowerOn,        0,        SRS_POWER_ON     },  // on entry, tell modem to power up
  {SRS_POWER_ON,     SRS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,        SRS_START_GNSS   },  // on success, move to the SJS_START_GNSS state
  {SRS_POWER_ON,     SRS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,        SRS_IDLE         },  // if we cannot power up the modem then go back to idle (nothing else to do)

  {SRS_START_GNSS,   SRS_EV_ENTRY,    NULL,                         SeqReportStartGNSS,      2,        SRS_START_GNSS   },  // on entry, tell modem to start GNSS
  {SRS_START_GNSS,   SRS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,        SRS_GET_POSITION },  // on Success, move to the SRS_GET_POSITION state
  {SRS_START_GNSS,   SRS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,        SRS_GET_POSITION },

  {SRS_GET_POSITION, SRS_EV_ENTRY,    NULL,                         SeqReportGetPosition,    0,        SRS_GET_POSITION },  // on entry, request position from GNSS
  {SRS_GET_POSITION, SRS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,        SRS_STOP_GNSS    },  // on success, move to the SHS_STOP_GNSS state
  {SRS_GET_POSITION, SRS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,        SRS_STOP_GNSS    },

  {SRS_STOP_GNSS,    SRS_EV_ENTRY,    NULL,                         SeqReportStopGNSS,       2,        SRS_STOP_GNSS    },  // on entry, tell modem to stop GNSS
  {SRS_STOP_GNSS,    SRS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,        SRS_SEND_REPORT  },  // on success, move to the SRS_SEND_REPORT state
  {SRS_STOP_GNSS,    SRS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,        SRS_SEND_REPORT  },

  {SRS_SEND_REPORT,    SRS_EV_ENTRY,    NULL,                         SeqReportSendReport,   5,        SRS_SEND_REPORT  },  // on entry, send REPORT message via GPRS to cloud
  {SRS_SEND_REPORT,    SRS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                  0,        SRS_SEND_SMS     },
  {SRS_SEND_REPORT,    SRS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                  0,        SRS_SEND_SMS     },

  {SRS_SEND_SMS,     SRS_EV_ENTRY,    NULL,                         SeqReportSendSms,        5,        SRS_SEND_SMS     },  // on entry, send REPORT message via modem to cloud
  {SRS_SEND_SMS,     SRS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,        SRS_POWER_OFF    },
  {SRS_SEND_SMS,     SRS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,        SRS_POWER_OFF    },

  {SRS_POWER_OFF,    SRS_EV_ENTRY,    NULL,                         SeqReportPowerOff,       0,        SRS_POWER_OFF    },  // on entry, tell modem to power down
  {SRS_POWER_OFF,    SRS_EV_SIGNAL,   (void *) SM_MODEM_COMPLETE,   NULL,                    0,        SRS_IDLE         },  // on success, move to the SRS_IDLE state
  {SRS_POWER_OFF,    SRS_EV_SIGNAL,   (void *) SM_MODEM_ERROR,      NULL,                    0,        SRS_IDLE         },

  /* the table must be terminated with a null entry */
  {SRS_NONE,         SRS_EV_NONE,     NULL,                         NULL,                    0,        SRS_NONE         },
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
bool SeqReportSM (SequencerMessage_t message)
{
  switch (message.msgId)
  {
  case SM_ABORT:
  case SM_INTERRUPT:
    /* if this is an ABORT/INTERRUPT message, we process it directly */
    SeqReportAbort();
    break;
  case SM_REPORT:
    reportType = (eSeqReportType*) message.parameter;
    TRACE_SEQ("Report---Type got %d\r\n",*reportType);
    /* Deliberate fall-through: Handle message like default case */
  default:
    SH_EVENT(SRS_EV_SIGNAL, message.msgId);
    break;
  }
  return (SRS_IDLE == CurrentState);
}


/*
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */
/**
 * @brief     The sequencer logic for the Report state.  Processes incoming events using the
 *            sequencer table defined above
 * @param     event - the sequencer event to process
 * @return    None
 * @Warning   Will be called recursively when changing state.  Must be re-entrant
 */
static void SeqReportSequencer (Event_t event)
{
  int index = 0;

  /* scan through the sequencer table */
  while (SRS_NONE != StateTable[index].State)
  {
    /* check if "current state" and "event" both match */
    if ( ((CurrentState == StateTable[index].State) || (SRS_ANY == StateTable[index].State))
      && (event.EventType == StateTable[index].Event))
    {
      /* if the event type is signal, see if it matches */
      if ( (SRS_EV_SIGNAL != event.EventType)
        || ((SRS_EV_SIGNAL == event.EventType) && (event.Parameter == StateTable[index].EventParam)) )
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
          TRACE_SEQ("ST %d->%d\n\r", CurrentState, StateTable[index].NextState);
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
static void SeqReportPowerOn (void)
{
  TRACE_SEQ("Power up modem\n\r");
  ModemPowerUp();
}


/**
 * @brief     Ask the modem to power down
 * @param     None
 * @return    None
 */
static void SeqReportPowerOff (void)
{
  TRACE_SEQ("Power down modem\n\r");
  ModemPowerDown();
}


/**
 * @brief     Ask the modem to send a Report message to the cloud
 *            The report could be "Device Self Test Message", Instant Event etc.
 * @param     None
 * @return    None
 */
static void SeqReportSendReport (void)
{
  if (*reportType == REPORT_INSTANT_EVENT)
  {
    /* Build the Device instant event Message and add to GPRS send queue */
    MessageEnqueueDsm ();

    TRACE_SEQ("Report events to cloud\n\r");
  }
  else if (*reportType == REPORT_SELFTEST)
  {
    /* Build the Device Initialisation Message and add to GPRS send queue */
    MessageEnqueueDim();

    TRACE_SEQ("Send Device Initialisation Message to cloud\n\r");
  }
  /* Ask the modem to send queued messages */
  ModemDefaultSendQueued(MESSAGE_PRIORITY_LOW);
}


/**
 * @brief     Abort all activity and return to the IDLE state
 * @param     None
 * @return    None
 */
static void SeqReportAbort (void)
{
  TRACE_SEQ("ST !! ABORT !!\n\r");

  /* tell the Network Module (Modem/Wifi) to stop whatever it is doing */
  SequencerNetworkAbort();

  /* change back to the idle state */
  CurrentState = SRS_IDLE;
}


/**
 * @brief     Tell the IMU we have finished processing the Report message for sleep purposes
 * @param     None
 * @return    None
 */
static void SeqReportComplete (void)
{
  MotionMonitorDstmSequenceFinished();
}


/**
 * @brief     Ask the modem to start the GNSS service
 * @param     None
 * @return    None
 */
static void SeqReportStartGNSS (void)
{
  TRACE_SEQ("Start GNSS\n\r");
  ModemGnssStart();
}

/**
 * @brief     Get our current GNSS position from the modem
 * @param     None
 * @return    None
 */
static void SeqReportGetPosition (void)
{
  TRACE_SEQ("Get GNSS data\n\r");
  ModemGnssGetData(false);
}

/**
 * @brief     Ask the modem to start the GNSS service
 * @param     None
 * @return    None
 */
static void SeqReportStopGNSS (void)
{
  TRACE_SEQ("Stop GNSS\n\r");
  ModemGnssStop();
}


/**
 * @brief     Send init SMS (if enabled)
 * @param     None
 * @return    None
 */
static void SeqReportSendSms (void)
{
  uint8_t * pMessage = NULL;
  int MessageSize;

  if ((*reportType == REPORT_SELFTEST))      /* self test sms */
  {
    TRACE_SEQ("Send initialization SMS\n\r");

    /*  send a DIM / reset SMS (if enabled in config store) */
    if (ConfigGetFlag(CONFIG_FLAG_SMS_DIM))
    {
      /* Build the SMS initialization message */
      MessageEncodeSmsDim(&pMessage, &MessageSize);

      /* Ask the modem to send it */
      ModemSmsSend((char*) pMessage);
    }
    else
    {
      /* Nothing to do - tell sequencer to skip to the next state */
      SequencerEnqueueMessage(SM_MODEM_ERROR, NULL);
    }
  }
  else
  {
    /* no SMS for Modem for the other report, send complete and carry on */
    SequencerModemComplete();  /* send complete and carry on*/
    return;
  }
}




