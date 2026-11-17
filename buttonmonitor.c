/**
 **************************************************************************************************
 * @file   buttonmonitor.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   29/10/2019
 *
 * @brief  Button Monitor Thread main file.
 *
 **************************************************************************************************
 */

#include "global.h"


/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */
/* Button Press delay to avoid false press */
#define BUTTON_PRESS_DELAY_MS  1000
#define BUTTON_DEBOUNCE_MS     100

/* Time for RedButton MAX Hold Time before hardware reset */
#define BUTTON_REDBUTTON_MAX_HOLD_TIME   ((18 * 1000) / MM_TIMER_PERIOD_MS)  // 18 seconds in MM timer ticks


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
static bool ButtonDown = false;
static uint32_t ButtonHoldCount = 0;

/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static void ButtonMonitorInitButton (void);


/*
 **************************************************************************************************
 *                                   PUBLIC SECTION / INTERFACE
 **************************************************************************************************
 */
/*
 * @brief     The Button Monitor Task function
 * @param     (not used)
 * @return    None
 */
void ButtonMonitorTask (void const * argument)
{
  uint32_t tick0= 0;
  ButtonMonitorInitButton();

  
  /* Infinite loop */
  for (;;)
  {
    WatchdogSetWaiting(BUTTONMONITORTASK);
    
    WatchdogSetActive(BUTTONMONITORTASK);

    /* now check the button state */
    if (false == ButtonDown)
    {
        if (!read_gpio (USER_BUTTON))
        {
            sleep(BUTTON_DEBOUNCE_MS);  /* allow some de-bounce time */
            
            if (!read_gpio (USER_BUTTON))
            {
                ButtonDown = true;
                TRACE_("BUTTON", "Button Down\n\r");
                tick0 = csGetTick();
                SequencerButtonDown();
            }
        }
    }
    else
    {
        if (read_gpio(USER_BUTTON))
        {
            sleep(BUTTON_DEBOUNCE_MS);  /* allow some de-bounce time */
            
            if (read_gpio(USER_BUTTON))
            {
                ButtonHoldCount = 0;

                if ((csGetTick() - tick0) >= (BUTTON_PRESS_DELAY_MS - (2 * BUTTON_DEBOUNCE_MS)))
                {
                    SequencerButtonPress();
                }
                
                ButtonDown = false;
            }
        }
    }
  }
}


/**
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */
/*
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void ButtonMonitorInitButton (void)
{
  
  set_gpio_input (USER_BUTTON);
  
#if 0
  /* interrupt init */
  InterruptConfig.irqPrePriority = 5;
  InterruptConfig.irqSubPriority = 0;
  InterruptConfig.InterruptType = IO_INT_EDGE_BOTH;
  GpioConfigInterrupt(RED_BUTTON_PORT, RED_BUTTON_PIN, &InterruptConfig);

  /* Enable the interrupt */
  GpioEnableInterrupt(RED_BUTTON_PORT, RED_BUTTON_PIN);
#endif
}


/*
 * @brief   Red Button Press Callback
 * @param   None
 * @retval  None
 */
void ButtonMonitorPressCallback (void)
{
}


/*
 * @brief   Red Button Hardware line status (No Debouncing)
 * @param   None
 * @retval  True if line is low, otherwise False
 */
bool ButtonMonitorIsButtonDown (void)
{
  return ButtonDown;
}


/*
 * @brief   Increment the RB Hold Count, in scale of MM_TIMER_PERIOD_MS
 *          set the flag in watchDog Store if more than the Threshold
 * @param   None
 * @retval  None
 */
void ButtonMonitorHoldCountInc(void)
{
  if (ButtonDown)
    ButtonHoldCount++;

  if (ButtonHoldCount == BUTTON_REDBUTTON_MAX_HOLD_TIME)
  {
    /* Set the key press Long hold Flag in the reset backup register*/
    SetBackupResetRegister (RESET_RB_LHOLD);
  }
}


/*
 * @brief  update red button status
 * @param   bool: red button status
 * @retval  True if line is low, otherwise False
 */
bool SetButtonDownFlag (bool state)
{
  ButtonDown = state;
  return ButtonDown;
}

