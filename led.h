/**
 ******************************************************************************
 * @file   led.h
 ******************************************************************************
 *
 * @author Kirti Yadav
 * @date   22/10/2019
 *
 * @brief  Top level Bi-colour LED driver.
 *
 ******************************************************************************
 */
 
#ifndef LED_H
#define LED_H

/*
 ******************************************************************************
 *                               PUBLIC DEFINES                               *
 ******************************************************************************
 */

/* LED colours */
typedef enum
{
  RED = 0,
  GREEN,
  AMBER,
  UNKNOWN
} LedColor_e;


/*
 ******************************************************************************
 *                  PUBLIC FUNCTION PROTOTYPES / INTERFACE                    *
 ******************************************************************************
 */
/* API */
void LedInit              (void);
void LedUpdateStatus      (void);
void LedSleep             (void);

/* Console commands */
int  LedConsoleLed        (int argCount, char **pArguments);

#endif /* LED_H */
