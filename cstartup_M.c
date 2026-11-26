/**************************************************
 *
 * This file contains an interrupt vector for Cortex-M written in C.
 * The actual interrupt functions must be provided by the application developer.
 *
 * Copyright 2007-2017 IAR Systems AB.
 *
 * $Revision: 112610 $
 *
 **************************************************/

#pragma language=extended
#pragma segment="CSTACK"

extern void __iar_program_start( void );

extern void TIM1_Handler( void );
extern void TIM3_Handler( void );
extern void TIM4_Handler( void );
extern void USART0_IRQHandler ( void );
extern void USART1_IRQHandler ( void );
extern void UART3_IRQHandler (void);
extern void UART4_IRQHandler (void);
extern void EXTI5_9_IRQHandler (void);
extern void EXTI10_15_IRQHandler (void);
extern void RTC_IRQHandler (void);
extern void RTC_Alarm_IRQHandler (void);
extern void NMI_Handler( void );
extern void HardFault_Handler( void );
extern void MemManage_Handler( void );
extern void BusFault_Handler( void );
extern void UsageFault_Handler( void );
extern void SVC_Handler( void );
extern void DebugMon_Handler( void );
extern void PendSV_Handler( void );
extern void SysTick_Handler( void );

typedef void( *intfunc )( void );
typedef union { intfunc __fun; void * __ptr; } intvec_elem;

// The vector table is normally located at address 0.
// When debugging in RAM, it can be located in RAM, aligned to at least 2^6.
// If you need to define interrupt service routines,
// make a copy of this file and include it in your project.
// The name "__vector_table" has special meaning for C-SPY, which
// is where to find the SP start value.
// If vector table is not located at address 0, the user has to initialize
// the  NVIC vector table register (VTOR) before using interrupts.


#pragma location = ".intvec"
const intvec_elem __vector_table[] =
{
  { .__ptr = __sfe( "CSTACK" ) },
  __iar_program_start,

  NMI_Handler,
  HardFault_Handler,
  MemManage_Handler,
  BusFault_Handler,
  UsageFault_Handler,
  0,
  0,
  0,
  0,
  SVC_Handler,
  DebugMon_Handler,
  0,
  PendSV_Handler,
  SysTick_Handler,
	0,				//Window Watchdog
	0,				//LVD
	0,				//Tamper
	RTC_IRQHandler,				//RTC 
	0,				//flash
	0,				//RCU and CTC
	0,				//EXTI0
	0,				//EXTI1
	0,				//EXTI2
	0,				//EXTI3
	0,				//EXTI4
	0,				//DMA0_Stream0
	0,				//DMA0_Stream1
	0,				//DMA0_Stream2
	0,				//DMA0_Stream3
	0,				//DMA0_Stream4
	0,				//DMA0_Stream5
	0,				//DMA0_Stream6
	0,				//ADC0 and ADC1
	0,				//CAN0_TX
	0,				//CAN0_RX0
	0,				//CAN0_RX1
	0,				//CAN0_SCE
	EXTI5_9_IRQHandler,		//EXTI9_5
	0,				//TIM0_BRK_TIM8
	0,				//TIM0_UP_TIM9
	0,				//TIM0_TRG_COM_TIM10
	0,				//TIM0_CC
	0,	            //TIM1
	0,				//TIM2
	0,				//TIM3
	0,				//I2C0_EV
	0,				//I2C0_ER
	0,				//I2C1_EV
	0,				//T2C1_ER
	0,				//SPI0
	0,				//SPI1
	USART0_IRQHandler,				//USART0
	USART1_IRQHandler,	            //USART1
	0,	            //USART2
	EXTI10_15_IRQHandler,				//EXTI15_10
	RTC_Alarm_IRQHandler,				//RTC_alarm
	0,				//OTG_FS_WAKEUP
	0,				//TIM7_BRK_TIM11
	0,				//TIM7_UP_TIM12
	0,				//TIM7_TRG_COM_TIM13
	0,				//TIM7_CC
	0,				//Reserved
	0,				//EXMC
	0,				//Reserved
	0,				//TIM4
	0,				//SPI2
	UART3_IRQHandler ,				//UART3
	UART4_IRQHandler ,				//UART4
	0,				//TIM5
	0,				//TIM6
	0,				//DMA1_Stream0
	0,				//DMA1_Stream1
	0,				//DMA1_Stream2
	0,				//DMA1_Stream3
	0,				//DMA1_Stream4
	0,				//ETH
	0,				//ETH_WAKEUP
	0,				//CAN1_TX
	0,				//CAN1_RX0
	0,				//CAN1_RX1
	0,				//CAN1_SCE
	0,				//OTG_FS
	
};

#pragma call_graph_root = "interrupt"
__weak void NMI_Handler( void ) { while (1) {} }
#pragma call_graph_root = "interrupt"
__weak void HardFault_Handler( void ) { while (1) {} }
#pragma call_graph_root = "interrupt"
__weak void MemManage_Handler( void ) { while (1) {} }
#pragma call_graph_root = "interrupt"
__weak void BusFault_Handler( void ) { while (1) {} }
#pragma call_graph_root = "interrupt"
__weak void UsageFault_Handler( void ) { while (1) {} }
#pragma call_graph_root = "interrupt"
__weak void SVC_Handler( void ) { while (1) {} }
#pragma call_graph_root = "interrupt"
__weak void DebugMon_Handler( void ) { while (1) {} }
#pragma call_graph_root = "interrupt"
__weak void PendSV_Handler( void ) { while (1) {} }
#pragma call_graph_root = "interrupt"
__weak void SysTick_Handler( void ) { while (1) {} }
#pragma call_graph_root = "interrupt"
__weak void OTG_HS_Handler( void ) { while (1) {} }
#pragma call_graph_root = "interrupt"
__weak void OTG_FS_Handler( void ) { while (1) {} }


void __cmain( void );
__weak void __iar_init_core( void );
__weak void __iar_init_vfp( void );

#pragma required=__vector_table
void __iar_program_start( void )
{
  __iar_init_core();
  __iar_init_vfp();
  __cmain();
}
