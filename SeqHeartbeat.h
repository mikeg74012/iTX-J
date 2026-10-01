/*
 **************************************************************************************************
 * @file   SeqHeartbeat.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   2020-04-16
 *
 * @brief  Event sequencer for the "heartbeat" state.  Handles the sequence of modem events that must
 *         be performed when the device is heartbeat.
 *
 **************************************************************************************************
 */
#ifndef _SEQHEARTBEAT_H_
#define _SEQHEARTBEAT_H_

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
void SeqHeartbeatInit (void);
bool SeqHeartbeatSM   (SequencerMessage_t message);

uint8_t SeqHeartbeatGetHouseKeep (void);
void    LPRestoreHeartbeatHouseKeep (uint8_t value);

#endif /* _SEQIDLE_H_ */
