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
#if 0
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
#endif


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
static void     UartErrorProcess    (unsigned long base);


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
/* UARTs  */

/*
 **************************************************************************************************
 *                                 UART SPECIFIC STREAM ROUTINES
 * These functions are the interface between the stream function calls and the individual UARTs
 **************************************************************************************************
 */
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
stream_t * UARTInitialise (unsigned long base, uartconfig_t * UartConfig)
{
  /* Initialise the circular buffer module and create Tx and Rx buffers for this uart */
//  UART[UartIndex].RxBufHandle = ringbufferCreate(&UART[UartIndex].RxBuf, cUARTRxBufferSize, 1);
//  UART[UartIndex].TxBufHandle = ringbufferCreate(&UART[UartIndex].TxBuf, cUARTTxBufferSize, 1);

  /* initialise the receive data semaphore for this port */
//  UART[UartIndex].RxSem = xSemaphoreCreateCounting(cUARTRxBufferSize, 0);

  /* initialise the Send mutex for this port */
//  UART[UartIndex].SendMutex = xSemaphoreCreateBinary();
//  xSemaphoreGive(UART[UartIndex].SendMutex);
//  UART[UartIndex].UartBusy = false;

  /* set up the uart specific parameters */
  switch (base)
  {
  case USART0:
/*    UART[UartIndex].Instance              = USART0;
    UART[UartIndex].IRQn                  = USART0_IRQn;
    UART[UartIndex].RcuClock              = RCU_USART0;
    UART[UartIndex].Stream.Get            = UartGet0;
    UART[UartIndex].Stream.DataAvailable  = UartDataAvailable0;
    UART[UartIndex].Stream.Put            = UartPut0;
    UART[UartIndex].Stream.Send           = UartSendString0;
    UART[UartIndex].Stream.SendBuf        = UartSendBuffer0;
*/
    /* Set up GPIO pins for this port */
      
    or_register (RCU, RCU_APB2EN, (1 << 14));
    
    set_gpio_input (RXD_FROM_MODEM);
    set_gpio_alternate (TXD_TO_MODEM);
    
    break;
  case USART1:
/*
    UART[UartIndex].Instance              = USART1;
    UART[UartIndex].IRQn                  = USART1_IRQn;
    UART[UartIndex].RcuClock              = RCU_USART1;
    UART[UartIndex].Stream.Get            = UartGet1;
    UART[UartIndex].Stream.DataAvailable  = UartDataAvailable1;
    UART[UartIndex].Stream.Put            = UartPut1;
    UART[UartIndex].Stream.Send           = UartSendString1;
    UART[UartIndex].Stream.SendBuf        = UartSendBuffer1;
*/
      
    /* Set up GPIO pins for this port */
      
    or_register (RCU, RCU_APB1EN, (1 << 17));
    
    set_gpio_input (RX_FROM_WIFI);
    set_gpio_alternate (TX_TO_WIFI);

    break;


  case USART3:
/*
    UART[UartIndex].Instance              = UART3;
    UART[UartIndex].IRQn                  = UART3_IRQn;
    UART[UartIndex].RcuClock              = RCU_UART3;
    UART[UartIndex].Stream.Get            = UartGet3;
    UART[UartIndex].Stream.DataAvailable  = UartDataAvailable3;
    UART[UartIndex].Stream.Put            = UartPut3;
    UART[UartIndex].Stream.Send           = UartSendString3;
    UART[UartIndex].Stream.SendBuf        = UartSendBuffer3;
*/
      
    /* Set up GPIO pins for this port */

    or_register (RCU, RCU_APB1EN, (1 << 19));
    
    set_gpio_input (DEBUG_RX);
    set_gpio_alternate (DEBUG_TX);

    break;
    
  case USART4:
/*
    UART[UartIndex].Instance              = UART4;
    UART[UartIndex].IRQn                  = UART4_IRQn;
    UART[UartIndex].RcuClock              = RCU_UART4;
    UART[UartIndex].Stream.Get            = UartGet4;
    UART[UartIndex].Stream.DataAvailable  = UartDataAvailable4;
    UART[UartIndex].Stream.Put            = UartPut4;
    UART[UartIndex].Stream.Send           = UartSendString4;
    UART[UartIndex].Stream.SendBuf        = UartSendBuffer4;
*/
      
    /* Set up GPIO pins for this port */
      
    or_register (RCU, RCU_APB1EN, (1 << 20));
    
    set_gpio_input (GNSS_TX);
    
    break;
  }

  /* Initialise the UART hardware */
  
  /* AF enable */
  
  or_register (RCU, RCU_APB2EN, (1 << 0));
  
  /* TODO the baud rate will need converting */
  
  write_register (base, USART_BAUD, UartConfig->BaudRate);
  
  /* Enable receive */
  
  or_register (base, USART_CTL0, (1 << 2));
  
  if (base != USART4)
  {
      /* Enable transmit */
      
    or_register (base, USART_CTL0, (1 << 3));
  }

  /* USART interrupt configuration */
  nvic_irq_enable(UART[UartIndex].IRQn, 5, 0);

  /* enable UART Receive interrupt */

    or_register (base, USART_CTL0, (1 << 5));

  return (&UART[UartIndex].Stream);
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
static bool UartSendString (unsigned long base, uint8_t *pData)
{
  bool ret = true;

  /* add all the data to the transmit buffer. */

    while (*pData != 0)
    {
      if (false == ringbufferPut(UART[uartIndex].TxBufHandle, pData))
      {
        ret = false;
      }
      pData++;
    }

    /* if the uart is idle, we kick it off by enabling interrupts */

    or_register (base, USART_CTL0, (1 << 7));
    
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
static uint32_t UartSendBuffer (unsigned long base, uint8_t *pData, uint32_t Len)
{
  uint32_t BytesRemaining = Len;

  /* add all the data to the transmit buffer. */
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
    or_register (base, USART_CTL0, (1 << 7));
    
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
void UartRxIntCallback (unsigned long base)
{
  /* check error status */
    
    if (read_register (base, USART_STAT0) & 0xf)
    {
        /* An error has occurred */

        /* Flush any received data which then clears the error flags */
        
        read_register (base, USART_DATA);
        
    }
    else
    {
        /* stuff received data byte into the circular buffer */
        ringbufferPut(UART[UartPort].RxBufHandle, read_register (base, USART_DATA));
    }
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
void UartTxIntCallback (unsigned long base)
{
  /* Send any data waiting to go */
  UartTxData(base);
}


/**
 * @brief    This is the UART Error process, which is checked when receive IRQ is called.
 *           whenever a Uart error occurs.
 *           Some errors automatically disable UART operation / interrupts, so transmission or
 *           reception should be restarted here after error handling.
 * @param    huart : UART handle
 * @return   none
 */
static void UartErrorProcess (unsigned long base)
{
    if (read_register (base, USART_STAT0) & 0xf)
    {
        /* An error has occurred */

        /* Flush any received data which then clears the error flags */
        
        read_register (base, USART_DATA);
        
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
  /* indicate success */
  return (CONSOLE_ERROR_OK);
}

