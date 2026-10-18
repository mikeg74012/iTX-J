/*****************************************************************

     File:          stm32.h

     Created on:    22 Oct 2017

     Author:        Michael Gaut

     Description:   Pin and register definitions for stm32 micro
                    on Nero board

******************************************************************/
#ifndef GD32F30x_H_
#define GD32F30x_H_

#define     AHB1_SPEED          8000000
#define     AHB2_SPEED          8000000
#define     APB1_SPEED          8000000
#define     APB2_SPEED          8000000


#define     DBG                 0xe0042000

#define SPI0                0x40013000
#define SPI1                0x40003800
#define SPI2                0x40003c00

#define SPI_CTL0            0x0000
#define SPI_CTL1            0x0004
#define SPI_STAT            0x0008
#define SPI_DATA            0x000c
#define SPI_QCTL            0x0080





#define RTC                 0x40002800

#define RTC_INTEN           0x0000 
#define RTC_CTL             0x0004
#define RTC_PSCH            0x0008
#define RTC_PSCL            0x000c
#define RTC_DIVH            0x0010
#define RTC_DIVL            0x0014
#define RTC_CNTH            0x0018
#define RTC_CNTL            0x001c
#define RTC_ALRMH           0x0020
#define RTC_ALRML           0x0024



#define EXTI                    0x40010400

#define EXTI_INTEN          0x0000
#define EXTI_EVEN           0x0004
#define EXTI_RTEN           0x0008
#define EXTI_FTEN           0x000c
#define EXTI_SWIEV          0x0010
#define EXTI_PD             0x0014



#define PMU     0x40007000

#define PMU_CTL     0x0000
#define PMU_CS      0x0004
         
#define BKP     0x40007000

#define BKP_DATA_0                    0x04U     /*!< BKP data register 0 */
#define BKP_DATA_1                     0x08U     /*!< BKP data register 1 */
#define BKP_DATA_2                     0x0CU     /*!< BKP data register 2 */
#define BKP_DATA_3                     0x10U     /*!< BKP data register 3 */
#define BKP_DATA_4                     0x14U     /*!< BKP data register 4 */
#define BKP_DATA_5                     0x18U     /*!< BKP data register 5 */
#define BKP_DATA_6                     0x1CU     /*!< BKP data register 6 */
#define BKP_DATA_7                     0x20U     /*!< BKP data register 7 */
#define BKP_DATA_8                     0x24U     /*!< BKP data register 8 */
#define BKP_DATA_9                     0x28U     /*!< BKP data register 9 */
#define BKP_DATA_10                    0x40U     /*!< BKP data register 10 */
#define BKP_DATA_11                    0x44U     /*!< BKP data register 11 */
#define BKP_DATA_12                    0x48U     /*!< BKP data register 12 */
#define BKP_DATA_13                    0x4CU     /*!< BKP data register 13 */
#define BKP_DATA_14                    0x50U     /*!< BKP data register 14 */
#define BKP_DATA_15                    0x54U     /*!< BKP data register 15 */
#define BKP_DATA_16                    0x58U     /*!< BKP data register 16 */
#define BKP_DATA_17                    0x5CU     /*!< BKP data register 17 */
#define BKP_DATA_18                    0x60U     /*!< BKP data register 18 */
#define BKP_DATA_19                    0x64U     /*!< BKP data register 19 */
#define BKP_DATA_20                    0x68U     /*!< BKP data register 20 */
#define BKP_DATA_21                    0x6CU     /*!< BKP data register 21 */
#define BKP_DATA_22                    0x70U     /*!< BKP data register 22 */
#define BKP_DATA_23                    0x74U     /*!< BKP data register 23 */
#define BKP_DATA_24                    0x78U     /*!< BKP data register 24 */
#define BKP_DATA_25                    0x7CU     /*!< BKP data register 25 */
#define BKP_DATA_26                    0x80U     /*!< BKP data register 26 */
#define BKP_DATA_27                    0x84U     /*!< BKP data register 27 */
#define BKP_DATA_28                    0x88U     /*!< BKP data register 28 */
#define BKP_DATA_29                    0x8CU     /*!< BKP data register 29 */
#define BKP_DATA_30                    0x90U     /*!< BKP data register 30 */
#define BKP_DATA_31                    0x94U     /*!< BKP data register 31 */
#define BKP_DATA_32                    0x98U     /*!< BKP data register 32 */
#define BKP_DATA_33                    0x9CU     /*!< BKP data register 33 */
#define BKP_DATA_34                    0xA0U     /*!< BKP data register 34 */
#define BKP_DATA_35                    0xA4U     /*!< BKP data register 35 */
#define BKP_DATA_36                    0xA8U     /*!< BKP data register 36 */
#define BKP_DATA_37                    0xACU     /*!< BKP data register 37 */
#define BKP_DATA_38                    0xB0U     /*!< BKP data register 38 */
#define BKP_DATA_39                    0xB4U     /*!< BKP data register 39 */
#define BKP_DATA_40                    0xB8U     /*!< BKP data register 40 */
#define BKP_DATA_41                    0xBCU     /*!< BKP data register 41 */
#define BKP_OCTL                      0x2CU     /*!< RTC signal output control register */
#define BKP_TPCTL                     0x30U     /*!< tamper pin control register */
#define BKP_TPCS                      0x34U     /*!< tamper control and status register */
         


#define AFIO    0x40010000

#define AFIO_PCF0       0x0004
#define AFIO_EXTISS0    0x0008
#define AFIO_EXTISS1    0x000c
#define AFIO_EXTISS2    0x0010
#define AFIO_EXTISS3    0x0014
#define AFIO_PCF1       0x001c
#define AFIO_CPSCTL     0x0020


#define FMC     0x40022000

#define FMC_WS      0x0000
#define FMC_KEY0    0x0004
#define FMC_OBKEY   0x0008
#define FMC_STAT0   0x000c
#define FMC_CTL0    0x0010
#define FMC_ADDR0   0x0014
#define FMC_OBSTAT  0x001c
#define FMC_WP      0x0020
#define FMC_KEY1    0x0044
#define FMC_STAT1   0x004c
#define FMC_CTL1    0x0050
#define FMC_ADDR1   0x0054
#define FMC_WSEN    0x00fc
#define FMC_PID     0x0100






/* SCB base definition */

#define     SCB                 0xe000ed00

/* SCB register offset */

#define     SCB_VTOR            0x0008
#define     SCB_AIRCR           0x000c

/* WWDG base definition */

#define     WWDG                0x40002c00

/* WWDG register offset */

#define     WWDG_CR             0x0000
#define     WWDG_CFR            0x0004
#define     WWDG_SR             0x0008


/* GPIO base definitions */

#define     GPIOA               0x40010800
#define     GPIOB               0x40010c00
#define     GPIOC               0x40011000
#define     GPIOD               0x40011400
#define     GPIOE               0x40011800
#define     GPIOF               0x40011c00
#define     GPIOG               0x40012000

/* GPIO register offsets */

#define     GPIO_CTL0           0x0000
#define     GPIO_CTL1           0x0004
#define     GPIO_ISTAT          0x0008
#define     GPIO_OCTL           0x000c
#define     GPIO_BOP            0x0010
#define     GPIO_BC             0x0014
#define     GPIO_LOCK           0x0018
#define     GPIO_SPD            0x003c


/* Port definitions to be used by software */

#define     PTA                 0
#define     PTB                 1
#define     PTC                 2
#define     PTD                 3
#define     PTE                 4
#define     PTF                 5
#define     PTG                 6
#define     PTH                 7
#define     PTI                 8

#define     defpin(v1,v2)       ((v1<<4)+v2)

#define     FLASH               0x40023c00

/* Flash register definitions */

#define     FLASH_ACR           0x0000
#define     FLASH_KEYR          0x0004
#define     FLASH_OPTKEYR       0x0008
#define     FLASH_SR            0x000c
#define     FLASH_CR            0x0010
#define     FLASH_OPTCR         0x0014




/* Timer base definitions */

#define     TIM0                0x40012c00
#define     TIM1                0x40000000
#define     TIM2                0x40000400
#define     TIM3                0x40000800
#define     TIM4                0x40000c00
#define     TIM5                0x40001000
#define     TIM6                0x40001400
#define     TIM7                0x40013400
#define     TIM8                0x40014c00
#define     TIM9                0x40015000
#define     TIM10               0x40015400
#define     TIM11               0x40001800
#define     TIM12               0x40001c00
#define     TIM13               0x40002000

/* Timer register definitions */

#define     TIM_CTL0            0x0000
#define     TIM_CTL1            0x0004
#define     TIM_SMCFG           0x0008
#define     TIM_DMAINTEN        0x000c
#define     TIM_INTF            0x0010
#define     TIM_SWEVG           0x0014
#define     TIM_CHCTL0          0x0018
#define     TIM_CHCTL1          0x001c
#define     TIM_CHCTL2          0x0020
#define     TIM_CNT             0x0024
#define     TIM_PSC             0x0028
#define     TIM_CAR             0x002c
#define     TIM_CH0CV           0x0034
#define     TIM_CH1CV           0x0038
#define     TIM_CH2CV           0x003c
#define     TIM_CH3CV           0x0040
#define     TIM_DMACFG          0x0048
#define     TIM_DMATB           0x004c
#define     TIM_CFG             0x0050


/* ADC Base definitions */

#define     ADC0                0x40012400
#define     ADC1                0x40012800
#define     ADC2                0x40013c00

/* ADC register definitions */

#define     ADC_STAT            0x0000
#define     ADC_CTL0            0x0004
#define     ADC_CTL1            0x0008
#define     ADC_SAMPT0          0x000c
#define     ADC_SAMPT1          0x0010
#define     ADC_WDHT            0x0024
#define     ADC_WDLT            0x0028
#define     ADC_RSQ0            0x002c
#define     ADC_RSQ1            0x0030
#define     ADC_RSQ2            0x0034
#define     ADC_RDATA           0x004c
#define     ADC_OVSAMPCTL       0x0080


/* USART Base definitions */

#define     USART0              0x40013800
#define     USART1              0x40004400
#define     USART2              0x40004800
#define     USART3              0x40004c00
#define     USART4              0x40005000

/* USART Register definitions */

#define     USART_STAT0         0x0000
#define     USART_DATA          0x0004
#define     USART_BAUD          0x0008
#define     USART_CTL0          0x000c
#define     USART_CTL1          0x0010
#define     USART_CTL2          0x0014
#define     USART_GP            0x0018
#define     USART_CTL3          0x0080

/* RCU Base definitions */

#define     RCU                 0x40021000

/* RCC registers */

#define     RCU_CTL             0x0000
#define     RCU_CFG0            0x0004
#define     RCU_INT             0x0008
#define     RCU_APB2RST         0x000c
#define     RCU_APB1RST         0x0010
#define     RCU_AHBEN           0x0014
#define     RCU_APB2EN          0x0018
#define     RCU_APB1EN          0x001c
#define     RCU_BDCTL           0x0020
#define     RCU_RSTSCK          0x0024
#define     RCU_AHBRST          0x0028
#define     RCU_CFG1            0x002c
#define     RCU_DSV             0x0034
#define     RCU_ADDCTL          0x00c0
#define     RCU_ADDINT          0x00cc
#define     RCU_ADDAPB1RST      0x00e0
#define     RCU_ADDAPB1EN       0x00e4






/* NVIC Base definition */

#define     NVIC                0xe000e100

/* NVIC register definitions */

#define     ISER0               0x0000
#define     ISER1               0x0004
#define     ISER2               0x0008
#define     IPR0                0x0300
#define     IPR1                0x0304
#define     IPR2                0x0308
#define     IPR3                0x030c
#define     IPR4                0x0310
#define     IPR5                0x0314
#define     IPR6                0x0318
#define     IPR7                0x031c
#define     IPR8                0x0320
#define     IPR9                0x0324
#define     IPR10               0x0328
#define     IPR11               0x032c
#define     IPR12               0x0330
#define     IPR13               0x0334
#define     IPR14               0x0338
#define     IPR15               0x033c
#define     IPR16               0x0340
#define     IPR17               0x0344
#define     IPR18               0x0348
#define     IPR19               0x034c


#define     or_register(v1,v2,v3)     ((*(volatile unsigned long *) ((v1)+(v2))) |= (v3))
#define     and_register(v1,v2,v3)   ((*(volatile unsigned long *) ((v1)+(v2))) &= (v3))

#define     write_register(v1,v2,v3)        ((*(volatile unsigned long *) ((v1)+(v2))) = (v3))
#define     read_register(v1,v2)            ((*(volatile unsigned long *) ((v1)+(v2))))


#ifdef __IAR_SYSTEMS_ICC_
#define     IRQ(f)  __interrupt void f (void)
#else
#define     IRQ(f)  void f (void) __attribute__ ((interrupt("IRQ")));void f (void)
#endif


#define     __enable_interrupts()       asm("CPSIE i")
#define     __disable_interrupts()      asm("CPSID i")
#define     cpu_is_irq_enabled()        !(__get_PRIMASK())

#define     enable_irq(irq)     write_register (NVIC, ISER0 + (((irq) >> 5) << 2), 1 << ((irq) & 0x1f))
#define     disable_irq(irq)    write_register (NVIC, ICER0 + (((irq) >> 5) << 2), 1 << ((irq) & 0x1f))
#define     set_irq_priority(irq,p)    (*(volatile unsigned char *)(NVIC+IPR0+(irq)) = (p))

#define abs(x) ((((x))<0)?-(x):(x))

void     set_gpio_low       (unsigned char val);
void     set_gpio_high      (unsigned char val);
void     set_gpio_output    (unsigned char val);
void     set_gpio_enable_pulldown(unsigned char val);
void     set_gpio_open_drain(unsigned char val);
void     set_gpio_analog    (unsigned char val);
void     set_gpio_input     (unsigned char val);
void     set_gpio_alternate (unsigned char val);
unsigned long read_gpio     (unsigned char val);
unsigned long read_gpio_output     (unsigned char val);
void     set_gpio_multiple  (unsigned char val, unsigned short high_bits, unsigned short low_bits);
void     set_gpio_high_speed (unsigned char val);


#endif /* STM32_H_ */
