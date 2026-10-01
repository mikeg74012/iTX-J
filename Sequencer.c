/*
 **************************************************************************************************
 * @file   sequencer.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   2020-04-16
 *
 * @brief  GSM modem event sequencer.  Handles the high level sequencing of events that happen on
 *         the telematics device.  For example, in the event of an impact, we must send event data
 *         via GPRS, send a summary message via SMS, then make a voice call etc
 *
 **************************************************************************************************
 */
#include "global.h"


/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */
/* Define the queue parameters. */
#define SEQUENCER_QUEUE_LENGTH          8
#define RED_BUTTON_BLACKOUT_TIME        5         /* 5s blackout period after the RB cancel to prevent RB Seq initiated right after immediate */

/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
static eSequencerState  SequencerState = SS_IDLE;
static QueueHandle_t    SequencerQueue;
static uint8_t          msgPara;
static uint32_t         LastRBCancelTimeStamp = 0;

/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
/* state machine for the top level state */
static void SequencerSMTopLevel       (SequencerMessage_t message);


/**
 **************************************************************************************************
 *                                     PUBLIC FUNCTIONS / API
 **************************************************************************************************
 */
/*
 * @brief     The sequencer task.  Responsible for sequencing modem events.  For example, in the
 *            event of an impact, we send data, send an SMS and then make a voice call
 * @param     argument (not used)
 * @return    None
 */
void SequencerTask (void const * argument)
{
  SequencerMessage_t  message;

  /* create a message queue for this task */
  SequencerQueue = xQueueCreate(SEQUENCER_QUEUE_LENGTH, sizeof(SequencerMessage_t));

  /* initialise the sequence handlers */
  SeqJourneyInit();
  SeqHeartbeatInit();

  /* Main task loop */
  for (;;)
  {
    if (xQueueReceive(SequencerQueue, &message, portMAX_DELAY ) == pdPASS)
    {
      /* process a received message */
      SequencerSMTopLevel(message);
    }
  }
}


/*
 * @brief     Send a device heart beat message to the cloud
 * @param     None
 * @return    None
 */
void SequencerHeartBeat (void)
{
  SequencerEnqueueMessage(SM_HEARTBEAT, NULL);
  TRACE_SEQ("Heart beat\n\r");
}


/*
 * @brief     Start the "in journey" sequence
 *            If device is in Emergency Sequence (RB/Impact), SOJ will not start at that time
 *            If device is in other Sequences (DHM/Selftest), SOJ abort others and start SOJ
 * @param     None
 * @return    None
 */
void SequencerStartOfJourney (void)
{
  if (!SequencerInEmergency())
  {
    /* Journey will abort other sequence, except it is RB or Impact event */
    if (SequencerBusy())
    {
      SequencerEnqueueMessage(SM_ABORT, NULL);
    }
    SequencerEnqueueMessage(SM_START_OF_JOURNEY, NULL);
    TRACE_SEQ("Start of journey\n\r");
  }
}


/*
 * @brief     End the "in journey" sequence
 * @param     None
 * @return    None
 */
void SequencerEndOfJourney (void)
{
  SequencerEnqueueMessage(SM_END_OF_JOURNEY, NULL);
  TRACE_SEQ("End of journey\n\r");
}


/*
 * @brief     Send a low battery report to the cloud
 * @param     None
 * @return    None
 */
void SequencerLowBattery (void)
{
  SequencerEnqueueMessage(SM_LOW_BATTERY, NULL);
  TRACE_SEQ("Low battery\n\r");
}


/*
 * @brief     Start the emergency / impact sequence
 *            NOTE: This aborts any other active sequence
 * @param     isFreefall : indicate if it is impact with freefall event
 * @return    None
 */
void SequencerImpact (bool isFreefall)
{
  if (SequencerState != SS_EMERGENCY)
  {
    SequencerEnqueueMessage(SM_ABORT, NULL);
  }
  if (!isFreefall)
  {
    SequencerEnqueueMessage(SM_IMPACT, NULL);
    TRACE_SEQ("Impact\n\r");
  }
  else
  {
    SequencerEnqueueMessage(SM_FREEFALL_IMAPCT, NULL);
    TRACE_SEQ("Impact with freefall\n\r");
  }
}


/*
 * @brief     Start the emergency / button press sequence
 *            NOTE: This aborts any other active sequence if any
 * @param     None
 * @return    None
 */
void SequencerButtonPress (void)
{
  uint32_t now = RtcGetTimeStamp();

  if (SequencerBusy() && !SequencerInEmergency())
  {
    SequencerEnqueueMessage(SM_INTERRUPT, NULL);
  }

  if (abs(now - LastRBCancelTimeStamp) > RED_BUTTON_BLACKOUT_TIME)
  {
    SequencerEnqueueMessage(SM_BUTTON_PRESS, NULL);
    TRACE_SEQ("Button press\n\r");
  }
}


/*
 * @brief     Button Down only acknoledge if we are in RB Emergency Seq and state allowed
 *            Otherwise, Key need to be down more than 1s -> SequencerButtonPress
 * @param     None
 * @return    None
 */
void SequencerButtonDown (void)
{
  if (SequencerInEmergency() && SeqEmergencyAllowCallCancel())
  {
    LastRBCancelTimeStamp = RtcGetTimeStamp();
    SequencerEnqueueMessage(SM_BUTTON_PRESS, NULL);
    TRACE_SEQ("Button press\n\r");
  }
}


/*
 * @brief     Send device self test message to the cloud by Report Seq
 * @param     None
 * @return    None
 */
void SequencerSelfTest (void)
{
  msgPara = REPORT_SELFTEST;
  SequencerEnqueueMessage(SM_REPORT, (void*)&msgPara);
  TRACE_SEQ("Self Test\n\r");
}


/*
 * @brief     Checks the sequencer status and processes the instant event.
 *            If the device is in "Journey" mode, the modem is already on,
 *            and the instant event is enqueued immediately.
 * @param     None
 * @return    true if instantEventRequested requested, otherwise false
 */
bool SequencerProcessInstantEventRequest (void)
{
  bool instantEventRequested = true;

  if (!SequencerBusy())
  { 
    /* using report SEQ to send the Instant Report */
    msgPara = REPORT_INSTANT_EVENT;
    SequencerEnqueueMessage(SM_REPORT, (void*)&msgPara);
    TRACE_SEQ("Send Instant Event\n\r");
  }
  else if (SeqJourneyInProgress())
  {  /* modem is ready, send the event straight away */
    SequencerEnqueueMessage(SM_INSTANT_REPORT, NULL);
    TRACE_SEQ("Send Instant Event during journey \n\r");
  }
  else
  {
    instantEventRequested = false;
    TRACE_SEQ("Unit is busy, Instant Event will send later \n\r");
  }

  return instantEventRequested;
}


/*
 * @brief     Return if current Sequencer is Busy
 * @param     None
 * @return    true if the sequencer is busy
 */
bool SequencerBusy (void)
{
  return (SS_IDLE != SequencerState);
}


/*
 * @brief     Check if sequencer is processing an emergency (red button or impact)
 * @param     None
 * @return    true if the sequencer is processing an emergency
 */
bool SequencerInEmergency (void)
{
  return (SS_EMERGENCY == SequencerState);
}


/*
 * @brief     Check if sequencer is processing in journey
 * @param     None
 * @return    true if the sequencer is in journey
 */
bool SequencerInJourney (void)
{
  return (SS_IN_JOURNEY == SequencerState);
}


/*
 * @brief     Called by the modem when ithas successfully finished a command
 * @param     None
 * @return    None
 */
void SequencerModemComplete (void)
{
  SequencerEnqueueMessage(SM_MODEM_COMPLETE, NULL);
}


/*
 * @brief     Called by the modem if it could NOT complete a command
 * @param     None
 * @return    None
 */
void SequencerModemError (void)
{
  SequencerEnqueueMessage(SM_MODEM_ERROR, NULL);
}


/*
 * @brief     Notify Network Device (Modem/Wifi) to abort current action
 * @param     None
 * @return    None
 */
void SequencerNetworkAbort(void)
{
  /* tell the modem to stop whatever it is doing */
  ModemAbort();

  /* tell the wifi module to stop if it is scanning */
  WifiAbort();
}


/*
 * @brief  Add a new message to the sequencer task message queue
 * @param  msgId     the message ID to send
 * @param  parameter parameter of unknown type
 * @retval None
 */
void SequencerEnqueueMessage (uint8_t msgId, void * parameter)
{
  SequencerMessage_t message;

  message.msgId = msgId;
  message.parameter = parameter;
  xQueueSendToBackFromISR( SequencerQueue, &message, NULL );
}


/**
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */
/*
 * @brief     The top level sequencer state machine.  Handles the modem communications sequences
 *            required for different device operation states.  Sleep, In journey, Emergency
 * @param     message  the event message received by the sequencer
 * @return    None
 */
static void SequencerSMTopLevel (SequencerMessage_t message)
{
  bool SubStateIdle = false;

  /*
   * Check for top level state changes
   */
  switch (message.msgId)
  {
  case SM_START_OF_JOURNEY:
    SequencerState = SS_IN_JOURNEY;
    break;
  case SM_HEARTBEAT:
    SequencerState = SS_HEARTBEAT;
    break;
  case SM_IMPACT:
  case SM_FREEFALL_IMAPCT:
  case SM_BUTTON_PRESS:
    SequencerState = SS_EMERGENCY;
    break;
  case SM_REPORT:
    SequencerState = SS_REPORT;
    break;
  }

  /* Now handle the new message based on our current state */
  switch (SequencerState)
  {
  case SS_IDLE:
    /* do nothing */
    break;
  case SS_HEARTBEAT:
    SubStateIdle = SeqHeartbeatSM(message);
    break;
  case SS_IN_JOURNEY:
    SubStateIdle = SeqJourneySM(message);
    break;
  case SS_EMERGENCY:
    SubStateIdle = SeqEmergencySM(message);
    break;
  case SS_REPORT:
    SubStateIdle = SeqReportSM(message);
    break;
  }

  /* Other Sequencer finish all their job, resume Journey if any */
  if (SubStateIdle && SeqJourneyInProgress())
  {
    SequencerState = SS_IN_JOURNEY;
    SequencerEnqueueMessage(SM_RESUME, NULL);
  }
  else if (SubStateIdle)
  {
    SequencerState = SS_IDLE;
  }
}


/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       Show the current sequencer state
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int SequencerConsoleShowState (int argCount, char **pArguments)
{
  switch (SequencerState)
  {
  case SS_IDLE:
    ConsolePrintf("SS_IDLE\n\r");
    break;
  case SS_HEARTBEAT:
    ConsolePrintf("SS_HEARTBEAT\n\r");
    break;
  case SS_IN_JOURNEY:
    ConsolePrintf("SS_IN_JOURNEY\n\r");
    break;
  case SS_EMERGENCY:
    ConsolePrintf("SS_EMERGENCY\n\r");
    break;
  case SS_REPORT:
    ConsolePrintf("SS_REPORT\n\r");
    break;
  }

  ConsolePrintf("Have Journey in progress? %s\n\r",  (SeqJourneyInProgress()? "Yes": "No"));

  /* indicate success */
  return (CONSOLE_ERROR_OK);
}
