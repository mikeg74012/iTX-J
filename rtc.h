/**
 **************************************************************************************************
 * @file   rtc.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  On-chip Real Time Clock module
 *
 **************************************************************************************************
 * @attention                   (c) Ripcord Designs Limited 2020
 **************************************************************************************************
 */
#ifndef RTC_H_
#define RTC_H_

#include <time.h>

/*
 **************************************************************************************************
 *                                       PUBLIC DEFINES
 **************************************************************************************************
*/

#define RTC_BACKUP_REGISTER_TESTMODE      BKP_DATA_0  /* 0-41, uint16_t */
#define RTC_BACKUP_REGISTER_CLOCKSET      BKP_DATA_1
#define RTC_BACKUP_STANDBY_SLEEP_COUNT    BKP_DATA_2
#define RTC_BACKUP_STANDBY_RESET_STATUS   BKP_DATA_3  /* Low power sleep status reg */
#define RTC_BACKUP_FIRMWARE_VERSION       BKP_DATA_4  /* used for OTA update check */
#define RTC_BACKUP_BATTERY_VOLTAGE        BKP_DATA_5  /* battery voltage back up */
#define RTC_BACKUP_TIMESTAMP              BKP_DATA_7  /* sleep at TS: range 0-3600 */
#define RTC_BACKUP_LAST_GASREADTIME       BKP_DATA_8
#define RTC_BACKUP_HIBERNATE_FLAG         BKP_DATA_9
#define RTC_BACKUP_REGISTER_RESET_CAUSE   BKP_DATA_10
#define RTC_BACKUP_REGISTER_CLOCK_SYNCED  BKP_DATA_11
#define RTC_BACKUP_HARDWARE_VERSION       BKP_DATA_12
#define RTC_BACKUP_HARDWARE_TYPE          BKP_DATA_13

/* magic value for the RTC clock is synced */
#define RTC_CLOCK_SYNCED                  0x0FED


/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */
/* API */
bool        RtcInit                 (void);
uint32_t    RtcDateTimeToTimeStamp  (struct tm dateTime);
struct tm   RtcTimeStampToDateTime  (uint32_t timestamp);
uint32_t    RtcGetTimeStamp         (void);
struct tm   RtcGetDateTime          (void);
void        RtcSetDateTime          (struct tm dateTime, bool validSyncSrc);
void        RtcPrintCurrentTime     (void);
void        RtcPrintTime            (struct tm dateTime);
void        RtcPrintTimestamp       (uint32_t timestamp);
bool        RtcGetWakeupFlag        (void);
void        RtcSetWakeupTimer       (uint32_t WakeUpDelayInSeconds);
void        RtcClearWakeupTimer     (void);
void        RtcEXTICallback         (void);
void        RtcFlagCallback         (void);
bool        RtcTimeValid            (void);
void        RtcSyncedFlagVerify     (bool flag);

/* Console Commands */
int         RtcConsoleDefault       (int ArgCount, char **pArguments);
int         RtcConsoleSet           (int ArgCount, char **pArguments);
int         RtcConsoleGet           (int ArgCount, char **pArguments);
int         RtcConsoleTimeStamp     (int argCount, char **pArguments);


#endif /* RTC_H_ */
