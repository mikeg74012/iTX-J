/**
 ******************************************************************************
 * @file  gd32f30x_it.c
 ******************************************************************************
 *
 * @author Ripcord Designs
 * @date   2022-04-28
 *
 * @brief  interrupt service routines
 *
 *         Hardware: ITx V5 PCB + GD32F303Ex chip
 *
 ******************************************************************************
 */

#include "global.h"


/*!
    \brief      this function handles NMI exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void NMI_Handler(void)
{
  SetBackupResetRegister (RESET_SYSFAULT_NMI_FAULT);
  SystemReset();
}

/*!
    \brief      this function handles HardFault exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void HardFault_Handler(void)
{
  SetBackupResetRegister (RESET_SYSFAULT_HARD_FAULT);
  SystemReset();
}

/*!
    \brief      this function handles MemManage exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void MemManage_Handler(void)
{
  /* if Memory Manage exception occurs, go to infinite loop */
  SetBackupResetRegister (RESET_SYSFAULT_MEMMANAGE_FAULT);
  SystemReset();
}


/*!
    \brief      this function handles BusFault exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void BusFault_Handler(void)
{
  /* if Bus Fault exception occurs, go to infinite loop */
  SetBackupResetRegister (RESET_SYSFAULT_BUS_FAULT);
  SystemReset();
}


/*!
    \brief      this function handles UsageFault exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void UsageFault_Handler(void)
{
  /* if Usage Fault exception occurs, go to infinite loop */
  SetBackupResetRegister (RESET_SYSFAULT_USAGE_FAULT);
  SystemReset();
}


/*!
    \brief      this function handles DebugMon exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void DebugMon_Handler(void)
{
  SetBackupResetRegister (RESET_SYSFAULT_DEBUGMON_FAULT);
  SystemReset();
}



/******************************************************************************/
/* GD32 Peripheral Interrupt Handlers                                         */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f3xx.s).                    */
/******************************************************************************/

/**
 * @brief      This function handles EXTI line[9:5] interrupts.
 *             Interrupt used by the IMU
 * @param[in]  none
 * @return     none
 */
void EXTI5_9_IRQHandler (void)
{
  if (read_register (EXTI, EXTI_PD) & (1 << 7))
  {
    write_register (EXTI, EXTI_PD, (1 << 7));

    /*ACCEL_INT2_EXTI_LINE line call back*/
    AccelDriverCallback();
  }
}


/**
 * @brief      This function handles EXTI line[15:10] interrupts.
 *             Interrupt shared by Red Button and USB Power present
 * @param[in]  none
 * @return     none
 */
void EXTI10_15_IRQHandler (void)
{
  /* figure out which interrupt fired */
  if (read_register (EXTI, EXTI_PD) & (1 << 11))
  {
    write_register (EXTI, EXTI_PD, (1 << 11));
    ButtonMonitorPressCallback();
  }
  
  if (read_register (EXTI, EXTI_PD) & (1 << 13))
  {
    write_register (EXTI, EXTI_PD, (1 << 13));
    BatteryMonitorUsbPowerISR();
  }
}


/**
* @brief        Handles USART0 interrupts
* @param[in]    none
* @return       {none}
*/
void USART0_IRQHandler (void)
{
  if (read_register (USART0, USART_STAT0) & (1 << 7))
  {
    /* write one byte to the transmit data register */
    UartTxIntCallback(COM_PORT_0);
  }
  if (read_register (USART0, USART_STAT0) & (1 << 5))
  {
    /* read one byte from the receive data register */
    UartRxIntCallback(COM_PORT_0);
  }
}


/**
* @brief        Handles USART1 interrupts
* @param[in]    none
* @return       {none}
*/
void USART1_IRQHandler (void)
{
  if (read_register (USART1, USART_STAT0) & (1 << 7))
  {
    /* write one byte to the transmit data register */
    UartTxIntCallback(COM_PORT_1);
  }
  if (read_register (USART1, USART_STAT0) & (1 << 5))
  {
    /* read one byte from the receive data register */
    UartRxIntCallback(COM_PORT_1);
  }
}


/**
* @brief        Handles UART3 interrupts
* @param[in]    none
* @return       {none}
*/
void UART3_IRQHandler (void)
{
  if (read_register (USART3, USART_STAT0) & (1 << 5))
  {
    /* read one byte from the receive data register */
    UartRxIntCallback(COM_PORT_3);
  }
  if (read_register (USART3, USART_STAT0) & (1 << 7))
  {
    /* write one byte to the transmit data register */
    UartTxIntCallback(COM_PORT_3);
  }
}


/**
* @brief        Handles UART4 interrupts
* @param[in]    none
* @return       {none}
*/
void UART4_IRQHandler (void)
{
  if (read_register (USART4, USART_STAT0) & (1 << 5))
  {
    /* read one byte from the receive data register */
    UartRxIntCallback(COM_PORT_4);
  }
}


/**
* @brief        this function handles RTC global interrupt request
* @details      This one handle on chip RTC through EXTI Line
*
* @warning      none
*
* @param[in]    none
* @return       {none}
*/
void RTC_IRQHandler(void)
{
  RtcEXTICallback ();
}


/**
* @brief        this function handles RTC Alarm interrupt request
* @details      This one handle the RTC flags
*
* @warning      none
*
* @param[in]    none
* @return       {none}
*/
void RTC_Alarm_IRQHandler(void)
{
  RtcFlagCallback();
}

