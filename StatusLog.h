/**
 ******************************************************************************
 * @file   statuslog.h
 ******************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  Simple status log
 *
 ******************************************************************************
 */
#ifndef STATUSLOG_H
#define STATUSLOG_H


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
#pragma pack(1)

typedef struct
{
  uint32_t  TimeStamp;    // time of the event
  uint8_t   EventCode;
  uint8_t   EventData0;
  uint8_t   EventData1;
  uint8_t   EventData2;
  uint8_t   EventData3;
} StatusLog_t;

#pragma pack()

/* Event Codes */
#define STATUS_DATA_LOAD_NONE       0
#define STATUS_USB_ATTACH           (1|STATUS_INSTANT_REPORT_MASK)        /* byte0=byte1=byte2=byte3 = 0 by default for all not defined event code */
#define STATUS_USB_DETACH           (2|STATUS_INSTANT_REPORT_MASK)
#define STATUS_RED_BUTTON_CANCELLED 3
#define STATUS_CHARGING_ERROR       10
#define STATUS_CHARGING_STARTED     11       /* charger pay loader */
#define STATUS_CHARGING_TERMINATED  (12|STATUS_INSTANT_REPORT_MASK)       /* byte0 = charge to %, byte1 = charge added %, byte2+byte3*256 = charge time in second */
#define STATUS_CHARGING_COMPLETE    13       /* byte0 = charge to %, byte1 = charge added %, byte2+byte3*256 = charge time in second */
#define STATUS_CHARGING_ENABLED     14
#define STATUS_CHARGING_DISABLED    15
#define STATUS_BATTERY_ERROR        20
#define STATUS_BATTERY_LOW          21       /* charger pay loader */
#define STATUS_BATTERY_FULL         22       /* charger pay loader */
#define STATUS_BATTERY_EMPTY        23       /* charger pay loader */
#define STATUS_BATTERY_NORMAL       24       /* charger pay loader */
#define STATUS_BATTERY_RAM0_ERROR   25      /* Battery record write to RAM0 verify failed this time, 3 times repeat  */
#define STATUS_BATTERY_VOLTAGE_ERR  26      /* Catch negative voltage return from GAS GUAGE  */
#define STATUS_IMU_ERROR            30
#define STATUS_IMU_GFORECE_EVENT    31
#define STATUS_IMU_WOKE_WITH_NO_SOJ (32|STATUS_INSTANT_REPORT_MASK)      /* Device being woken up by IMU, but no SOJ Detected */
#define STATUS_MODEM_ERROR          40
#define STATUS_MODEM_IMEI_RETRY     41
#define STATUS_MODEM_NETWORK_ERR    42
#define STATUS_GNSS_ERROR           50
#define STATUS_GNSS_NO_INITIAL_FIX  51      /* no initial fix for 600s after Journey Start */
#define STATUS_GNSS_JAMMER_DETECTED 52
#define STATUS_GNSS_JAMMER_OFF      (53|STATUS_INSTANT_REPORT_MASK)       /* if it was on previously */
#define STATUS_HEAD_DATE_DRIFT      54      /* RTC date string drift detected */
#define STATUS_MEMORY_ERROR         60
#define STATUS_CONFIG_ERROR         70
#define STATUS_CONFIG_MOUNT_STATE   71      /* pitch and roll during Journey and Stationary */
#define STATUS_CONFIG_OFF_MOUNT     (72|STATUS_INSTANT_REPORT_MASK)      /* unit off the mounting position */
#define STATUS_OTA_ERROR            80
#define STATUS_OTA_BL_PROGRAM_ERR   81
#define STATUS_OTA_BL_VERIFY_ERR    82
#define STATUS_OTA_FW_SAMEVERSION   83
#define STATUS_WIFI_ERROR           90
#define STATUS_BLUETOOTH_ERROR      100

/* Event code for instantaneous report to cloud , Larger than 128 */
#define STATUS_INSTANT_REPORT_MASK  0x80

#define STATUS_DUMMY_EVENT          127  /* used for test */

/* Used for instant message, Magic token to tell create status Log entry instead of counting */
/* If data has NO token in it, it will increase the event count, instead of creating an Status Log Entry */
/* If data has token in it (Used when pack the event to status log), it will create entry with payload instead of counting*/
#define INSTANT_DATA_PAYLOAD             0xA5A5

/* Payload for counting only (lower 2 bytes)*/
#define STATUS_PAYLOAD_COUNT_ONLY        0xA5A50000
/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */
/* API */
void     StatusLogInitialize          (void);
bool     StatusLogWriteEventCode      (uint8_t eventCode,  uint32_t data);
bool     StatusLogReadEvent           (StatusLog_t * pEvent, bool delete);
uint8_t  StatusLogGetEventCount       (void);
bool     StatusLogWritetoPSRAM        (void);
bool     StatusLogInstantEventMatureCheck  (void);
void     StatusLogAppendInstantEventRecords (void);

/* Console Commands */
int      StatusLogConsoleList         (int ArgCount, char **pArguments);
int      StatusLogConsoleClear        (int ArgCount, char **pArguments);
int      StatusLogConsoleAdd          (int ArgCount, char **pArguments);
int      StatusLogConsoleRemove       (int ArgCount, char **pArguments);

#endif /* STATUSLOG_H */
