/**
 **************************************************************************************************
 * @file   gpio.c
 *
 * @author Ripcord Designs
 *
 * @brief  This module provides an abstraction layer for the GD32 on-chip GPIO.
 *         It shares a common Ripcord API with GPIO modules for other chips, so higher software
 *         layers can be easily moved between different MCU's.
 *
 **************************************************************************************************
 * @attention                  (c) Ripcord Designs Limited 2022
 **************************************************************************************************
 */
#include "global.h"

/*
 **************************************************************************************************
 *                                        PUBLIC VARIABLES
 **************************************************************************************************
 */


/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */
#define EXTI_TRIG_NONE 0xFF


/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static uint8_t ConsoleSetGpioPin    (char * pCmd);
static uint8_t ConsoleGetGpioPin    (char * pCmd);

/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
/* Table to convert between generic GPIO port ID and STM32 specific GPIO port ID */
uint32_t PortLookup [] = {GPIOA, GPIOB, GPIOC, GPIOD};


/*
 **************************************************************************************************
 *                                   PUBLIC SECTION / INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       Initialise a GPIO pin.  Must be performed before a pin is used.
 * @param[in]   Port   : The GPIO port to initialise
 * @param[in]   Pin    : The GPIO pin to initialise
 * @param[in]   Config :
 * @return      none
 */
void GpioInitPin (gpioport_e Port, gpiopin_e Pin, gpioconfig_t * pConfig)
{
  uint32_t Mode = GPIO_MODE_IN_FLOATING;

  /* Enable the GPIO port clock */
  switch (Port)
  {
  case IO_PORT_A:
      rcu_periph_clock_enable(RCU_GPIOA);
	  break;
  case IO_PORT_B:
      rcu_periph_clock_enable(RCU_GPIOB);
	  break;
  case IO_PORT_C:
      rcu_periph_clock_enable(RCU_GPIOC);
	  break;
  case IO_PORT_D:
      rcu_periph_clock_enable(RCU_GPIOD);
	  break;
  }

  /* Convert the generic pin direction to GD32 HAL format */
  switch (pConfig->Direction)
  {
  case IO_OUTPUT_PP:
	  Mode = GPIO_MODE_OUT_PP;
	  break;
  case IO_OUTPUT_OD:
	  Mode = GPIO_MODE_OUT_OD;
	  break;
  case IO_INPUT_DIGITAL:
	  switch (pConfig->Pull)
	  {
	  case IO_PULL_NONE:
	    Mode = GPIO_MODE_IN_FLOATING;
	    break;
	  case IO_PULL_DOWN:
	    Mode = GPIO_MODE_IPD;
	    break;
	  case IO_PULL_UP:
	    Mode = GPIO_MODE_IPU;
	    break;
	  }
	  break;
  case IO_INPUT_ANALOG:
	  Mode = GPIO_MODE_AIN;
	  break;
  }

  /* initialise the pin */
  gpio_init(PortLookup[Port], Mode, GPIO_OSPEED_2MHZ, (uint32_t) Pin);
}


/**
 * @brief       De-initialise a GPIO pin.
 * @param[in]   Port   : The GPIO port to de-initialise
 * @param[in]   Pin    : The GPIO pin to de-initialise
 * @return      none
 */
void GpioDeInitPin (gpioport_e Port, gpiopin_e Pin)
{
  gpio_init(PortLookup[Port], GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, (uint32_t)Pin);

}


/**
 * @brief       Set the state of a single GPIO pin
 * @param[in]   Port  : The GPIO port to set
 * @param[in]   Pin   : The GPIO pin to set
 * @param[in]   Value : The value to set (true / high or false / low)
 * @return      none
 */
void GpioWritePin (gpioport_e Port, gpiopin_e Pin, gpiostate_e Value)
{
  gpio_bit_write(PortLookup[Port], (uint32_t)Pin, (bit_status) Value);
}


/**
 * @brief       Get the state of a single GPIO pin
 * @param[in]   Port  : The GPIO port to get
 * @param[in]   Pin   : The GPIO pin to get
 * @return      Value : The current value of the pin
 *  (true / high or false / low)
 */
gpiostate_e GpioReadPin (gpioport_e Port, gpiopin_e Pin)
{
  return (gpiostate_e) gpio_input_bit_get(PortLookup[Port], (uint32_t)Pin);
}


/**
 * @brief       Set GPIO configuration parameters to the interrupt line
 * @param[in]   Port  : The GPIO port to set
 * @param[in]   Pin   : The GPIO pin to set
 * @param[in]   pInterruptConfig : config of the interrupt specific
 * @return      none
 */
void GpioConfigInterrupt (gpioport_e Port, gpiopin_e Pin, gpioexti_t * pInterruptConfig)
{
  uint8_t n_irq = 0;
  uint8_t SourcePort = 0;
  uint8_t SourcePin = 0;
  uint8_t trig_type = EXTI_TRIG_NONE;


  /* Map the irq number according to pin and Port */
  switch (Pin)
  {
    case IO_PIN_0:
    	n_irq = EXTI0_IRQn;
    	SourcePin = GPIO_PIN_SOURCE_0;
    	break;
    case IO_PIN_1:
    	n_irq = EXTI1_IRQn;
    	SourcePin = GPIO_PIN_SOURCE_1;
    	break;
    case IO_PIN_2:
    	n_irq = EXTI2_IRQn;
    	SourcePin = GPIO_PIN_SOURCE_2;
    	break;
    case IO_PIN_3:
    	n_irq = EXTI3_IRQn;
    	SourcePin = GPIO_PIN_SOURCE_3;
    	break;
    case IO_PIN_4:
    	n_irq = EXTI4_IRQn;
    	SourcePin = GPIO_PIN_SOURCE_4;
    	break;
    case IO_PIN_5:
        n_irq = EXTI5_9_IRQn;
    	SourcePin = GPIO_PIN_SOURCE_5;
    	break;
    case IO_PIN_6:
        n_irq = EXTI5_9_IRQn;
    	SourcePin = GPIO_PIN_SOURCE_6;
    	break;
    case IO_PIN_7:
        n_irq = EXTI5_9_IRQn;
    	SourcePin = GPIO_PIN_SOURCE_7;
    	break;
    case IO_PIN_8:
        n_irq = EXTI5_9_IRQn;
    	SourcePin = GPIO_PIN_SOURCE_8;
    	break;
    case IO_PIN_9:
        n_irq = EXTI5_9_IRQn;
    	SourcePin = GPIO_PIN_SOURCE_9;
      break;
    case IO_PIN_10:
        n_irq = EXTI10_15_IRQn;
    	SourcePin = GPIO_PIN_SOURCE_10;
        break;
    case IO_PIN_11:
        n_irq = EXTI10_15_IRQn;
    	SourcePin = GPIO_PIN_SOURCE_11;
        break;
    case IO_PIN_12:
        n_irq = EXTI10_15_IRQn;
    	SourcePin = GPIO_PIN_SOURCE_12;
        break;
    case IO_PIN_13:
        n_irq = EXTI10_15_IRQn;
    	SourcePin = GPIO_PIN_SOURCE_13;
        break;
    case IO_PIN_14:
        n_irq = EXTI10_15_IRQn;
    	SourcePin = GPIO_PIN_SOURCE_14;
        break;
    case IO_PIN_15:
        n_irq = EXTI10_15_IRQn;
  	    SourcePin = GPIO_PIN_SOURCE_15;
      break;
    default:
      n_irq = 0;
   }

  /* Map GD32 IO Port */
  switch (Port)
  {
  case IO_PORT_A:
	SourcePort = GPIO_PORT_SOURCE_GPIOA;
    break;
  case IO_PORT_B:
	SourcePort = GPIO_PORT_SOURCE_GPIOB;
    break;
  case IO_PORT_C:
	SourcePort = GPIO_PORT_SOURCE_GPIOC;
    break;
  case IO_PORT_D:
	SourcePort = GPIO_PORT_SOURCE_GPIOD;
    break;
  }

  /* Map GD32 Trigger Type */
  switch (pInterruptConfig->InterruptType)
  {
  case IO_INT_EDGE_RISING:
      trig_type = EXTI_TRIG_RISING;
      break;
  case IO_INT_EDGE_FALLING:
      trig_type = EXTI_TRIG_FALLING;
      break;
  case IO_INT_EDGE_BOTH:
      trig_type = EXTI_TRIG_BOTH;
      break;
  case IO_INT_NONE:
  default:
	  trig_type = EXTI_TRIG_NONE;
      break;
  }

  uint32_t ExtiLine = BIT(SourcePin);

  /* config pin as EXTI interrupt if irq setup correctly */
  if((n_irq) && (trig_type != EXTI_TRIG_NONE))
  {
    /* Enable the interrupt with priority */
    nvic_irq_enable(n_irq, pInterruptConfig->irqPrePriority, pInterruptConfig->irqSubPriority);

    /* configure the interrupt source port */
    gpio_exti_source_select(SourcePort, SourcePin);

    /* configure pin EXTI line */
    exti_init(ExtiLine, EXTI_INTERRUPT, trig_type );
    exti_interrupt_flag_clear(ExtiLine);
  }
}

/**
 * @brief       Enable the interrupt line
 * @param[in]   Port  : The GPIO port to set
 * @param[in]   Pin   : The GPIO pin to set
 * @return      none
 */
void GpioEnableInterrupt (gpioport_e Port, gpiopin_e Pin)
{
  uint32_t ExtiLine = BIT(Pin);
  exti_interrupt_flag_clear(ExtiLine);
  exti_interrupt_enable(ExtiLine);
}


/**
 * @brief       Disable the interrupt line
 * @param[in]   Port  : The GPIO port to set
 * @param[in]   Pin   : The GPIO pin to set
 * @return      none
 */
void GpioDisableInterrupt (gpioport_e Port, gpiopin_e Pin)
{
  uint32_t ExtiLine = BIT(Pin);
  exti_interrupt_flag_clear(ExtiLine);
  exti_interrupt_disable(ExtiLine);
}


/*
 **************************************************************************************************
 *                                          PRIVATE SECTION
 **************************************************************************************************
 */

/**
 * @brief       set a pin state, sample a3=1, b12=0
 * @param[in]   pCmd  : console cmd string
 *
 * @return      > 0 indicate cmd parse error
 */
static uint8_t ConsoleSetGpioPin (char *pCmd)
{
  gpioport_e PortToSet;
  gpiopin_e  PinToSet;
  gpiostate_e ValueToSet;

  uint8_t InpuErr = 0;

   switch (pCmd[0])
  {
    case 'a':
    case 'A':
      PortToSet = IO_PORT_A; break;
    case 'b':
    case 'B':
      PortToSet = IO_PORT_B; break;
    case 'c':
    case 'C':
      PortToSet = IO_PORT_C; break;
    case 'd':
    case 'D':
      PortToSet = IO_PORT_D; break;
    default:
      InpuErr ++;
      break;
  }

  uint8_t pin = atoi (&pCmd[1]);
  if( (0 <= pin) && (pin <= 15))
  {
    PinToSet = BIT(pin);
  }
  else
  {
    InpuErr ++;
  }

  char *p = strchr(pCmd, '=');
  uint8_t pinvalue = atoi (++p);
  if( (0 <= pinvalue) && (pinvalue <= 1))
  {
    ValueToSet = (pinvalue == 0)? IO_PIN_RESET : IO_PIN_SET;
  }
  else
  {
    InpuErr ++;
  }

  if (InpuErr == 0)
  {
    GpioWritePin(PortToSet, PinToSet, ValueToSet);
  }
  return (InpuErr);
}


/**
 * @brief       Get a pin state, usage: get a3
 * @param[in]   pCmd  : console cmd string
 *
 * @return      pin state or  > 10 indicate cmd parse error
 */
static uint8_t ConsoleGetGpioPin (char *pCmd)
{
  gpioport_e PortToGet;
  gpiopin_e  PinToGet;
  gpiostate_e PinValue;
  uint8_t pin = 0;

  uint8_t InpuErr = 10;

  switch (pCmd[0])
  {
    case 'a':
    case 'A':
      PortToGet = IO_PORT_A; break;
    case 'b':
    case 'B':
      PortToGet = IO_PORT_B; break;
    case 'c':
    case 'C':
      PortToGet = IO_PORT_C; break;
    case 'd':
    case 'D':
      PortToGet = IO_PORT_D; break;
    default:
      InpuErr ++;
      break;
  }

  pin = atoi (&pCmd[1]);
  if( (0 <= pin) && (pin <= 15))
  {
    PinToGet = BIT(pin);
  }
  else
  {
    InpuErr ++;
  }

  if (InpuErr == 10)  /* no cmd error */
  {
    PinValue = GpioReadPin(PortToGet, PinToGet);
    return (PinValue);
  }
  return (InpuErr);
}


/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */

/**
 * @brief       Set the state of an output pin
 * @param[in]   argCount    : The number of arguments passed from the console
 * @param[in]   pArguments  : The list of arguments passed from the console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int GpioConsoleSet (int ArgCount, char **pArguments)
{
  if (1 == ArgCount)
  {
    if ( ConsoleSetGpioPin(pArguments[0]) == 0)
    {
      /* indicate success */
      return(CONSOLE_ERROR_OK);
    }
    else
    {
      ConsolePrintf("Usage: set <port><pin>=0/1 \r\n");
      ConsolePrintf("       it can be: set a1=1 or set B3=0 \r\n");
      return(CONSOLE_CMD_UNKNOWN);
    }
  }
  else
  {
    ConsolePrintf("Usage: set <port><pin>=0/1 \r\n");
    ConsolePrintf("       it can be: set a1=1 or set B3=0 \r\n");
    return(CONSOLE_ARG_NO);
  }
}


/**
 * @brief       Get the state of an input pin
 * @param[in]   argCount    : The number of arguments passed from the console
 * @param[in]   pArguments  : The list of arguments passed from the console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int GpioConsoleGet (int ArgCount, char **pArguments)
{
  uint8_t RtnValue = 0;
  if (1 == ArgCount)
  {
    RtnValue = ConsoleGetGpioPin(pArguments[0]);
    if ( RtnValue < 10)
    {
      ConsolePrintf("  -> %d \r\n", RtnValue);
      /* indicate success */
      return(CONSOLE_ERROR_OK);
    }
    else
    {
      ConsolePrintf("Usage: get <port><pin> \r\n");
      ConsolePrintf("       sample: get a1 or get B3 \r\n");
      return(CONSOLE_CMD_UNKNOWN);
    }
  }
  else
  {
    ConsolePrintf("Usage: get <port><pin> \r\n");
    ConsolePrintf("       sample: get a1 or get B3 \r\n");
    return(CONSOLE_ARG_NO);
  }
}


