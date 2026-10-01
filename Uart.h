/**
 **************************************************************************************************
 * @file   Uart.h
 *
 * @author Ripcord Designs
 *
 * @brief  This module provides an abstraction layer for the on-chip uart.
 *         It shares a common Ripcord API with UART modules for other chips, so higher software
 *         layers can be easily moved between different MCU's.
 *
 **************************************************************************************************
 * @attention                      (c) Ripcord Designs Limited 2022
 **************************************************************************************************
 */

#ifndef _UART_H_
#define _UART_H_

#include "stream.h"

/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
/*
 * UART Com Ports available on this chip.  The application layer
 * must use one of these port definitions when initialising the port
 */
typedef enum
{
  COM_PORT_0 = 0,
  COM_PORT_1,
  COM_PORT_2,
  COM_PORT_3,
  COM_PORT_4
} comport_e;


typedef enum
{
  COM_DATABITS_7,
  COM_DATABITS_8,
  COM_DATABITS_9
} uartdatabits_e;


typedef enum
{
  COM_PARITY_NONE,
  COM_PARITY_EVEN,
  COM_PARITY_ODD,
} uartparity_e;


typedef enum
{
  COM_STOPBITS_1,
  COM_STOPBITS_1_5,
  COM_STOPBITS_2
} uartstopbits_e;


typedef enum
{
  COM_FLOWCONTROL_NONE,
  COM_FLOWCONTROL_HW,
} uartflowcontrol_e;


/* Configuration structure.  Used by the application layer when initializing the port */
typedef struct
{
  uint32_t           BaudRate;
  uartdatabits_e     DataBits;
  uartstopbits_e     StopBits;
  uartparity_e       Parity;
  uartflowcontrol_e  FlowControl;
} uartconfig_t;



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
stream_t * UARTInitialise    (comport_e UartPort, uartconfig_t * UartHandle);
void       UARTDeInit        (comport_e UartPort);
int        UARTConsoleStats  (int ArgCount, char **pArguments);
int        UARTConsoleClear  (int ArgCount, char **pArguments);
void       UartRxIntCallback (comport_e UartPort);
void       UartTxIntCallback (comport_e UartPort);


#endif /* _UART_H_ */
