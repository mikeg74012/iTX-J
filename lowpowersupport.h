/**
 **************************************************************************************************
 * @file   lowpowersupport.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   2023-08-01
 *
 * @brief  Driver for MUC low power standby sleep mode support and its wake up logic chip
 *
 **************************************************************************************************
 * @attention                   (c) Ripcord Designs Limited 2023
 **************************************************************************************************
 */
#ifndef LOWPOWERSUPPORT_H
#define LOWPOWERSUPPORT_H

#include "main.h"
#include "rtc.h"
/*
 **************************************************************************************************
 *                                       PUBLIC DEFINES
 **************************************************************************************************
 */
/* flag bit */
#define RTC_BKUP_STANDBYSLEEP_FLAG         0x01       /* LP sleep flag */
#define RTC_BKUP_WAKEUP_CHECK_FLAG         0x02       /* LP mode wake up checking flag */
#define RTC_BKUP_ACCEL_INT2_FLAG           0x04       /* wkup pin sate before LP sleep */
#define RTC_BKUP_REDBUTTON_FLAG            0x08       /* wkup pin sate before LP sleep */
#define RTC_BKUP_USB_CABLE_FLAG            0x10       /* wkup pin sate before LP sleep */
#define RTC_BKUP_CHARGER_FLAG              0x20       /* Charge status before LP sleep */
#define RTC_BKUP_SLEEPMODE_STANDBY         0x80       /* control the sleep mode when go to sleep  */

/* sleep action count down setting */
#define PSRAM_LP_MCU_STANDBY_SLEEP_MARK    0xAA
#define PSRAM_SYSTEM_FAULT_RESET_MARK      0x55


/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */


void        SetLPSleepStatus        (uint32_t flag);
void        ResetLPSleepStatus      (uint32_t flag);
uint32_t    GetLPSleepStatus        (void);
void        SetSleepCount           (uint32_t data);
uint32_t    GetSleepCount           (void);
void        SetStandbySleepMode     (bool mode);
bool        IsLowPowerSleepModeSet  (void);
void        SetupLPSleepModeFlag    (void);
bool        IsLowPowerSleepFlagUp   (void);
void        UpdateResetDataToRAM    (uint8_t ControlFlag);
bool        ReadResetDataFromSpiRAM (void);

#endif /* LOWPOWERSUPPORT_H */
