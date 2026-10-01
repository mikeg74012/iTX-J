/**
 **************************************************************************************************
 * @file   power.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   10/12/2019
 *
 * @brief  Power Management functions.
 *
 **************************************************************************************************
 */

#ifndef POWER_H
#define POWER_H

#include <stdbool.h>
#include <stdint.h>



/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
/* For buffer handling we must have byte-packed structures and arrays */
#pragma pack(1)



typedef enum ResetSources
{
  RESET_HIBERNATE = 0u,  // default state
  RESET_WATCHDOGWIN,
  RESET_WATCHDOGIDG,
  RESET_SOFTWARE,
  RESET_EXTERNALRESETPIN,
  RESET_EXTERNALRESETOPTBYT,
  RESET_EXTERNALRESETPORPDR,
  RESET_LOWPOWER,
  RESET_BATTERYREMOVED,
  RESET_LOW_BATTERY,
  RESET_RB_LHOLD,
  RESET_FW_UPDATE,
  RESET_BL_UPDATE,

  RESET_SYSFAULT_NONE = 40,
  RESET_SYSFAULT_NMI_FAULT,
  RESET_SYSFAULT_HARD_FAULT,
  RESET_SYSFAULT_MEMMANAGE_FAULT,
  RESET_SYSFAULT_BUS_FAULT,
  RESET_SYSFAULT_USAGE_FAULT,
  RESET_SYSFAULT_DEBUGMON_FAULT,
  RESET_UNKNOWN = 99,
  RESET_BKUP_NONE = 100

} eResetCause_t;


#pragma pack()

/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
void          PowerPutPeripheralsToSleep              (void);
void          PowerWakeupPeripherals                  (void);
eResetCause_t PowerGetResetCause                      (void);
eResetCause_t PowerGetResetCauseFromRTCBkupRegister   (void);
void          PowerStartHibernation                   (void);
void          PowerResetMCU                           (void);
void          SetBackupResetRegister                  (uint32_t resetCause);

#endif /* POWER_H */
