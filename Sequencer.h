/*
 **************************************************************************************************
 * @file   sequencer.h
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
#ifndef MC60E_SEQUENCER_H_
#define MC60E_SEQUENCER_H_


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
/* Debug trace for the sequencer and its sub-modules */

#define TRACE_SEQ(...)

/*
#if TRACE_SEQUENCER_ENABLE
  #define TRACE_SEQ(...) TRACE_COLOUR_(FG_MAGENTA, BG_DEFAULT, "SEQ", __VA_ARGS__)
#else
  #define TRACE_SEQ(...)
#endif
*/


/* Sequencer States */
typedef enum
{
  SS_IDLE,                // Modem is idle
  SS_HEARTBEAT,           // Heart beat - get position, send heart beat message to cloud
  SS_IN_JOURNEY,          // In journey - do GNSS receive, SOJ, DJO, EOJ messages,
  SS_EMERGENCY,           // Emergency sequence (data, sms, voice call, mini-events)
  SS_REPORT,              // Reporting sequence (send report to cloud, eg. selftest, instantReport etc. )
} eSequencerState;

/* Structure for messages sent to the Sequencer task */
typedef struct
{
  int     msgId;
  void *  parameter;
} SequencerMessage_t;

/* Message ID's that can be sent to the sequencer */
typedef enum
{
  /* messages from imu, button monitor etc */
  SM_HEARTBEAT        = 0, /* 0 */
  SM_START_OF_JOURNEY,     /* 1 */
  SM_END_OF_JOURNEY,       /* 2 */
  SM_HALO_CHANGE,          /* 3 */
  SM_LOW_BATTERY,          /* 4 */
  SM_IMPACT,               /* 5 */
  SM_BUTTON_PRESS,         /* 6 */
  SM_REPORT,               /* 7 */
  SM_INSTANT_REPORT,       /* 8 */
  SM_FREEFALL_IMAPCT,      /* 9 */

  /* messages from the modem */
  SM_MODEM_COMPLETE   = 100,
  SM_MODEM_ERROR,

  /* internal sequencer messages */
  SM_ABORT            = 200,
  SM_INTERRUPT,
  SM_RESUME,
  SM_JOURNEY_SEND_RDY,
  SM_TIMER_GNSS,
  SM_TIMER_DJM,
  SM_TIMER_SCAN,
  SM_TIMER_WIFI_STOP,
  SM_TIMER_MODEM_OFF,
} eSequencerMessage;


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
void SequencerTask           (void const * argument);
void SequencerEnqueueMessage (uint8_t msgId, void * parameter);

/* API for device events */
void SequencerHeartBeat      (void);
void SequencerStartOfJourney (void);
void SequencerEndOfJourney   (void);
void SequencerLowBattery     (void);
void SequencerImpact         (bool isFreefall);
void SequencerButtonPress    (void);
void SequencerButtonDown     (void);
bool SequencerBusy           (void);
bool SequencerInEmergency    (void);
bool SequencerInJourney      (void);
void SequencerSelfTest       (void);
void SequencerNetworkAbort   (void);
bool SequencerProcessInstantEventRequest    (void);

/* API for modem events */
void SequencerModemComplete  (void);
void SequencerModemError     (void);

/* Console commands */
int SequencerConsoleShowState (int argCount, char **pArguments);


#endif /* MC60E_SEQUENCER_H_ */
