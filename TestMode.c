/**
 **************************************************************************************************
 * @file   testmode.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   2022-05-27
 *
 * @brief  Factory test mode module.
 *         Test mode is used to prevent the device from performing its normal operations during
 *         factory test.
 *
 *
 * @notes  1. The factory tests involve resetting the module once to test the HW reset function.
 *            In order to maintain test mode through this reset, an RTC backup register is used to
 *            store the TestMode flag.
 *
 *         2. We never want TestMode to be enabled during normal operation, so the TestMode value
 *            is initially set to 2, then decremented with each reset of the device.  The first
 *            reset will decrement the count to 1, thus keeping TestMode active after the factory
 *            test reset. Subsequent resets will decrement the count to 0, thus disabling TestMode.
 *
 *         3. Hibernating the device will always clear the TestMode flag, since the backup
 *            registers are cleared when the power is shut down.
 *
 **************************************************************************************************
 */
#include "global.h"


/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */
#define TESTMODE_INIT_VALUE            2


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */


/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
void        RtcBkupWriteRegister    (uint32_t regAddress, uint32_t data);



/*
 **************************************************************************************************
 *                                   PUBLIC SECTION / INTERFACE
 **************************************************************************************************
 */

/**
 * @brief     Read the test mode flag
 * @param     none
 * @return    bool: flag value (true = test mode, false = normal operation)
 */
bool TestModeGet (void)
{
  uint32_t test_mode_cnt = RtcBkupReadRegister(RTC_BACKUP_REGISTER_TESTMODE);

  if (test_mode_cnt)
  {
    return true;
  }
  else
  {
    return false;
  }
}


/**
 * @brief     Write the test mode flag
 * @param     none
 * @return    bool: true on success, false on failure
 */
void TestModeSet (void)
{
  RtcBkupWriteRegister(RTC_BACKUP_REGISTER_TESTMODE, TESTMODE_INIT_VALUE);
  ConsoleUpdatePrintable();
}



/**
 * @brief   Reset the TestMode Flag in the RTC backup
 * @param   none
 * @return  none
 */
void TestModeClear (void)
{
  RtcBkupWriteRegister(RTC_BACKUP_REGISTER_TESTMODE, 0);
  ConsoleUpdatePrintable();
}


/**
 * @brief   Decrement the count of test mode
 * @param   none
 * @return  none
 */
void TestModeCountDown (void)
{
  /* read the current value from the backup register */
  uint32_t test_mode_cnt = RtcBkupReadRegister(RTC_BACKUP_REGISTER_TESTMODE);

  /* sanity check and clip the value, just in case of error */
  if (test_mode_cnt > TESTMODE_INIT_VALUE)
  {
    test_mode_cnt = 0;
  }

  /* decrement the value and write back to the backup register */
  if (test_mode_cnt)
  {
    RtcBkupWriteRegister(RTC_BACKUP_REGISTER_TESTMODE, test_mode_cnt-1);
  }
  ConsoleUpdatePrintable();
}


/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       Set test mode ON
 * @param[in]   argCount    : The number of arguments passed from the CLI
 * @param[in]   pArguments  : The list of arguments passed from the CLI
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int TestModeConsoleSet (int argCount, char **pArguments)
{
  if (1 == argCount)
  {
    int FlagValue = 0;
    sscanf(pArguments[0], "%d", &FlagValue);
    if (FlagValue)
    {
      TestModeSet();
    }
    else
    {
      TestModeClear();
    }
    ConsolePrintf("TestMode = %i\n\r", TestModeGet());
    return(CONSOLE_ERROR_OK);
  }
  return(CONSOLE_ARG_NO);
}



/**
 * @brief       Set test mode OFF
 * @param[in]   argCount    : The number of arguments passed from the CLI
 * @param[in]   pArguments  : The list of arguments passed from the CLI
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int TestModeConsoleGet (int argCount, char **pArguments)
{
  if (0 == argCount)
  {
    ConsolePrintf("TestMode = %i\n\r", TestModeGet());
    return(CONSOLE_ERROR_OK);
  }
  return(CONSOLE_ARG_NO);
}

