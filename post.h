/**
 **************************************************************************************************
 * @file   post.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   27/01/2022
 *
 * @brief  Power on self test module
 *
 **************************************************************************************************
 */

#ifndef POST_H
#define POST_H

#include <stdbool.h>
#include <stdint.h>
#include "gasgauge.h"
#include "xyzfilter.h"
#include "power.h"

/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
/* For buffer handling we must have byte-packed structures and arrays */
#pragma pack(1)

/*
 * Flag in Config to indicate whether Wake-Up Self-Test has been carried out.
 * (update according if no. of devices in WUSTDevices tested changes )
 */
#define WUST_DONE       0x0555u   // max value 0x5555 for WUSTDevicesMAX
#define WUST_NOT_DONE   0x0000u
#define WUST_DONE_PASS  0x0FFFu

typedef enum WUSTDevices
{
  IMU = 0u,
  SRAM,
  MODEM,
  WIFI,
  BATTERYCHARGER,
  GASGUAGE,
  WUSTDevicesMAX = 7u
} eWUSTDeviceID_t;

typedef struct SemanticVersionInfo
{
  uint8_t MajorVersion;
  uint8_t MinorVersion;
  uint8_t PatchVersion;
} eVersion_Info_t;

typedef struct WUSTReportInfo
{
  eVersion_Info_t          AppFirmwareVersion;  // holds device firmware version
  uint16_t                 WUSTFlag;            // holds result of WUST
  uint8_t                  batteryPercentage;
  eResetCause_t            resetCause;          // register reset cause
  GASGAUGE_BatteryStatus_t batteryData;         // holds battery information
  AccelerationData_t       accelDataRef;        // holds XYZ reference data
} WUST_ReportInfo_t;

#pragma pack()

/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
void PostWakeUpSelfTest                      (eWUSTDeviceID_t deviceTypeID);
void PostWUSTSetBatteryInfo                  (GASGAUGE_BatteryStatus_t batteryInfo);
WUST_ReportInfo_t  getWUSTReportInfoSnapShot (void);

#endif /* POST_H */
