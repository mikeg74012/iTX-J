/*
 **************************************************************************************************
 * @file   statistics.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  This module handles device statistics
 *
 **************************************************************************************************
 */

#include "global.h"


/*
 **************************************************************************************************
 *                                         PRIVATE DEFINES
 **************************************************************************************************
 */



/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */


/*
 **************************************************************************************************
 *                                   PRIVATE FUNCTION PROTOTYPES
 **************************************************************************************************
 */
void StatisticsPrint (void);


/*
 **************************************************************************************************
 *                                        PUBLIC VARIABLES
 **************************************************************************************************
 */
Statistics_t Statistics;


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
/**
 * @brief   Initialize the statistics module
 * @param   None
 * @retval  None
 */
void StatisticsInitialize (void)
{
  if (!IsLowPowerSleepFlagUp())
  {
    memset(&Statistics, 0, sizeof(Statistics));
  }
}



/**
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */
/**
 * @brief   Print the statistics to the console
 * @param   None
 * @retval  None
 */
void StatisticsPrint (void)
{
  ConsolePrintf("*** DEVICE STATISTICS ***\n\r");
  ConsolePrintf("Charge Cycles     = %d\n\r", Statistics.Battery.ChargeCycles);
  ConsolePrintf("Last Charge to    = %d%%\n\r", Statistics.Battery.LastChargePercent);
  ConsolePrintf("Last Charge Time  = ");
  RtcPrintTimestamp(Statistics.Battery.LastChargeTimestamp);
  ConsolePrintf("\n\r");
  ConsolePrintf("Last Charge Last  = %ds\r\n", ((Statistics.Battery.LastChargeEndData >>16) & 0xffff));
  ConsolePrintf("Last Charge Added = %d%%\r\n", ((Statistics.Battery.LastChargeEndData >>8) & 0xff));

}



