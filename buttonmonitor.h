/**
 ******************************************************************************
 * @file   buttonmonitor.h
 ******************************************************************************
 *
 * @author Ripcord Designs
 * @date   29/10/2019
 *
 * @brief  Button Monitor Thread main file.
 *
 ******************************************************************************
 */

#ifndef BUTTONMONITOR_H
#define BUTTONMONITOR_H

/*
 * Public Defines
 */

/**
 * PUBLIC PROTOTYPES
 */
void ButtonMonitorTask          (void const * argument);
void ButtonMonitorPressCallback (void);
bool ButtonMonitorIsButtonDown  (void);
bool SetButtonDownFlag          (bool state);
void ButtonMonitorHoldCountInc  (void);

#endif /*BUTTONMONITOR_H */
