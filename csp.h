/**
 **************************************************************************************************
 * @file   csp.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  Top Level Chip Support for GD32
 *         include power up setup and sleep mode functions
 *
 **************************************************************************************************
 * @attention                  (c) Ripcord Designs Limited 2022
 **************************************************************************************************
 */
#ifndef CSP_H_
#define CSP_H_

/* Comment out lines that are not needed */
#include "crcdriver.h"
#include "flash.h"
#include "spi.h"
#include "uart.h"
#include "gpio.h"


/*
 **************************************************************************************************
 *                                       PUBLIC DEFINES
 **************************************************************************************************
 */

/* MCU reset */
#define SystemReset()     NVIC_SystemReset()

#define csGetTick()       xTaskGetTickCount()


void SystemFreeWatchDogEnable                   (void);
void SystemFreeWatchDogReload                   (void);

/*
 **************************************************************************************************
 *                             MAPPING BETWEEN GD32 AND STM32 HAL
 *
 * Note: These should become abstracted within the CSP. Fixed here as a temporary measure.
 *
 **************************************************************************************************
 */

/* Mapping for interrupt naming convention differences */
#define EXTI15_10_IRQn    EXTI10_15_IRQn

/* Macro to set interrupt priority using STM32 HAL calls */
#define HAL_NVIC_SetPriority(x_,y_,z_)                                \
  ({                                                                  \
    uint32_t prioritygroup = NVIC_GetPriorityGrouping();              \
  	NVIC_SetPriority(x_, NVIC_EncodePriority(prioritygroup, y_, z_)); \
  })

/* Mappings to Enable / disable interrupt using STM32 HAL calls */
#define HAL_NVIC_EnableIRQ(x_)  NVIC_EnableIRQ(x_)
#define HAL_NVIC_DisableIRQ(x_)  NVIC_DisableIRQ(x_)

/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */
/* API */

/* MCU sleep */
void SystemSleep (void);


#endif /* CSP_H_ */
