/**
 **************************************************************************************************
 * @file   testmode.h
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
#ifndef TESTMODE_H
#define TESTMODE_H


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */


/*
 **************************************************************************************************
 *                                       PUBLIC VARIABLES
 **************************************************************************************************
 */


/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */
/* API */
bool TestModeGet        (void);
void TestModeSet        (void);
void TestModeClear      (void);
void TestModeCountDown  (void);

/* Console Commands */
int  TestModeConsoleSet (int argCount, char **pArguments);
int  TestModeConsoleGet (int argCount, char **pArguments);


#endif /* TESTMODE_H */
