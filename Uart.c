/**
 **************************************************************************************************
 * @file   Uart.c
 *
 * @author Ripcord Designs
 *
 * @brief  This module provides an abstraction layer for the GD32 on-chip uart.
 *         It shares a common Ripcord API with UART modules for other chips, so higher software
 *         layers can be easily moved between different MCU's.
 *
 **************************************************************************************************
 * @attention                  (c) Ripcord Designs Limited 2022
 **************************************************************************************************
 */
#include "global.h"


/*
 **************************************************************************************************
 *                                       PRIVATE DEFINES
 **************************************************************************************************
 */
 
 /* UART BUFFER definitions */
#define cUARTRxBufferSize 256
#define cUARTTxBufferSize 512

/* number of UARTS available on this chip */
#define NO_OF_UARTS   5

/* UART statistics */
typedef struct
{
  int TxCount;
  int RxCount;
  int Overrun;
  int Framing;
  int Parity;
} uarterror_t;


/* UART definition */
typedef struct
{
  uint32_t             Instance;                     /* USART/UART port register */
  uint8_t              IRQn;                         /* USART/UART IRQ number */
  rcu_periph_enum      RcuClock;                     /* RCU clock reference for the uart */
  uint8_t              RxBuf[cUARTRxBufferSize];     /* actual buffer storage, never accessed directly */
  uint8_t              TxBuf[cUARTTxBufferSize];
  uint8_t              RxBufHandle;                  /* buffer handles for external buffer module */
  uint8_t              TxBufHandle;
  uint8_t              TxData;                       /* The data byte currently being sent */
  uint8_t              RxData;                       /* The data byte currently being received */
  bool                 UartBusy;
  uarterror_t          Error;
  SemaphoreHandle_t    RxSem;
  SemaphoreHandle_t    SendMutex;                    /* makes sure Send and SentBuffer complete atomically */
  stream_t             Stream;
} uart_t;


/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static bool     UartDataAvailable   (uint8_t uartIndex);
static uint8_t  UartGet             (uint8_t uartIndex);
static bool     UartSendString      (uint8_t uartIndex, uint8_t *pData);
static uint32_t UartSendBuffer      (uint8_t uartIndex, uint8_t *pData, uint32_t Len);
static bool     UartPut             (uint8_t uartIndex, uint8_t Data);
static void     UartTxData          (uint8_t uartIndex);
static void     UartErrorProcess    (uint8_t uartIndex);


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
/* UARTs  */
static uart_t UART[NO_OF_UARTS];


/*
 **************************************************************************************************
 *                                 UART SPECIFIC STREAM ROUTINES
 * These functions are the interface between the stream function calls and the individual UARTs
 **************************************************************************************************
 */
static bool     UartDataAvailable0  (void)                           {  return(UartDataAvailable(0));        }
static bool     UartDataAvailable1  (void)                           {  return(UartDataAvailable(1));        }
static bool     UartDataAvailable2  (void)                           {  return(UartDataAvailable(2));        }
static bool     UartDataAvailable3  (void)                           {  return(UartDataAvailable(3));        }
static bool     UartDataAvailable4  (void)                           {  return(UartDataAvailable(4));        }
static uint8_t  UartGet0            (void)                           {  return(UartGet(0));                  }
static uint8_t  UartGet1            (void)                           {  return(UartGet(1));                  }
static uint8_t  UartGet2            (void)                           {  return(UartGet(2));                  }
static uint8_t  UartGet3            (void)                           {  return(UartGet(3));                  }
static uint8_t  UartGet4            (void)                           {  return(UartGet(4));                  }
static bool     UartSendString0     (uint8_t *pData)                 {  return(UartSendString(0,pData));           }
static bool     UartSendString1     (uint8_t *pData)                 {  return(UartSendString(1,pData));           }
static bool     UartSendString2     (uint8_t *pData)                 {  return(UartSendString(2,pData));           }
static bool     UartSendString3     (uint8_t *pData)                 {  return(UartSendString(3,pData));           }
static bool     UartSendString4     (uint8_t *pData)                 {  return(UartSendString(4,pData));           }
static uint32_t UartSendBuffer0     (uint8_t *pData, uint32_t Len)   {  return(UartSendBuffer(0, pData, Len));  }
static uint32_t UartSendBuffer1     (uint8_t *pData, uint32_t Len)   {  return(UartSendBuffer(1, pData, Len));  }
static uint32_t UartSendBuffer2     (uint8_t *pData, uint32_t Len)   {  return(UartSendBuffer(2, pData, Len));  }
static uint32_t UartSendBuffer3     (uint8_t *pData, uint32_t Len)   {  return(UartSendBuffer(3, pData, Len));  }
static uint32_t UartSendBuffer4     (uint8_t *pData, uint32_t Len)   {  return(UartSendBuffer(4, pData, Len));  }
static bool     UartPut0            (uint8_t Data)                   {  return(UartPut(0, Data));            }
static bool     UartPut1            (uint8_t Data)                   {  return(UartPut(1, Data));            }
static bool     UartPut2            (uint8_t Data)                   {  return(UartPut(2, Data));            }
static bool     UartPut3            (uint8_t Data)                   {  return(UartPut(3, Data));            }
static bool     UartPut4            (uint8_t Data)                   {  return(UartPut(4, Data));            }



/*
 **************************************************************************************************
 *                                   PUBLIC SECTION / INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       Initialise the UART and return pointer to the stream interface that should
 *              be used to send and receive data
 * @param[in]   UartHandle - handle for the UART to initialise
 * @return      stream_t : pointer to the stream interface for this UART
 */
stream_t * UARTInitialise (comport_e ComPort, uartconfig_t * UartConfig)
{
  /* find the correct UART structure */
  int UartIndex = ComPort;

  /* Initialise the circular buffer module and create Tx and Rx buffers for this uart */
  ringbufferInit();
  UART[UartIndex].RxBufHandle = ringbufferCreate(&UART[UartIndex].RxBuf, cUARTRxBufferSize, 1);
  UART[UartIndex].TxBufHandle = ringbufferCreate(&UART[UartIndex].TxBuf, cUARTTxBufferSize, 1);

  /* initialise the receive data semaphore for this port */
  UART[UartIndex].RxSem = xSemaphoreCreateCounting(cUARTRxBufferSize, 0);

  /* initialise the Send mutex for this port */
  UART[UartIndex].SendMutex = xSemaphoreCreateBinary();
  xSemaphoreGive(UART[UartIndex].SendMutex);
  UART[UartIndex].UartBusy = false;

  /* set up the uart specific parameters */
  switch (UartIndex)
  {
  case 0:
    UART[UartIndex].Instance              = USART0;
    UART[UartIndex].IRQn                  = USART0_IRQn;
    UART[UartIndex].RcuClock              = RCU_USART0;
    UART[UartIndex].Stream.Get            = UartGet0;
    UART[UartIndex].Stream.DataAvailable  = UartDataAvailable0;
    UART[UartIndex].Stream.Put            = UartPut0;
    UART[UartIndex].Stream.Send           = UartSendString0;
    UART[UartIndex].Stream.SendBuf        = UartSendBuffer0;

    /* Set up GPIO pins for this port */
    rcu_periph_clock_enable(USART0_PORT_CLK);
    gpio_init(USART0_TX_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, USART0_TX_PIN);
    gpio_init(USART0_RX_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, USART0_RX_PIN);
    break;
  case 1:
    UART[UartIndex].Instance              = USART1;
    UART[UartIndex].IRQn                  = USART1_IRQn;
    UART[UartIndex].RcuClock              = RCU_USART1;
    UART[UartIndex].Stream.Get            = UartGet1;
    UART[UartIndex].Stream.DataAvailable  = UartDataAvailable1;
    UART[UartIndex].Stream.Put            = UartPut1;
    UART[UartIndex].Stream.Send           = UartSendString1;
    UART[UartIndex].Stream.SendBuf        = UartSendBuffer1;

    /* Set up GPIO pins for this port */
    rcu_periph_clock_enable(USART1_PORT_CLK);
    gpio_init(USART1_TX_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, USART1_TX_PIN);
    gpio_init(USART1_RX_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, USART1_RX_PIN);
    break;
  case 2:
    UART[UartIndex].Instance              = USART2;
    UART[UartIndex].IRQn                  = USART2_IRQn;
    UART[UartIndex].RcuClock              = RCU_USART2;
    UART[UartIndex].Stream.Get            = UartGet2;
    UART[UartIndex].Stream.DataAvailable  = UartDataAvailable2;
    UART[UartIndex].Stream.Put            = UartPut2;
    UART[UartIndex].Stream.Send           = UartSendString2;
    UART[UartIndex].Stream.SendBuf        = UartSendBuffer2;
    break;
  case 3:
    UART[UartIndex].Instance              = UART3;
    UART[UartIndex].IRQn                  = UART3_IRQn;
    UART[UartIndex].RcuClock              = RCU_UART3;
    UART[UartIndex].Stream.Get            = UartGet3;
    UART[UartIndex].Stream.DataAvailable  = UartDataAvailable3;
    UART[UartIndex].Stream.Put            = UartPut3;
    UART[UartIndex].Stream.Send           = UartSendString3;
    UART[UartIndex].Stream.SendBuf        = UartSendBuffer3;

    /* Set up GPIO pins for this port */
    rcu_periph_clock_enable(UART3_PORT_CLK);
    gpio_init(UART3_TX_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, UART3_TX_PIN);
    gpio_init(UART3_RX_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, UART3_RX_PIN);
    break;
  case 4:
    UART[UartIndex].Instance              = UART4;
    UART[UartIndex].IRQn                  = UART4_IRQn;
    UART[UartIndex].RcuClock              = RCU_UART4;
    UART[UartIndex].Stream.Get            = UartGet4;
    UART[UartIndex].Stream.DataAvailable  = UartDataAvailable4;
    UART[UartIndex].Stream.Put            = UartPut4;
    UART[UartIndex].Stream.Send           = UartSendString4;
    UART[UartIndex].Stream.SendBuf        = UartSendBuffer4;

    /* Set up GPIO pins for this port */
    rcu_periph_clock_enable(UART4_PORT_CLK);
//    gpio_init(UART4_TX_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, UART4_TX_PIN);
    gpio_init(UART4_RX_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, UART4_RX_PIN);
    break;
  }

  /* Initialise the UART hardware */
  rcu_periph_clock_enable(RCU_AF);
  rcu_periph_clock_enable(UART[UartIndex].RcuClock);
  usart_baudrate_set(UART[UartIndex].Instance, UartConfig->BaudRate);
  usart_receive_config(UART[UartIndex].Instance, USART_RECEIVE_ENABLE);
  if (UartIndex != COM_PORT_4)
  {
    usart_transmit_config(UART[UartIndex].Instance, USART_TRANSMIT_ENABLE);
  }
  usart_enable(UART[UartIndex].Instance);

  /* USART interrupt configuration */
  nvic_irq_enable(UART[UartIndex].IRQn, 5, 0);

  /* enable UART Receive interrupt */
  usart_interrupt_enable(UART[UartIndex].Instance, USART_INT_RBNE);

  return (&UART[UartIndex].Stream);
}



/**
 * @brief       De-initialise the UART
 * @param[in]   ComPort - handle for the uart to deinitialse
 * @return      none
 */
void UARTDeInit (comport_e ComPort)
{
  /* stop interrupts */
  usart_interrupt_disable(UART[ComPort].Instance, USART_INT_RBNE);
  usart_interrupt_disable(UART[ComPort].Instance, USART_INT_TBE);

  /* deinit the UART */
  usart_deinit(UART[ComPort].Instance);

  /* free the ring buffers */
  ringbufferDestroy(UART[ComPort].RxBufHandle);
  ringbufferDestroy(UART[ComPort].TxBufHandle);

  /* delete the semaphores */
  vSemaphoreDelete(UART[ComPort].RxSem);
  vSemaphoreDelete(UART[ComPort].SendMutex);

  UART[ComPort].IRQn = 0;
}


/*
 **************************************************************************************************
 *                                       GENERIC INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       This function determines whether data is available
 * @param[in]   none
 * @return      bool : true  = data available
 *                     false = no data available
 */
static bool UartDataAvailable (uint8_t uartIndex)
{
  return (ringbufferDataAvailable (UART[uartIndex].RxBufHandle));
}


/**
 * @brief       Gets the next byte from the UART receive buffer
 *
 * @warning     blocks if no data available, check first!
 *
 * @param[in]   none
 * @return      uint8_t : The next received data
 */
static uint8_t UartGet (uint8_t uartIndex)
{
  uint8_t Result;

  /* yield until serial data is available */
  xSemaphoreTake(UART[uartIndex].RxSem, portMAX_DELAY);

  /* return the data */
  ringbufferGet(UART[uartIndex].RxBufHandle, &Result);
  return (Result);
}


/**
 * @brief       Add a null terminated string to the tx buffer
 *
 * @warning     Data may be lost if the buffer is full
 *
 * @param[in]   pData : the string to send
 * @return      bool  : true  = Success
 *                      false = Failure - the tx queue is full
 */
static bool UartSendString (uint8_t uartIndex, uint8_t *pData)
{
  bool ret = true;

  /* add all the data to the transmit buffer. */
  if (pdTRUE == xSemaphoreTake(UART[uartIndex].SendMutex, portMAX_DELAY))
  {
    while (*pData != 0)
    {
      if (false == ringbufferPut(UART[uartIndex].TxBufHandle, pData))
      {
        ret = false;
      }
      pData++;
    }

    /* if the uart is idle, we kick it off by enabling interrupts */
    if (false == UART[uartIndex].UartBusy)
    {
      usart_interrupt_enable(UART[uartIndex].Instance, USART_INT_TBE);
    }
    xSemaphoreGive(UART[uartIndex].SendMutex);
  }

  return (ret);
}


/**
 * @brief       Add a buffer of specified length to the tx buffer
 *
 * @warning     Data may be lost if the buffer is full
 *
 * @param[in]   pData    : the buffer to send
 * @paran[in]   Len      : the number of bytes to send
 * @return      uint32_t : the number of bytes actually sent (will be less then Len if buffer full)
 */
static uint32_t UartSendBuffer (uint8_t uartIndex, uint8_t *pData, uint32_t Len)
{
  uint32_t BytesRemaining = Len;

  /* add all the data to the transmit buffer. */
  if (pdTRUE == xSemaphoreTake(UART[uartIndex].SendMutex, portMAX_DELAY))
  {
    while (BytesRemaining > 0)
    {
      if (false == ringbufferPut(UART[uartIndex].TxBufHandle, pData))
      {
    	  break;
      }
      else
      {
        pData++;
        BytesRemaining--;
      }
    }
    /* if the uart is idle, we kick it off by enabling interrupts */
    if (false == UART[uartIndex].UartBusy)
    {
      usart_interrupt_enable(UART[uartIndex].Instance, USART_INT_TBE);
    }
    xSemaphoreGive(UART[uartIndex].SendMutex);
  }

  return (Len - BytesRemaining);
}


/**
 * @brief       Add a single character to tx buffer.
 *
 * @warning     Blocks if the buffer is full
 *
 * @param[in]   uartIndex : Index of the Uart to send on
 * @paran[in]   Data      : the char to be sent
 * @return      bool      : true  = Success
 *                          false = Failure
 */
static bool UartPut (uint8_t uartIndex, uint8_t Data)
{
  /* if the buffer is full an error has occurred */
  if (false == ringbufferPut(UART[uartIndex].TxBufHandle, &Data)) return false;

  /* if the uart is idle, we kick it off by enabling interrupts */
  if (false == UART[uartIndex].UartBusy)
  {
    usart_interrupt_enable(UART[uartIndex].Instance, USART_INT_TBE);
  }

  return (true);
}


/*
 **************************************************************************************************
 *                                         PRIVATE SECTION
 **************************************************************************************************
 */
/**
 * @brief   If there is data in the tx buffer, send the next byte
 *
 * @param   uartIndex : Index of the Uart to send on
 * @return  none
 */
static void UartTxData (uint8_t uartIndex)
{
  /* Is data ready to transmit? */
  if (ringbufferGet(UART[uartIndex].TxBufHandle, &UART[uartIndex].TxData))
  {
    /* note that the uart is now busy */
    UART[uartIndex].UartBusy = true;

    /* transmit the next byte */
    usart_data_transmit(UART[uartIndex].Instance, UART[uartIndex].TxData);      /* send the next data byte */

    /* update the stats */
    UART[uartIndex].Error.TxCount++;
  }
  else
  {
    /* Disable interrupts if there is nothing else to send */
    usart_interrupt_disable(UART[uartIndex].Instance, USART_INT_TBE);
  }
}


/**
 * @brief    This is the receiver interrupt process function.
 *           It receives one character from the UART and adds it to the receive buffer.
 *
 * @param    huart : UART handle
 * @return   none
 */
void UartRxIntCallback (comport_e UartPort)
{
  /* get the received data */
  UART[UartPort].RxData = (uint8_t)usart_data_receive(UART[UartPort].Instance);

  /* stuff received data byte into the circular buffer */
  ringbufferPut(UART[UartPort].RxBufHandle, &UART[UartPort].RxData);

  /* update the stats */
  UART[UartPort].Error.RxCount++;

  /* check error status */
  UartErrorProcess(UartPort);

  /* signal the other task */
  xSemaphoreGiveFromISR(UART[UartPort].RxSem, NULL);
}


/**
 * @brief    This is the transmit interrupt callback, which is called by the UART driver.
 *           It is called in response to a Uart Transmitter Ready (TXRDY) interrupt whenever the
 *           uart txbuffer is empty.  If there is more data waiting to go, we call SerialTxData to
 *           transmit the next byte.
 *
 * @param    huart : UART index
 * @return   none
 */
void UartTxIntCallback (comport_e UartPort)
{
  /* note that the UART is now idle */
  UART[UartPort].UartBusy = false;

  /* Send any data waiting to go */
  UartTxData(UartPort);
}


/**
 * @brief    This is the UART Error process, which is checked when receive IRQ is called.
 *           whenever a Uart error occurs.
 *           Some errors automatically disable UART operation / interrupts, so transmission or
 *           reception should be restarted here after error handling.
 * @param    huart : UART handle
 * @return   none
 */
static void UartErrorProcess (uint8_t uartIndex)
{
  uint8_t erro_flag = 0;
  if (SET == usart_flag_get(UART[uartIndex].Instance, USART_FLAG_ORERR))
  {
    usart_flag_clear(UART[uartIndex].Instance, USART_FLAG_ORERR);
    UART[uartIndex].Error.Overrun++;
    erro_flag++;
  }
  if (SET == usart_flag_get(UART[uartIndex].Instance, USART_FLAG_FERR))
  {
    usart_flag_clear(UART[uartIndex].Instance, USART_FLAG_FERR);
    UART[uartIndex].Error.Framing++;
    erro_flag++;
  }
  if (SET == usart_flag_get(UART[uartIndex].Instance, USART_FLAG_PERR))
  {
   usart_flag_clear(UART[uartIndex].Instance, USART_FLAG_PERR);
    UART[uartIndex].Error.Parity++;
    erro_flag++;
  }
  if (SET == usart_flag_get(UART[uartIndex].Instance, USART_FLAG_NERR))
  {
    usart_flag_clear(UART[uartIndex].Instance, USART_FLAG_NERR);
    UART[uartIndex].Error.Parity++;
    erro_flag++;
  }
  /* Re-start reception */
  if (erro_flag > 0)
  {
    usart_interrupt_enable(UART[uartIndex].Instance, USART_INT_RBNE);
  }
}


/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       Display UART statistics
 *
 * @param[in]   argCount    : THe number of arguments passed from the CLI
 * @param[in]   pArguments  : The list of arguments passed from the CLI
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int UARTConsoleStats (int ArgCount, char **pArguments)
{
  /* print title row */
  ConsolePrintf("Uart  Tx-Count  Rx-Count  Overrun   Framing   Parity\n\r");

  /* print the status data */
  for (int UartIndex = 0; UartIndex < NO_OF_UARTS; UartIndex++)
  {
    ConsolePrintf("%d     %6d    %6d   %6d    %6d   %6d\n\r",
                  UartIndex,
                  UART[UartIndex].Error.TxCount,
                  UART[UartIndex].Error.RxCount,
                  UART[UartIndex].Error.Overrun,
                  UART[UartIndex].Error.Framing,
                  UART[UartIndex].Error.Parity);
  }

  return(CONSOLE_ERROR_OK);
}


/**
 * @brief       Reset all UART statistics back to zero
 *
 * @param[in]   argCount    : THe number of arguments passed from the CLI
 * @param[in]   pArguments  : The list of arguments passed from the CLI
 * @return      status code : CLI_ERROR_OK on success, error code on failure
 */
int UARTConsoleClear (int ArgCount, char **pArguments)
{
  /* Reset the statistics */
  for (int UartIndex = 0; UartIndex < NO_OF_UARTS; UartIndex++)
  {
    UART[UartIndex].Error.TxCount = 0;
    UART[UartIndex].Error.RxCount = 0;
    UART[UartIndex].Error.Overrun = 0;
    UART[UartIndex].Error.Framing = 0;
    UART[UartIndex].Error.Parity = 0;
  }

  /* indicate success */
  return (CONSOLE_ERROR_OK);
}

