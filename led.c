/**
 **************************************************************************************************
 * @file   Led.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  Top level Bi-colour LED driver.
 *
 **************************************************************************************************
 */

#include "global.h"


/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */
/*
#define LED_TIMER_TICK_PERIOD_MS        100
#define LED_BATTERY_LOW_ON_TICKS        1       //   1 * 100 mS = 100 mS
#define LED_BATTERY_LOW_REPEAT_TICKS    100     // 100 * 100 mS = 10 seconds
#define LED_BATTERY_OK_ON_TICKS         1       //   1 * 100 mS = 100 mS
#define LED_BATTERY_OK_REPEAT_TICKS     100     // 100 * 100 mS = 10 seconds
#define LED_EMERGENCY_ON_TICKS          1       //   1 * 100 mS = 100 mS
#define LED_EMERGENCY_REPEAT_TICKS      1       //   1 * 100 mS = 100 mS
#define LED_STARTUP_ON_TICKS            3       //   3 * 100 mS = 300 mS
#define LED_STARTUP_REPEAT_TICKS        5      //    5 * 100 mS = 500 mS
*/
#define LED_TIMER_TICK_PERIOD_MS        50
#define LED_BATTERY_LOW_ON_TICKS        1       //   1 * 100 mS = 50 mS
#define LED_BATTERY_LOW_REPEAT_TICKS    200     // 100 * 100 mS = 10 seconds
#define LED_BATTERY_OK_ON_TICKS         1       //   1 * 100 mS = 50 mS
#define LED_BATTERY_OK_REPEAT_TICKS     200     // 100 * 100 mS = 10 seconds
#define LED_EMERGENCY_ON_TICKS          1       //   1 * 100 mS = 50 mS
#define LED_EMERGENCY_REPEAT_TICKS      1       //   1 * 100 mS = 50 mS
#define LED_STARTUP_ON_TICKS            6       //   3 * 100 mS = 300 mS
#define LED_STARTUP_REPEAT_TICKS        10      //   5 * 100 mS = 500 mS
#define LED_TIMER_TEST_PERIOD_MS        125     //   Test Counting at 125ms
#define LED_TEST_DURATIONS              24     //     24 * 125 = 3s  Test for 3s for each LED Color
#define LED_TIMER_TEST_COUNT            240     //   240 * 125 = 30s Default Test Durations 30s
#define LED_GREEN_TEST_COUNT            1       //     1 * 125 = 125ms (Result Period = 250ms @4Hz)
#define LED_RED_TEST_COUNT              4       //     4 * 125 = 500ms (Result Period = 1s    @1Hz)

/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */

TimerHandle_t    LedTimer               = NULL;
TimerHandle_t    LedTestTimer           = NULL;
static bool      m_statusLedInit        = false;
static bool      LedFlashState          = false;
static int       FlashCount             = 0;
static int       FlashTestCount         = 0;
static int       FlashOnTicks           = 0;
static int       FlashRepeatTicks       = 0;
static LedColor_e FlashColour           = UNKNOWN;
static bool      TestOverride           = false;

/* Number of timer ticks to flash the amber LED at startup.
 * 40 x 100mS ticks = 8 flashes of 500mS each */
static int        StartUpFlashTicks       = 40;


/*
 **************************************************************************************************
 *                                   PRIVATE FUNCTION PROTOTYPES
 **************************************************************************************************
 */
static void LedInitialise    (void);
static void LedTimerCallback (xTimerHandle LedTimer);
static void LedTestTimerCallback (xTimerHandle timer);
static void LedOnLL          (LedColor_e color);
static void LedOffLL         (LedColor_e color);
static void LedFlash         (LedColor_e colour, uint32_t LedOnTicks, uint32_t LedRepeatTicks );
static void LedStopFlash     (void);
static void LedON            (LedColor_e color);
static void LedOFF           (LedColor_e color);
static void LedTest          (void);


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
/**
 * @brief     Initialise and configure the status LED
 * @param     None
 * @return    None
 */
void LedInit (void)
{
  if (m_statusLedInit == false)
  {
    LedInitialise();

    /* Create a software Timer for LED */
    LedTimer = xTimerCreate("LedFlashTimer", LED_TIMER_TICK_PERIOD_MS, pdTRUE, (void *)NULL, LedTimerCallback);
    m_statusLedInit = true;
    if (IsLowPowerSleepFlagUp())
    {
      /* For LP sleep, battery checking will flash green or red when wake up */
      StartUpFlashTicks = 0;  /* just enable flash process without delay */
    }
    else
    {
      /* Flash the LED amber to indicate device start up */
      LedFlash(AMBER, LED_STARTUP_ON_TICKS, LED_STARTUP_REPEAT_TICKS);
    }
  }
}


/**
* @brief     Updates the Current Battery status w.r.t Charging/Not charging on LED
* @param     None
* @return    None
*/
void LedUpdateStatus  (void)
{
  ChargingState_e charging = BatteryMonitorGetChargingState();
  uint8_t batteryLevel =  BatteryMonitorGetLevel();

  if (!TestOverride)
  {
    /* if we are starting up, flash the LED amber */
    if (!StartUpFlashTicks)
    {
      /* if the emergency sequence is active, and Voice Call is initiating or in progress, rapid blink the red LED */
      if (SequencerInEmergency() && SeqEmergencyIsInVoiceCallStage())
      {
        LedFlash(RED, LED_EMERGENCY_ON_TICKS, LED_EMERGENCY_REPEAT_TICKS);
      }
      else
      {
        /* If charging is connected */
        switch (charging)
        {
        case CHARGING_COMPLETE:
          /* if the battery level is charged to full when charging is connected, Turn on green LED */
          LedStopFlash();
          LedON(GREEN);
          break;
        case CHARGING_IN_PROCESS:
        case CHARGING_IN_PROCESS_CAPACITY_FULL:
          /* if the battery getting charged but not full, Turn on RED LED */
          LedStopFlash();
          LedON(RED);
          break;
        case NOT_CHARGING:
          /* If charging is not connected */
          if (batteryLevel <= BATTERY_WARNING_THRESHOLD_PERCENT)
          {
            /* If Battery level is Low and NO charging is present, flash the RED Led */
            LedFlash(RED, LED_BATTERY_LOW_ON_TICKS, LED_BATTERY_LOW_REPEAT_TICKS);
          }
          else
          {
            /* If Battery level is OK and NO charging is present, flash the GREEN Led */
            LedFlash(GREEN, LED_BATTERY_OK_ON_TICKS, LED_BATTERY_OK_REPEAT_TICKS);
          }
          break;
        default:
          break;
        }
      }
    }
  }
}


/**
 * @brief     Put the LED module into its sleep state.  LED stays on if USB power is present,
 *            but turns off otherwise
 * @param     None
 * @return    None
 */
void LedSleep (void)
{
  LedStopFlash();
  ChargingState_e chargingstate = BatteryMonitorGetChargingState();

  if (NOT_CHARGING == chargingstate)
  {
    LedOffLL(GREEN);
    LedOffLL(RED);
  }
}


/**
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */

/**
 * @brief     Initialise and configure the status LED
 * @param     None
 * @return    None
 */
static void LedInitialise (void)
{
  gpioconfig_t PinConfig;

  PinConfig.Direction = IO_OUTPUT_PP;
  PinConfig.Pull = IO_PULL_NONE;
  PinConfig.InterruptType = IO_INT_NONE;
  GpioInitPin(GREEN_LED_PORT, GREEN_LED_PIN, &PinConfig);
  GpioInitPin(RED_LED_PORT, RED_LED_PIN, &PinConfig);
}


/**
 * @brief     Turns ON the respective LED
 * @param     color: Led color to be Turned OFF
 * @return    None
 */
static void LedON (LedColor_e color)
{
  LedStopFlash();
  LedOnLL(color);
}


/**
 * @brief     Turns OFF the respective LED
 * @param     color: Led color to be Turned OFF
 * @return    None
 */
static void LedOFF (LedColor_e color)
{
  LedStopFlash();
  LedOffLL(color);
}

/**
 * @brief     Turns LED Flashing for testing purpose, Green at fast rate and Red as slow rate
 * @param     None
 * @return    None
 */
static void LedTest (void)
{
  if (NULL == LedTestTimer)
  {
    /* Create the test timer if not created before */
    LedTestTimer = xTimerCreate("LedTestTimer", LED_TIMER_TEST_PERIOD_MS, pdTRUE, (void *)NULL, LedTestTimerCallback);
    FlashTestCount = 0;

    if (!LedTestTimer)
    {
      TRACE_("SYSTEM", "FaiLed to Create LED Test Timer\n\r");
      return;
    }

    /* Kick off the Timer */
    if (xTimerStart(LedTestTimer, 0) != pdPASS)
    {
      TRACE_("SYSTEM", "FaiLed to Start LED Test Timer\n\r");
    }
  }
  else
  {
    TRACE_("SYSTEM", "Test Timer still counting\n\r");
  }
}

/**
 * @brief     Turns ON the respective LED
 * @param     color: Led color to be Turned ON
 * @return    None
 */
static void LedOnLL (LedColor_e color)
{
  switch (color)
  {
    case RED:
      GpioWritePin(RED_LED_PORT, RED_LED_PIN, IO_PIN_SET);
      GpioWritePin(GREEN_LED_PORT, GREEN_LED_PIN, IO_PIN_RESET);
      break;
    case GREEN:
      GpioWritePin(RED_LED_PORT, RED_LED_PIN, IO_PIN_RESET);
      GpioWritePin(GREEN_LED_PORT, GREEN_LED_PIN, IO_PIN_SET);
      break;
    case AMBER:
      GpioWritePin(RED_LED_PORT, RED_LED_PIN, IO_PIN_SET);
      GpioWritePin(GREEN_LED_PORT, GREEN_LED_PIN, IO_PIN_SET);
      break;
    default:
      break;
  }
}


/**
 * @brief     Turns Off the respective LED
 * @param     color: Led color to be Turned ON
 * @return    None
 */
static void LedOffLL (LedColor_e color)
{
  switch (color)
  {
    case RED:
      GpioWritePin(RED_LED_PORT, RED_LED_PIN, IO_PIN_RESET);
      break;
    case GREEN:
      GpioWritePin(GREEN_LED_PORT, GREEN_LED_PIN, IO_PIN_RESET);
      break;
    case AMBER:
      GpioWritePin(RED_LED_PORT, RED_LED_PIN, IO_PIN_RESET);
      GpioWritePin(GREEN_LED_PORT, GREEN_LED_PIN, IO_PIN_RESET);
      break;
    default:
      break;
  }
}


/**
 * @brief     Flashes the LED with predefined delay
 * @param     colour         : LED colour to be flashed
 *            LedOnTicks     : LED on time in ticks
 *            LedPeriodTicks : Pattern repeat period in ticks
 * @return    None
 */
static void LedFlash (LedColor_e colour, uint32_t LedOnTicks, uint32_t LedRepeatTicks)
{
  /* if the configuration has changed, update and start the timer */
  if ((FlashColour != colour) || (FlashOnTicks != LedOnTicks) || (FlashRepeatTicks != LedRepeatTicks) || (LedFlashState == false))
  {
    /* Configure the flash colour and timings */
    LedFlashState = true;
    FlashColour = colour;
    FlashOnTicks = LedOnTicks;
    FlashRepeatTicks = LedRepeatTicks;
    FlashCount = 0;

    /* start with the LED on */
    LedOnLL(FlashColour);

    /* Start the timer to begin flashing */
    if ( xTimerChangePeriod( LedTimer, LED_TIMER_TICK_PERIOD_MS, 100 ) != pdPASS )
    {
      TRACE_("SYSTEM", "FaiLed to Start LED Timer\n\r");
    }
  }
}


/**
 * @brief   Stop flashing the LEDs
 * @param   None
 * @retval  None
 */
static void LedStopFlash (void)
{
  /* Change the Led flash status */
  if (LedFlashState == true)
  {
    LedFlashState = false;
    /* Stop the Timer if it was previously started to flash the LED */
    xTimerStop( LedTimer, 0 );
  }
}


/**
  * @brief   Led Timer Callback(set w.r.t to the Led flash period defined)
  * @param   LedTimer - Timer Handle
  * @retval  None
  */
static void LedTimerCallback (xTimerHandle LedTimer)
{
  FlashCount++;

  if (FlashCount == FlashOnTicks)
  {
    LedOffLL(FlashColour);
  }
  else if (FlashCount >= FlashRepeatTicks)
  {
    LedOnLL(FlashColour);
    FlashCount = 0;
  }
  if (StartUpFlashTicks)
  {
    StartUpFlashTicks--;
  }
}

/**
  * @brief   Led Test Timer Callback
  *          Execute the test pattern of the LED,
  *          Green LED flash at the faster rate for 3s
  *          Red LED flash at the slower rate for 3s
  *          And total of the test will take place for at most 30s
  * @param   timer - Timer Handle
  * @retval  None
  */
static void LedTestTimerCallback (xTimerHandle timer)
{
  static bool LedOn = false;
  static bool greenLedFlashing = true; // Start with green LED flashing
  static int counter = 0;
  LedColor_e targetLed = GREEN;
  int targetPeriod = LED_GREEN_TEST_COUNT;

  /* Increment counter for each callback call */
  FlashTestCount++;     /* it is for count for whole test period */
  counter++;            /* It is to control the Green or Red Flashing */

  /* Test Count is enough, kill the timer */
  if (FlashTestCount >= LED_TIMER_TEST_COUNT)
  {
    xTimerStop(timer, 0 );
    xTimerDelete(timer, 0);

    /* Reset all the LED related test parameters */
    FlashTestCount = 0;

    /* Ensure the callback is from desired timer and reset it to NULL*/
    if (timer == LedTestTimer)
    {
       LedTestTimer = NULL;
    }

    LedOn = false;
    greenLedFlashing = true;
    counter = 0;

    /* Turn off the LED */
    TestOverride = false;
    LedOffLL(RED);
    LedOffLL(GREEN);
    return;
  }

  /* Toggle the Target LED for flashing */
  if (counter >= LED_TEST_DURATIONS) {
      greenLedFlashing = !greenLedFlashing;
      counter = 0;
  }

  /* Set the target LED to be handled */
  if (greenLedFlashing)
  {
    targetLed = GREEN;
    targetPeriod = LED_GREEN_TEST_COUNT;
  }
  else
  {
    targetLed = RED;
    targetPeriod = LED_RED_TEST_COUNT;
  }

  /* Toggle the LED */
  if (FlashTestCount % targetPeriod == 0)
  {
    LedOn = !LedOn;
    if (LedOn) {
      LedOnLL(targetLed);
    } else {
      LedOffLL(targetLed);
    }
  }

}


/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       Control the LED from the console
 * @param[in]   argCount    : The number of arguments passed from the CLI
 * @param[in]   pArguments  : The list of arguments passed from the CLI
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int LedConsoleLed (int argCount, char **pArguments)
{
  if (1 == argCount)
  {
    if (!strcmp(pArguments[0], "red"))
    {
      TestOverride = true;
      LedON(RED);
    }
    if (!strcmp(pArguments[0], "green"))
    {
      TestOverride = true;
      LedON(GREEN);
    }
    if (!strcmp(pArguments[0], "amber"))
    {
      TestOverride = true;
      LedON(AMBER);
    }
    if (!strcmp(pArguments[0], "test"))
    {
      TestOverride = true;
      LedTest();
    }
    if (!strcmp(pArguments[0], "off"))
    {
      /* Force test counter to finish itself */
      if (FlashTestCount)
      {
        FlashTestCount = LED_TIMER_TEST_COUNT;
      }
      TestOverride = false;
      LedOFF(RED);
      LedOFF(GREEN);
    }
  }
  return CONSOLE_ERROR_OK;
}
