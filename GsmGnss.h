/**
 **************************************************************************************************
 * @file   GsmGnss.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  This file implements the function to access the GNSS data from the module
 *
 **************************************************************************************************
 * @attention                       (c) Ripcord Designs Limited
 **************************************************************************************************
 */


#ifndef GSMGNSS_H_
#define GSMGNSS_H_

#include "main.h"
#include <stdbool.h>
#include "time.h"


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
/* flag bit positions used in the GNSS status byte */
#define GNSS_STATUS_FLAG_DATA_VALID     0x01
#define GNSS_STATUS_FLAG_GSM_JAMMED     0x02
#define GNSS_STATUS_FLAG_OVERFLOW       0x04
#define GNSS_STATUS_FLAG_GPS_JAMMED     0x02

/* Precision multipliers for stored data */
#define DEGREE_PRECISION            (0.000001)
#define HEADING_PRECISION           (0.1)
#define SPEED_PRECISION             (0.1)


/* structure definition for storing GNSS data */
#pragma pack(1)
typedef struct GnssData
{
  /*
   * Status is an 8-bit value to hold GNSS status information
   *   b[0]:1 = valid, 0 = invalid
   *   b[1]:1 = jammed, 0 = not jammed
   *   b[2]:0 = reserved
   *   b[3-7] = number of Satellites
   */
  uint8_t   Status;
  uint32_t  TimeStamp;
  int32_t   Latitude;
  int32_t   Longitude;
  uint16_t  Heading;
  uint16_t  Speed;
} GnssData_t;

#pragma pack()


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
/* API */
bool GsmGnssStart                 (void);
bool GsmGnssStop                  (void);
bool GsmGnssGetData               (bool storeToSram);
bool GsmGnssAbort                 (void);
bool GsmGnssDecodeMessageRMC      (char * GnssData);
bool GsmGnssDecodeMessageGGA      (char * GnssData);
bool GsmGnssDecodeMessageGSV      (char * GnssData);
bool GsmGnssDecodeMessageMTK      (char * GnssData);
void GsmGnssInitialFixStatusCheck (void);
bool GsmGnssJamDetectionControl   (bool nswitch);
uint32_t GsmGnssGetTimeStamp      (void);
bool GsmGnssGetCurrentGnss        (GnssData_t *pGnssData);

/* Console commands */
int  GsmGnssConsoleStart      (int argCount, char **pArguments);
int  GsmGnssConsoleStop       (int argCount, char **pArguments);
int  GsmGnssConsoleRead       (int argCount, char **pArguments);
int  GsmGnssConsoleSetRef     (int argCount, char **pArguments);
int  GsmGnssConsoleStatus     (int argCount, char **pArguments);
int  GsmGnssConsoleJammer     (int argCount, char **pArguments);

#endif /* GSMGNSS_H_ */
