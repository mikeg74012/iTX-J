/**
 **************************************************************************************************
 * @file   gpio.h
 *
 * @author Ripcord Designs
 *
 * @brief  This module provides an abstraction layer for the on-chip GPIO.
 *         It shares a common Ripcord API with GPIO modules for other chips, so higher software
 *         layers can be easily moved between different MCU's.
 *
 **************************************************************************************************
 * @attention                  (c) Ripcord Designs Limited 2022
 **************************************************************************************************
 */
#ifndef _GPIO_H_
#define _GPIO_H_


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
/*
 * GPIO ports available on this chip.  The application layer must use one of these
 * port definitions when initialising or accessing the port
 */
typedef enum
{
  IO_PORT_A = 0,
  IO_PORT_B,
  IO_PORT_C,
  IO_PORT_D,
} gpioport_e;

/* GPIO pin definitions */
typedef enum
{
  IO_PIN_0  = 0x0001U,
  IO_PIN_1  = 0x0002U,
  IO_PIN_2  = 0x0004U,
  IO_PIN_3  = 0x0008U,
  IO_PIN_4  = 0x0010U,
  IO_PIN_5  = 0x0020U,
  IO_PIN_6  = 0x0040U,
  IO_PIN_7  = 0x0080U,
  IO_PIN_8  = 0x0100U,
  IO_PIN_9  = 0x0200U,
  IO_PIN_10 = 0x0400U,
  IO_PIN_11 = 0x0800U,
  IO_PIN_12 = 0x1000U,
  IO_PIN_13 = 0x2000U,
  IO_PIN_14 = 0x4000U,
  IO_PIN_15 = 0x8000U,
} gpiopin_e;


/* GPIO pin states */
typedef enum
{
  IO_PIN_RESET = 0,
  IO_PIN_SET
} gpiostate_e;


/* GPIO pin Direction */
typedef enum
{
  IO_OUTPUT_PP = 0,
  IO_OUTPUT_OD,
  IO_INPUT_DIGITAL,
  IO_INPUT_ANALOG
} gpiodirection_e;


/* Pull types */
typedef enum
{
  IO_PULL_NONE = 0,
  IO_PULL_DOWN,
  IO_PULL_UP,
} gpiopull_e;

/* Interrupt types */
typedef enum
{
  IO_INT_NONE,
  IO_INT_EDGE_RISING,
  IO_INT_EDGE_FALLING,
  IO_INT_EDGE_BOTH,
} gpiointtype_e;


typedef struct
{
  gpiointtype_e   InterruptType;
  uint8_t         irqPrePriority;
  uint8_t         irqSubPriority;
} gpioexti_t;


typedef struct
{
  gpiodirection_e Direction;
  gpiopull_e      Pull;
  gpiointtype_e   InterruptType;
} gpioconfig_t;

/*
 **************************************************************************************************
 *                                        GLOBAL VARIABLES
 **************************************************************************************************
 */


/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */
/* API */
void           GpioInitPin          (gpioport_e Port, gpiopin_e Pin, gpioconfig_t * pConfig);
void           GpioConfigInterrupt  (gpioport_e Port, gpiopin_e Pin, gpioexti_t * pInterruptConfig);
void           GpioEnableInterrupt  (gpioport_e Port, gpiopin_e Pin);
void           GpioDisableInterrupt (gpioport_e Port, gpiopin_e Pin);
void           GpioDeInitPin        (gpioport_e Port, gpiopin_e Pin);
void           GpioWritePin         (gpioport_e Port, gpiopin_e Pin, gpiostate_e Value);
gpiostate_e    GpioReadPin          (gpioport_e Port, gpiopin_e Pin);

/* Console Commands */
int            GpioConsoleSet   (int ArgCount, char **pArguments);
int            GpioConsoleGet   (int ArgCount, char **pArguments);

#endif /* _GPIO_H_ */
