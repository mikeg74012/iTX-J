/*
 **************************************************************************************************
 * @file   SeqEmergency.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   2020-04-16
 *
 * @brief  Event sequencer for the "emergency" state.
 *
 **************************************************************************************************
 */
#ifndef _SEQEMERGENCY_H_
#define _SEQEMERGENCY_H_

#include "Sequencer.h"
/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */

/* emergency type in process */
typedef enum
{
  ET_NONE,
  ET_IMPACT,
  ET_BUTTON
} eEmergencyType;


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
bool SeqEmergencySM             (SequencerMessage_t message);
bool SeqEmergencyAllowCallCancel        (void);
bool SeqEmergencyIsInVoiceCallStage     (void);     /* Indicate if seq is in voice call stage, from Modem Power on to in the Call */
bool SeqEmergencyImpactWithFreeFall     (void);     /* Tell if the current Emergency Seq contains free-fall data */

#endif /* _SEQEMERGENCY_H_ */
