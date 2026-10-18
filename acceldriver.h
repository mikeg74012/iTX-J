/**
 ******************************************************************************
 * @file   acceldriver.h
 ******************************************************************************
 *
 * @author Ripcord Designs
 * @date   20/11/2019
 *
 * @brief  Accelerometer Driver
 *
 ******************************************************************************
 */

#ifndef ACCELDRIVER_H
#define ACCELDRIVER_H

#include "main.h"
#include "spi.h"
#include <stdbool.h>


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
#define SPI_BUFFER_SIZE       256
#define SPI_MAX_DATA_LENGTH   (SPI_BUFFER_SIZE - 1)


/*
 **************************************************************************************************
 *                                         PUBLIC VARIABLES
 **************************************************************************************************
 */
extern bool AccelWakeUp;


/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */
/* API */
bool    AccelDriverInit                   (void);
int16_t AccelDriverGetAccelerationX       (void);
int16_t AccelDriverGetAccelerationY       (void);
int16_t AccelDriverGetAccelerationZ       (void);
uint8_t AccelDriverGetFifoDataPair        (int16_t* pData);
bool    AccelDriverGetFifoStatus          (uint16_t* pSamplesAvailable);
void    AccelDriverResetFifo              (void);
bool    AccelDriverCheckWhoAmI            (void);
void    AccelDriverEnableWakeUpInterrupt  (void);
void    AccelDriverDisableWakeUpInterrupt (void);
void    AccelDriverCallback               (void);
void    AccelDriverSleepConfig            (void);
void    AccelDriverActiveConfig           (void);
void    AccelDriverWakeupThresholdConfig  (void);

/* Console Commands */
int     AccelDriverConsolePeek            (int argCount, char **pArguments);
int     AccelDriverConsolePoke            (int argCount, char **pArguments);
int     AccelDriverConsoleRead            (int argCount, char **pArguments);

#endif // ACCELDRIVER_H
