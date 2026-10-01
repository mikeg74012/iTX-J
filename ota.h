/**
 **************************************************************************************************
 * @file   ota.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  Header File - Handles OTA data storage and retrieve from SRAM.
 *
 **************************************************************************************************
 * @attention                   (c) Ripcord Designs Limited 2020
 **************************************************************************************************
 */
#ifndef OTA_H
#define OTA_H

#include <stdint.h>
#include <stdbool.h>


/*
 **************************************************************************************************
 *                                       PUBLIC DEFINES
 **************************************************************************************************
 */
#pragma pack(1)

typedef struct OTADataInformation
{
  uint8_t  UpdateFlag;      /* flag to initiate a firmware upgrade */
  uint32_t FileSize;        /* File size of the OTA image in SRAM */
  uint32_t FileCrc;         /* Checksum of the OTA image in SRAM */
  uint32_t FlashAddress;    /* FLASH Address for the OTA image to be stored to */
} OTA_Info_t;

#pragma pack()

/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */
/*
 * API */
bool      OtaProcessManifest        (void);
bool      OtaVerify                 (void);
void      OtaProgram                (void);
bool      OTAUpdateFWVerify         (void);
uint32_t  GetBLVersionFromSpiRAM    (void);
/* Console Commands */
int OTAImageConsoleUpgradeNow      (int argCount, char **pArguments);
int OTAImageConsoleSetInfo         (int argCount, char **pArguments);
int OTAImageConsoleGetInfo         (int argCount, char **pArguments);
int OTAImageConsoleVerify          (int argCount, char **pArguments);
int OTAImageConsoleProcessManifest (int argCount, char **pArguments);
int OTAImageConsoleUpdateBL        (int argCount, char **pArguments);


#endif /* OTA_H */
