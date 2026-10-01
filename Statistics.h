/*
 **************************************************************************************************
 * @file   statistics.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  This module handles device statistics
 *
 **************************************************************************************************
 */

#ifndef _STATISTICS_H_
#define _STATISTICS_H_

#include <stdbool.h>


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
/* System Statistics */
typedef struct
{
  uint32_t   CommissionTimestamp;
  uint32_t   LastBootTimestamp;
  // boot cause
} SystemStats_t;


/* Battery Statistics */
typedef struct
{
  uint32_t   ChargeCycles;
  uint32_t   LastChargeTimestamp;
  uint32_t   LastChargePercent;
  uint32_t   LastChargeEndData;
} BatteryStats_t;


/* Modem Statistics */
typedef struct
{
  uint32_t   HTTPTxMessageCount;  // Do we want to log number of successes / failures ?
  uint32_t   HTTPRxMessageCount;
  uint32_t   SMSTxMessageCount;
  uint32_t   SMSRxMessageCount;
} ModemStats_t;


/* Statistics */
typedef struct
{
  SystemStats_t    System;
  BatteryStats_t   Battery;
  ModemStats_t     Modem;
} Statistics_t;


/*
 **************************************************************************************************
 *                                        PUBLIC VARIABLES
 **************************************************************************************************
 */
extern Statistics_t Statistics;


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
void StatisticsInitialize   (void);
int  StatisticsConsolePrint (int argCount, char **pArguments);


#endif /* _STATISTICS_H_ */
