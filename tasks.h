/**
 ******************************************************************************
 * @file   tasks.h
 ******************************************************************************
 *
 * @author Ripcord Designs
 * @date   29/10/2019
 *
 * @brief  Task Definitions.
 *
 ******************************************************************************
 */

#ifndef TASKS_H
#define TASKS_H

/* Task Handles*/

/* Task Priorities*/
#define BUTTON_MONITOR_PRIORITY     osPriorityLow
#define BATTERY_MONITOR_PRIORITY    osPriorityLow
#define MOTION_MONITOR_PRIORITY     osPriorityLow
#define CONSOLE_PRIORITY            osPriorityLow
#define SEQUENCER_PRIORITY          osPriorityLow
#define MODEM_PRIORITY              osPriorityNormal
#define GSM_COMMS_PRIORITY          osPriorityNormal
#define GNSS_COMMS_PRIORITY         osPriorityNormal
#define WIFI_PRIORITY               osPriorityLow

/*
 * Task stack sizes
 * NOTE: if stack sizes are increased, you may also need to increase configTOTAL_HEAP_SIZE in FreeRTOSCOnfig.h
 */
#define BUTTON_MONITOR_STACK_SIZE   256
#define BATTERY_MONITOR_STACK_SIZE  512
#define MOTION_MONITOR_STACK_SIZE   512
#define CONSOLE_STACK_SIZE          640
#define SEQUENCER_STACK_SIZE        512
#define MODEM_STACK_SIZE            512
#define GSM_COMMS_STACK_SIZE        512
#define GNSS_COMMS_STACK_SIZE       512
#define WIFI_STACK_SIZE             320

#endif /* TASKS_H */
