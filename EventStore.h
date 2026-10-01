/**
 ******************************************************************************
 * @file   impactevent.h
 ******************************************************************************
 *
 * @author Ripcord Designs - UA
 * @date   28/04/2020
 *
 * @brief  Impact-Event handling.
 *
 ******************************************************************************
 */

#ifndef IMPACTEVENT_H
#define IMPACTEVENT_H

#include "motionbuffer.h"

/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */

/* Note: A fully loaded mini-event in SRAM contains approximately 122,560 kB with current settings */
#define MAX_MINI_EVENTS                   10


typedef enum
{
  EVENT_STORE_MINI,
  EVENT_STORE_IMPACT
} EventStore_e;


#pragma pack(1)

typedef struct
{
  uint8_t   EventID;      // Used to track mini-event ID to read or write.
  uint32_t  TimeStamp;    // time of the event
  int32_t   Latitude;     // location of the event
  int32_t   Longitude;
} EventSummary_t;

#pragma pack()

/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */
/* API */
void     EventStoreInitialize         (void);
bool     EventStoreWrite              (EventStore_e EventStore, EventSummary_t EventSummary);
bool     EventStoreReadSummary        (EventStore_e EventStore, EventSummary_t * pEventSummary);
bool     EventStoreReadRecords        (EventStore_e EventStore, ImuData_t * pImuData, int numberOfRecords, EventSummary_t * pEventSummary);
bool     EventStorePeekRecords        (EventStore_e EventStore, ImuData_t * pImuData, int numberOfRecords, int offset, EventSummary_t * pEventSummary);
uint32_t EventStoreGetRecordCount     (EventStore_e EventStore);
uint8_t  EventStoreGetEventCount      (EventStore_e EventStore);
bool     EventStoreReadAccelDecimated (EventStore_e EventStore, AccelerationData_t * pBuf, int numberOfRecords, EventSummary_t * pEventSummary);
void     EventStoreDeleteEvent        (EventStore_e EventStore, EventSummary_t * pEventSummary);
uint32_t EventStoreGetImpactForceArea (void);
int      GetImpactEventCount          (void);
int      GetMiniEventIndex            (void);
void     SetImpactEventCount          (int eCount);
void     SetMiniEventIndex            (int eIndex);

/* Console Commands */
int      EventStoreConsoleList        (int ArgCount, char **pArguments);

#endif /*IMPACTEVENT_H */
