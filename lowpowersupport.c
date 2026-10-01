/**
 **************************************************************************************************
 * @file   lowpowersupport.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  This module provides APIs support for low power sleep mode operations
 *
 **************************************************************************************************
 * @attention                      (c) Ripcord Designs Limited 2023
 **************************************************************************************************
 */

#include "global.h"

/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */
/*
 * The volatile data structure
 */
typedef struct
{
  /* the value need to keep during power reset sleep mode */
  /* status log count */
  uint8_t  LPSleepFlag;
  /*mini impact ram index */
  int      ImpactEventCount ;
  int      MiniEventCurrentIndex;
  /* motion monitor */
  uint32_t LastdhmTS;
  uint32_t PacketCounts;
  uint8_t  PrevPowerState;
  uint8_t  PrevImpactId;
  uint8_t  PrevRedButtonId;
  uint8_t  prevHPFindex;

  /* GSM related */
  uint32_t gsmLastOnlineTime;                   /* Last Known GSM Online Timestamp */

  /* Sequencer Heart Beat House Keep*/
  uint8_t seqHBHouseKeep;                       /* Sequencer HeartBeat House Keep related stuff*/
} LPResetKeptValue_t;


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */

/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static bool WriteResetDataToPSRAM (uint8_t resetFlag);
/*
 **************************************************************************************************
 *                                   PUBLIC SECTION / INTERFACE
 **************************************************************************************************
 */


/**
 * @brief   Write RTC trigged sleep/wakeup count to backup register
 * @return  none
 */
void SetSleepCount (uint32_t data)
{
  RtcBkupWriteRegister(RTC_BACKUP_STANDBY_SLEEP_COUNT, data);
}


/**
 * @brief   Read RTC trigged sleep/wakeup count from the backup register
 * @param   none
 * @return  uint32_t: return register value
 */
uint32_t GetSleepCount (void)
{
  return ((uint32_t)RtcBkupReadRegister(RTC_BACKUP_STANDBY_SLEEP_COUNT));
}


/**
 * @brief   Reset RTC backup system flag of the register value
 * @note    The flag value is reverse then AND to the read back register
 *
 * @param   flag: the value to write
 * @return  none
 */
void ResetLPSleepStatus (uint32_t flag)
{
  uint16_t temp = RtcBkupReadRegister(RTC_BACKUP_STANDBY_RESET_STATUS) & ((uint16_t)(~flag));
  RtcBkupWriteRegister(RTC_BACKUP_STANDBY_RESET_STATUS,  temp);
}


/**
 * @brief   Set RTC backup system flag bit of the register
 * @note    The flag value is OR to the read back register
 *
 * @param   flag: the value to write
 * @return  none
 */
void SetLPSleepStatus (uint32_t flag)
{
  RtcBkupWriteRegister(RTC_BACKUP_STANDBY_RESET_STATUS,  RtcBkupReadRegister(RTC_BACKUP_STANDBY_RESET_STATUS) | ((uint16_t)flag));
}


/**
 * @brief   Read RTC backup system STANDBY sleep flag register value
 * @param   none
 * @return  uint32_t: return register value
 */
uint32_t GetLPSleepStatus (void)
{
  return ((uint32_t)RtcBkupReadRegister(RTC_BACKUP_STANDBY_RESET_STATUS));
}


/**
 * @brief   return the Low Power Slept flag
 * @param   none
 * @return  bool: true if Low Power Sleep is on
 */
bool IsLowPowerSleepFlagUp (void)
{
  return ((GetLPSleepStatus() & RTC_BKUP_STANDBYSLEEP_FLAG) == RTC_BKUP_STANDBYSLEEP_FLAG);
}


/**
 * @brief   return the Low Power sleep mode true is selected
 * @param   none
 * @return  bool: true if Low Power Sleep mode is on
 */
bool IsLowPowerSleepModeSet (void)
{
  return ((GetLPSleepStatus() & RTC_BKUP_SLEEPMODE_STANDBY) == RTC_BKUP_SLEEPMODE_STANDBY);
}


/**
 * @brief    Set the standby sleep flag
 * @param     none
 * @return    none
 */
void SetupLPSleepModeFlag (void)
{
  /* Standby mode sleep will reset MCU */
  if (IsLowPowerSleepModeSet())
  {
    SetLPSleepStatus(RTC_BKUP_STANDBYSLEEP_FLAG);
  }
  else
  {
    ResetLPSleepStatus(RTC_BKUP_STANDBYSLEEP_FLAG);
  }
}


/**
 * @brief   Write standby sleep mode control bit
 * @param   mode      true/false
 * @return  none
 */
void SetStandbySleepMode (bool mode)
{
  if (mode)
  {
    SetLPSleepStatus(RTC_BKUP_SLEEPMODE_STANDBY);
  }
  else
  {
    ResetLPSleepStatus(RTC_BKUP_SLEEPMODE_STANDBY);
  }
}


/**
 * @brief   update the values need to be kept before reset.
 * @param   uint8_t ControlFlag: reset control flag
 * @return  none
 */
void UpdateResetDataToRAM (uint8_t ControlFlag)
{
  StatusLogWritetoPSRAM();    /* back up Status Log */
  FitmentDataHandler(MOUNT_SURVEY_BACKUP);  /* Save current mount survey data */
  WriteResetDataToPSRAM(ControlFlag);  /* set up the control flag and the static value block */
  
}


/**
 * @brief     Read reset data from SPI RAM
 * @param     none
 * @return    none
 */
bool ReadResetDataFromSpiRAM (void)
{
  bool success = false;
  uint32_t startAddress = SERIAL_RAM_LPSLEEP_KEEP_AREA_START;
  LPResetKeptValue_t resetData;

  int nl = sizeof(resetData);
  success = SerialRamRead(startAddress, (uint8_t*)&resetData, nl);
  if (success)
  {
    if (resetData.LPSleepFlag != PSRAM_LP_MCU_STANDBY_SLEEP_MARK)  /* for hibernate and reset */
    {
      SetSleepCount(0);
      /* LP sleep flag down at once for a normal reset process */
      ResetLPSleepStatus(RTC_BKUP_STANDBYSLEEP_FLAG);
    }
    else
    {
      /* restore backup values */
      SetLastDHMTime(resetData.LastdhmTS);
      MessageLPSSetPacketCount(resetData.PacketCounts);
      BatteryRestorePowerState(resetData.PrevPowerState);
      MessageRestoreIDs(ET_IMPACT, resetData.PrevImpactId);
      MessageRestoreIDs(ET_BUTTON, resetData.PrevRedButtonId);
      SetImpactEventCount(resetData.ImpactEventCount);
      SetMiniEventIndex(resetData.MiniEventCurrentIndex);
      RestoreLastHPfilterIndex(resetData.prevHPFindex);
      LPRestoreGsmLastOnlineTime(resetData.gsmLastOnlineTime);
      LPRestoreHeartbeatHouseKeep(resetData.seqHBHouseKeep);

      /* restore statistic */
      SerialRamRead(startAddress+nl, (uint8_t*)&Statistics, sizeof(Statistics_t));
      nl += sizeof(Statistics_t);
      /* restore the global ModemStatus_t ModemStatus value */
      SerialRamRead(startAddress+nl, (uint8_t*)&ModemStatus, sizeof(ModemStatus_t));
      nl += sizeof(ModemStatus_t);
      /* restore QInfo_t LPQueue, HPQueue values*/
      SerialRamRead(startAddress+nl, GetMessageQueueLPPoint(), GetMessageQueueInfoSize());
      nl +=  GetMessageQueueInfoSize();
      SerialRamRead(startAddress+nl, GetMessageQueueHPPoint(), GetMessageQueueInfoSize());
      nl +=  GetMessageQueueInfoSize();
      SerialRamRead(startAddress+nl, GetHPFilerBufferPointer(), GetHPFilerBufferSize());
      nl +=  GetHPFilerBufferSize();
    }
  }
  else
  {
    ConsolePrintf("Failed read back ResetData block from SPI RAM\r\n");
  }

  return (success);
}

/*
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */

/**
 * @brief     Write the keeping reset data to SPI RAM
 * @param     uint8_t resetFlag
 * @return    none
 */
static bool WriteResetDataToPSRAM (uint8_t resetFlag)
{
  bool success = false;
  LPResetKeptValue_t resetData;

  uint32_t startAddress = SERIAL_RAM_LPSLEEP_KEEP_AREA_START;
  resetData.LPSleepFlag = resetFlag;  /* always set this flag, a hibernate reset will miss this mark */
  resetData.ImpactEventCount = GetImpactEventCount();
  resetData.MiniEventCurrentIndex = GetMiniEventIndex();
  resetData.LastdhmTS = GetLastDHMTime();
  resetData.PacketCounts = MessageLPSGetPacketCount();
  resetData.PrevPowerState = BatteryMonitorGetPowerState();
  resetData.PrevImpactId = MessageGetIDs(ET_IMPACT);
  resetData.PrevRedButtonId = MessageGetIDs(ET_BUTTON);
  resetData.prevHPFindex = GetLastHPfilterIndex();
  resetData.gsmLastOnlineTime = GsmGprsGetLastOnLineTime();
  resetData.seqHBHouseKeep = SeqHeartbeatGetHouseKeep();

  int nl = sizeof (resetData);
  if (SerialRamWrite(startAddress, (uint8_t*)&resetData, nl))
  {
    /* write statistic */
    SerialRamWrite(startAddress+nl, (uint8_t*)&Statistics, sizeof(Statistics_t));
    nl += sizeof(Statistics_t);
    /* ModemStatus_t ModemStatus */
    SerialRamWrite(startAddress+nl, (uint8_t*)&ModemStatus, sizeof(ModemStatus_t));
    nl += sizeof(ModemStatus_t);
    /* QInfo_t LPQueue, HPQueue;*/
    SerialRamWrite(startAddress+nl, GetMessageQueueLPPoint(), GetMessageQueueInfoSize());
    nl +=  GetMessageQueueInfoSize();
    SerialRamWrite(startAddress+nl, GetMessageQueueHPPoint(), GetMessageQueueInfoSize());
    nl +=  GetMessageQueueInfoSize();
    SerialRamWrite(startAddress+nl, GetHPFilerBufferPointer(), GetHPFilerBufferSize());
    nl +=  GetHPFilerBufferSize();

    RtcPrintTimestamp(resetData.LastdhmTS);
    #ifdef DEBUG
      ConsolePrintf("-->Write ResetData block BC=%d ldhm=%u, PC=%u, Bspace = %d\r\n", nl, resetData.LastdhmTS, resetData.PacketCounts, SERIAL_RAM_LPSLEEP_KEEP_AREA_SIZE - nl);
    #endif
  }
  else
  {
    ConsolePrintf("Failed backup ResetData block to SPI RAM\r\n");
  }
  return (success);
}


