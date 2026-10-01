/**
 **************************************************************************************************
 * @file   main.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   30/10/2019
 *
 * @brief  Main software application for the iTelematiX V4 Vehicle Telematics Device.  This version
 *         is for GigaDevice GD32F303RE MCU.
 *
 **************************************************************************************************
 */
#include "global.h"


/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */
#define VECT_TAB_OFFSET 0x10000


/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */


/*
 **************************************************************************************************
 *                                         PUBLIC SECTION
 **************************************************************************************************
 */

unsigned long console_stack[CONSOLE_STACK_SIZE / 4];
unsigned long button_monitor_stack[BUTTON_MONITOR_STACK_SIZE / 4];
unsigned long battery_monitor_stack[BATTERY_MONITOR_STACK_SIZE / 4];
unsigned long motion_monitor_stack[MOTION_MONITOR_STACK_SIZE / 4];
unsigned long sequencer_stack[SEQUENCER_STACK_SIZE / 4];
unsigned long modem_stack[MODEM_STACK_SIZE / 4];
unsigned long gsm_comms_stack[GSM_COMMS_STACK_SIZE / 4];
unsigned long gnss_comms_stack[GNSS_COMMS_STACK_SIZE / 4];
unsigned long wifi_stack[WIFI_STACK_SIZE / 4];


/**
 * @brief  The application entry point.
 * @param  none
 * @retval int (not used)
 */
int main (void)
{
  uartconfig_t    UartConfig;
  stream_t *      pStream;
  
  /* Check if the system has resumed from WWDG reset */
  eResetCause_t ResetCause = PowerGetResetCause();
  if ((ResetCause == RESET_WATCHDOGWIN) || (ResetCause == RESET_WATCHDOGIDG))
  {
    watchdogResetDetected = true;
  }

  /* Configure the system clock */
  SystemClockInit();

  /* Enable the Free Hardware watch Dog */
  SystemFreeWatchDogEnable();

  /* configure the systick handler priority */
  
// Should already be 0   NVIC_SetPriority(SysTick_IRQn, 0x00U);

  /* initialize the watchdog module */
  WatchdogInitialize();

  /* initialise the real time clock */
  RtcInit();

  /* ied Init after RTC is ready */
  LedInit();

  if (ResetCause == RESET_UNKNOWN)
  {
    /* If still have an unknown reset cause, Read the RTC bkup register and reset afterward */
    ResetCause = PowerGetResetCauseFromRTCBkupRegister();
  }

  if ((ResetCause == RESET_HIBERNATE) || (ResetCause == RESET_FW_UPDATE))
  {
    /* reset the Standby sleep flag for a normal boot */
    ResetLPSleepStatus(RTC_BKUP_STANDBYSLEEP_FLAG);
  }

  /* Decrement the count of test mode */
  TestModeCountDown();

  /* Initialise the Console */
  UartConfig.BaudRate = 460800;
  UartConfig.DataBits = COM_DATABITS_8;
  UartConfig.StopBits = COM_STOPBITS_1;
  UartConfig.Parity   = COM_PARITY_NONE;
  UartConfig.FlowControl = COM_FLOWCONTROL_NONE;
  pStream = UARTInitialise(CONSOLE_UART, &UartConfig);
  ConsoleInitialise(pStream);

  /* Initialise the serial SRAM */
  SerialRamInit();

  /* SPI Ram Ready, restore the system data if it is boot from low power sleep */
  if (IsLowPowerSleepFlagUp())
  {
    ReadResetDataFromSpiRAM();  /* first read PSRAM after RAM SPI when it ready */

    /* Restore the Fitment related data*/
    FitmentDataHandler(MOUNT_SURVEY_IMPORT);
  }

  /* Init the fitment matrix */
  FitmentMatrixInit();

  /* Initialise status log and LED */
  StatusLogInitialize();

  /* set the exception vector table location */
  nvic_vector_table_set(NVIC_VECTTAB_FLASH, VECT_TAB_OFFSET);  /* set vector offset */

  set_irq_priority (41, 6);
  enable_irq (41);          /* Enable RTC alarm interrupt */
  
  or_register (RCU, RCU_AHBEN, (1 << 6));       /* enable CRC clock */
  or_register (RCU, RCU_APB1EN, (1 << 28));     /* enable PMU clock */
  or_register (RCU, RCU_APB2EN, (1 << 0));      /* alternate function clock */

  or_register (PMU, PMU_CTL, (1 << 2));         /* clear wake up flag*/
  and_register (RTC, RTC_CTL, ~(1 << 1));       /* clear the RTC second interrupt flag*/

  /* Clear the Reset flag */
  if (ResetCause == RESET_LOW_BATTERY)
  {
    ConfigResetLowBatFlag();
  }

  /* On first boot, we initialise the configuration data store module */
  if (ConfigCheckValidData() != true)
  {
    /* Set some sensible defaults */
    ConfigSetDefaults();
  }

  /* Reset the customer ID to zero if boot from hibernate */
  if (ResetCause == RESET_HIBERNATE)
  {
    ConfigSetCustomer(0);
  }

  /* Initialise the device statistics */
  StatisticsInitialize();

  /* Initialise the GPRS Message Queue */
  MessageQueueInitialise();

  /* Carry out WIFI Wake-Up Self-Test if required */
  PostWakeUpSelfTest(WIFI);

  /* Initialise the I2C bus before the threads start */
  I2cInitialise(I2C_BUS_0, 400000);

  /* hardware version detection */
  HardwareVersionCheck();

  /* Reload the watchdog before the task creation */
  SystemFreeWatchDogReload();

  /*
   * Create the threads
   */

  init_threads ();
  
  /* Console Thread */
  add_thread ((void (*) (void)) ConsoleTask, &console_stack[(sizeof (CONSOLE_STACK_SIZE) / 4) - 1]);
  
  /* Button Monitor Thread */
  add_thread ((void (*) (void)) ButtonMonitorTask, &button_monitor_stack[(sizeof (BUTTON_MONITOR_STACK_SIZE) / 4) - 1]);
  
  /* Battery Monitor Thread */
  add_thread ((void (*) (void)) BatteryMonitorTask, &battery_monitor_stack[(sizeof (BATTERY_MONITOR_STACK_SIZE) / 4) - 1]);

  /* Motion Monitor Thread */
  add_thread ((void (*) (void)) MotionMonitorTask, &motion_monitor_stack[(sizeof (MOTION_MONITOR_STACK_SIZE) / 4) - 1]);

  /* Sequencer Thread */
  add_thread ((void (*) (void)) SequencerTask, &sequencer_stack[(sizeof (SEQUENCER_STACK_SIZE) / 4) - 1]);

  /* GSM / GNSS Modem Threads */
  add_thread ((void (*) (void)) ModemTask, &modem_stack[(sizeof (MODEM_STACK_SIZE) / 4) - 1]);

  add_thread ((void (*) (void)) GsmCommsTask, &gsm_comms_stack[(sizeof (GSM_COMMS_STACK_SIZE) / 4) - 1]);

#if MODEM_SEPARATE_GNSS_UART
  add_thread ((void (*) (void)) GnssCommsTask, &gnss_comms_stack[(sizeof (GNSS_COMMS_STACK_SIZE) / 4) - 1]);
#endif

  /* WiFi Thread */

  add_thread ((void (*) (void)) WifiTask, &wifi_stack[(sizeof (WIFI_STACK_SIZE) / 4) - 1]);
  
  while (1)
  {
    sleep (0);
  }
}


/**
 * @brief  System Clock Configuration
 * @note   Called during system startup AND each time the device wakes from sleep
 * @retval None
 */
void SystemClockInit (void)
{
 /* GD will call the SystemInit() from the start up .S code, it include in system_gd32f30x.c file
 * it will setup the microcontroller system, initialize the system clock
 * inside the file, system clock is selected by the macro define in IRC8M section, From 8M to 120M from the PLL
 * __SYSTEM_CLOCK_48M_PLL_IRC8M is selected
 */
  SystemInit(); /* setup system clock */
}


/**
 * @brief  This function is executed in case of error occurrence.
 * @param  None
 * @retval None
 */
void Error_Handler (void)
{
  /* User can add his own implementation to report the HAL error return state */
}


/*
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed (char *file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
  tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
}
#endif /* USE_FULL_ASSERT */


/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       Print stack usage details to console
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int SystemConsoleStack (int ArgCount, char **pArguments)
{
/*
  ConsolePrintf("TASK NAME        STK-REM STK-MAX(bytes)\n\r");
  ConsolePrintf("Button Monitor   %4lu    %4lu\n\r", uxTaskGetStackHighWaterMark(buttonMonitorTaskHandle),  BUTTON_MONITOR_STACK_SIZE);
  ConsolePrintf("Battery Monitor  %4lu    %4lu\n\r", uxTaskGetStackHighWaterMark(batteryMonitorTaskHandle), BATTERY_MONITOR_STACK_SIZE);
  ConsolePrintf("Motion Monitor   %4lu    %4lu\n\r", uxTaskGetStackHighWaterMark(motionMonitorTaskHandle),  MOTION_MONITOR_STACK_SIZE);
  ConsolePrintf("Console          %4lu    %4lu\n\r", uxTaskGetStackHighWaterMark(consoleTaskHandle),        CONSOLE_STACK_SIZE);
  ConsolePrintf("Sequencer        %4lu    %4lu\n\r", uxTaskGetStackHighWaterMark(sequencerTaskHandle),      SEQUENCER_STACK_SIZE);
  ConsolePrintf("Modem            %4lu    %4lu\n\r", uxTaskGetStackHighWaterMark(modemTaskHandle),          MODEM_STACK_SIZE);
  ConsolePrintf("GSM Comms        %4lu    %4lu\n\r", uxTaskGetStackHighWaterMark(gsmCommsTaskHandle),       GSM_COMMS_STACK_SIZE);
  ConsolePrintf("GNSS Comms       %4lu    %4lu\n\r", uxTaskGetStackHighWaterMark(gnssCommsTaskHandle),      GNSS_COMMS_STACK_SIZE);
  ConsolePrintf("WiFi             %4lu    %4lu\n\r", uxTaskGetStackHighWaterMark(wifiTaskHandle),           WIFI_STACK_SIZE);
*/
  /* indicate success */
  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       Reset / reboot the device
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int SystemConsoleReset (int ArgCount, char **pArguments)
{
  /* only allow reset during system test */
  if (TestModeGet())
  {
    /*
     * Reboot the MCU
     * WARNING - Never returns
     */
    PowerResetMCU();
  }

  ConsolePrintf("Only available in test mode\n\r");

  /* indicate success */
  return (CONSOLE_ERROR_OK);
}

