/**
 ******************************************************************************
 * @file   hardwareversion.h
 ******************************************************************************
 *
 * @author Ripcord Designs
 * @date   07/08/2023
 *
 * @brief  hardware detection Driver header
 *
 ******************************************************************************
 * @attention                   (c) Ripcord Designs Limited 2023
 **************************************************************************************************
 */

#ifndef HARDWAREVERSION_H
#define HARDWAREVERSION_H

/*
 **************************************************************************************************
 *                                       PUBLIC DEFINES
 **************************************************************************************************
 */
/* Board HW Version Detection Pin */
#define HWBOARD_VERSION_DF_PORT                 IO_PORT_B
#define HWBOARD_VERSION_DF_PIN                  IO_PIN_1
#define HWBOARD_VERSION_DDE_PORT                IO_PORT_A
#define HWBOARD_VERSION_DDE_PIN                 IO_PIN_0
#define HWBOARD_VERSION_DD2_PORT                IO_PORT_C
#define HWBOARD_VERSION_DD2_PIN                 IO_PIN_0
#define HWBOARD_VERSION_USER_BUT_PORT           IO_PORT_B
#define HWBOARD_VERSION_USER_BUT_PIN            IO_PIN_11


#define STANDBY_SLEEP_SUPPORT      0x08    /* hardware support bit mask write to RTC register */
#define DEEP_SLEEP_SUPPORT         0x00    /* HW version A,B,C,D only support Deep sleep mode */

#define HW_PCB_VERSION_F           5
#define HW_PCB_VERSION_E           4
#define HW_PCB_VERSION_D           3
#define HW_PCB_VERSION_BC          2
#define HW_PCB_VERSION_A           1
#define HW_PCB_VERSION_NONE        0

#define HW_VERSION_NAME(version) \
    ((version == HW_PCB_VERSION_F) ? "F" : \
    (version == HW_PCB_VERSION_E) ? "E" : \
    (version == HW_PCB_VERSION_D) ? "D" : \
    (version == HW_PCB_VERSION_BC) ? "BC" : \
    (version == HW_PCB_VERSION_A) ? "A" : \
    (version == HW_PCB_VERSION_NONE) ? "NONE" : "Unknown")

/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */
void  HardwareVersionCheck      (void);
bool  isPCBVersionF             (void);
bool  isPCBVersionDE            (void);
bool  isPCBVersionD2            (void);
bool  isPCBVersionA             (void);
void  SetHardwareVersion        (uint8_t hwVersion);
int   GetHardwareVersion        (void);


#endif /* HARDWAREVERSION_H */
