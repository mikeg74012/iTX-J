/*
 **************************************************************************************************
 * @file   SeqReport.h
 **************************************************************************************************
 *
 * @author Ripcord Designs - UA
 * @date   2020-05-24
 *
 * @brief  Event sequencer for the Report state.  Handles the sequence of modem events that must
 *         be performed when the device has finished Report sending.
 *
 **************************************************************************************************
 */
#ifndef _SEQREPORT_H_
#define _SEQREPORT_H_

/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */

/* Report Type to be used for this sequencer for sending */
typedef enum
{
  REPORT_SELFTEST,
  REPORT_INSTANT_EVENT
} eSeqReportType;

/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
bool  SeqReportSM           (SequencerMessage_t message);

#endif /* _SEQREPORT_H_ */
