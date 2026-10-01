/*
 **************************************************************************************************
 * @file   SeqJourney.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   2020-04-16
 *
 * @brief  Event sequencer for the "journey" state.  Handles the sequence of modem events that
 *         must be performed when the vehicle is in-journey.
 *
 **************************************************************************************************
 */
#ifndef _SEQJOURNEY_H_
#define _SEQJOURNEY_H_


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
void      SeqJourneyInit          (void);
bool      SeqJourneySM            (SequencerMessage_t message);
bool      SeqJourneyInProgress    (void);
void*     SeqJourneyGetGNSSData   (bool isSoj);
uint32_t  SeqJourneyGetJourneyId  (void);
bool      IsSeqInJourney          (void);

#endif /* _SEQJOURNEY_H_ */
