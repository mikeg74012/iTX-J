/**
 **************************************************************************************************
 * @file   gnssevent.h
 **************************************************************************************************
 *
 * @author Ripcord Designs - UA
 * @date   15/05/2020
 *
 * @brief  Header File - Handles GNSS-Event storage and retrieve from SRAM.
 *
 **************************************************************************************************
 */
#ifndef GNSSEVENT_H
#define GNSSEVENT_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "gsmgnss.h"


/*
 **************************************************************************************************
 *                                       PUBLIC DEFINES
 **************************************************************************************************
 */

#pragma pack(1)
typedef struct GnssEventMotionData
{
   GnssData_t               gnssData;
   AccelerationData_t       accelData;
} GnssEventMotionData_t;

#pragma pack()

/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */
/* API */
void     GnssEventWrite              (GnssEventMotionData_t *pBuffer);
bool     GnssEventRead               (GnssEventMotionData_t *pBuffer);
uint32_t GnssEventGetRecordCount     (void);
uint32_t GnssEventGetCountSize       (void);
uint32_t GnssEventWriteIndex         (void);
bool     GnssEventReadLatest         (GnssEventMotionData_t *pBuffer);
bool     GnssEventPeek               (GnssEventMotionData_t *pBuffer);
void     GnssEventReset              (void);
bool     GnssEventGet                (uint32_t gnssEventReadIndex, GnssEventMotionData_t *pBuffer);
void     GnssEventSaveToSram         (void);

/* Console Commands */
int      GnssEventConsoleList        (int ArgCount, char **pArguments);

#endif /*GNSSEVENT_H */
