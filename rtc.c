/**
 **************************************************************************************************
 * @file   rtc.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  This module provides an abstraction layer for the GD32 on-chip RTC peripheral.
 *         It shares a common Ripcord API with RTC modules for other chips, so higher software
 *         layers can be easily moved between different MCU's.
 *
 **************************************************************************************************
 * @attention                      (c) Ripcord Designs Limited 2022
 **************************************************************************************************
 */
#include "global.h"

/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */
#define RTC_SYNC_PREDIV                 0x3FF
#define RTC_ASYNC_PREDIV                0x1F

#define SECONDS_SINCE_1970_TO_2000      946684800UL

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
static void       nvic_Rtc_configuration    (void);


/*
 **************************************************************************************************
 *                                   PUBLIC SECTION / INTERFACE
 **************************************************************************************************
 */

/**
 * @brief  Initialise the Real Time Clock peripheral
 * @param  None
 * @retval None
 */
bool RtcInit (void)
{
  /* set RTC IRQn */
  nvic_Rtc_configuration ();

  if (read_register (BKP, RTC_BACKUP_REGISTER_CLOCKSET) != 0xA5A5)  /* not set up yet, first time  */
  {
    /* enable PMU and BKPI clocks */
    rcu_periph_clock_enable(RCU_BKPI);
    rcu_periph_clock_enable(RCU_PMU);

    /* allow access to BKP domain */
    pmu_backup_write_enable();

    /* reset backup domain */
    bkp_deinit();

    /* enable LXTAL */
    rcu_osci_on(RCU_LXTAL);
    rcu_osci_stab_wait(RCU_LXTAL);

    /* select RCU_LXTAL as RTC clock source */
    rcu_rtc_clock_config(RCU_RTCSRC_LXTAL);

    /* enable RTC Clock */
    rcu_periph_clock_enable(RCU_RTC);
    /* wait for RTC registers synchronization */
    rtc_register_sync_wait();

    /* wait until last write operation on RTC registers has finished */
    rtc_lwoff_wait();

    /* enable the RTC alarm interrupt*/
    rtc_interrupt_enable(RTC_INT_ALARM);
    rtc_lwoff_wait();

    /* set RTC prescaler: set RTC period to 1s */
    rtc_prescaler_set(32767);
    rtc_lwoff_wait();

    bkp_write_data(RTC_BACKUP_REGISTER_CLOCKSET, 0xA5A5); /* set flag */
  }
  else
  {
    /* allow write access to BKP domain */
    rcu_periph_clock_enable(RCU_BKPI);
    rcu_periph_clock_enable(RCU_PMU);
    pmu_backup_write_enable();

    /* No need to reconfig RTC if it setup already */
    /* The RTC core (prescaler, divider, counter and alarm) is reset only by a backup domain reset.*/
  }

  return true;
}


/**
 * @brief    Convert DateTime_t into seconds since 1/1/2000 00:00:00
 *           Handles dates up to 2106
 * @param    dateTime - the date and time to convert to a timestamp
 * @retval   seconds since 1/1/2000 00:00:00
 */
uint32_t RtcDateTimeToTimeStamp (struct tm dateTime)
{
  static time_t timeStampUnix;
  timeStampUnix = mktime(&dateTime);
  return (uint32_t)(timeStampUnix - (time_t)SECONDS_SINCE_1970_TO_2000);
}


/**
 * @brief    Convert seconds since 1/1/2000 00:00:00 into DateTime_t
 *           Handles dates up to 2106
 * @param    timestamp - seconds since 1/1/2000 00:00:00
 * @retval   dateTime - the date and time convert from the timestamp
 */
struct tm RtcTimeStampToDateTime (uint32_t timestamp)
{
  static time_t timeStampUnix;
  static struct tm DateTime;

  timeStampUnix = (time_t)(timestamp + SECONDS_SINCE_1970_TO_2000);
  localtime_r(&timeStampUnix, &DateTime);
  return DateTime;
}


/**
 * @brief   Get the current RTC time as a timestamp (seconds since 1/1/2000 00:00:00)
 * @param   none
 * @retval  seconds since 1/1/2000 00:00:00
 */
uint32_t RtcGetTimeStamp (void)
{
  uint32_t rtn = 0;

    rtn = rtc_counter_get();
    
  return (rtn);
}


/**
 * @brief   Get the current RTC time and date in tm format
 * @param   none
 * @retval  struct tm - current date and time
 */
struct tm RtcGetDateTime (void)
{
  struct tm DateTime;

    uint32_t itvalue = rtc_counter_get();
    DateTime = RtcTimeStampToDateTime(itvalue);
    
  return DateTime;
}


/**
 * @brief   Set the current RTC time and date and update the rtc Valid flag
 * @param   dateTime - the date and time to set
 * @param   validSyncSrc - true to set the rtc valid flag
 * @retval  none
 */
void RtcSetDateTime (struct tm dateTime, bool validSyncSrc)
{
    uint32_t temp = RtcDateTimeToTimeStamp(dateTime);
#ifdef DEBUG
      RtcPrintTime (dateTime);
#endif
    /* update current date,time */
    rtc_counter_set(temp);
    if (validSyncSrc)
    {
      /* Mark the RTC clock is valid */
      write_register (BKP, RTC_BACKUP_REGISTER_CLOCK_SYNCED, RTC_CLOCK_SYNCED); /* set flag */
    }
}


/**
 * @brief   Find out if the wake up event has occurred since this function was last called
 * @param   none
 * @return  true if wakeup event has occured, false otherwise
 */
bool RtcGetWakeupFlag (void)
{
  if (rtc_flag_get(RTC_FLAG_ALARM) != RESET)
  {
    pmu_flag_clear(PMU_FLAG_RESET_WAKEUP);   /* clear wake up flag*/
    rtc_flag_clear(RTC_FLAG_ALARM);          /* clear the RTC second interrupt flag*/
    return true;
  }
  return false;
}

/**
 * @brief   return if the RTC is synced ever
 * @param   none
 * @return  true if the rtc is synced(NTP/GNSS/CONSOLE)
 */
bool RtcTimeValid(void)
{
  if (bkp_read_data(RTC_BACKUP_REGISTER_CLOCK_SYNCED) != RTC_CLOCK_SYNCED)  /* not set up yet, first time  */
  {
    return false;
  }
  return true;
}


/**
 * @brief   Set the wakeup timer to wake the device after a delay
 * @param   WakeUpDelayInSeconds : wake up delay in seconds
 * @return  none
 */
void RtcSetWakeupTimer (uint32_t WakeUpDelayInSeconds)
{
  /* clear wakeup flag if previously set */
  if (rtc_flag_get(RTC_FLAG_ALARM) != RESET)
  {
    pmu_flag_clear(PMU_FLAG_RESET_WAKEUP);   /* clear wake up flag*/
    rtc_flag_clear(RTC_FLAG_ALARM);          /* clear the RTC second interrupt flag*/
  }

  /* Set up new periodic wake-up alarm */
  rtc_interrupt_disable(RTC_FLAG_ALARM);        /* RTC alarm off */
  uint32_t temp = rtc_counter_get() + WakeUpDelayInSeconds;       /* get TS then set alarm  */
  rtc_flag_clear(RTC_FLAG_ALARM);
  rtc_alarm_config(temp);                      /* set alarm */
  rtc_interrupt_enable(RTC_FLAG_ALARM);        /* RTC alarm on */
  exti_init(EXTI_17, EXTI_EVENT, EXTI_TRIG_RISING); /* set RTC alarm wake up event */
  exti_interrupt_flag_clear(EXTI_17);
}


/**
 * @brief   Clear the wake up timer to prevent it triggering
 * @param   WakeUpDelayInSeconds : wake up delay in seconds
 * @return  none
 */
void RtcClearWakeupTimer (void)
{
  rtc_interrupt_disable(RTC_FLAG_ALARM);   /* RTC alarm off */
  rtc_flag_clear(RTC_FLAG_ALARM);          /* clear the RTC alarm interrupt flag*/
  pmu_flag_clear(PMU_FLAG_RESET_WAKEUP);
}


/**
 * @brief   Print the current time to the console
 * @param   none
 * @return  none
 */
void RtcPrintCurrentTime (void)
{
  /* get the current date and time */
  struct tm DateTime = RtcGetDateTime();

  /* print to console */
  RtcPrintTime(DateTime);
}


/**
 * @brief   Print the provided timestamp to the console
 * @param   timestamp - the time to print
 * @return  none
 */
void RtcPrintTimestamp (uint32_t timestamp)
{
  /* get the current date and time */
  struct tm DateTime = RtcTimeStampToDateTime(timestamp);

  /* print to console */
  RtcPrintTime(DateTime);
}


/**
 * @brief   Print the provided DateTime to the console
 * @param   dateTime - the time to print
 * @return  none
 */
void RtcPrintTime (struct tm dateTime)
{
  /* Display date Format : dd-mm-yy */
  ConsolePrintf("%02d-%02d-%4d ", dateTime.tm_mday, dateTime.tm_mon + 1, dateTime.tm_year + 1900);
  /* Display time Format : hh:mm:ss */
  ConsolePrintf("%02d:%02d:%02d ", dateTime.tm_hour, dateTime.tm_min, dateTime.tm_sec);
}


/**
 * @brief   RTC interrupt call back handler
 * @param   none
 * @return  none
 */
void RtcFlagCallback (void)
{
  if (rtc_flag_get(RTC_FLAG_SECOND) != RESET)
  {
    /* clear the RTC second interrupt flag*/
    rtc_flag_clear(RTC_FLAG_SECOND);
  }
  if (rtc_flag_get(RTC_FLAG_ALARM) != RESET)
  {
    /* clear the RTC alarm interrupt flag*/
    rtc_flag_clear(RTC_FLAG_ALARM);
  }
}


/**
 * @brief   RTC interrupt call back handler
 * @param   none
 * @return  none
 */
void RtcEXTICallback (void)
{
  if (exti_flag_get(EXTI_17) != RESET)
  {
    exti_interrupt_flag_clear(EXTI_17);
  }
}




/**
 * @brief   Checking to make sure Synced flag have set to the verify state.
 * @param   bool: flag state to verify for
 * @return  none
 */
void RtcSyncedFlagVerify (bool flag)
{
  if (!RtcTimeValid())
  {
    if (flag)
    {
      write_register (BKP, RTC_BACKUP_REGISTER_CLOCK_SYNCED, RTC_CLOCK_SYNCED); /* set flag */
    }
    else
    {
      write_register (BKP, RTC_BACKUP_REGISTER_CLOCK_SYNCED, 0x0);      /* reset flag */
    }
  }
}


/*
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */
/**
* @brief        configure the RTC global interrupt
* @details      enable RTC_IRQn and set the priority level
*
* @param[in]    none
* @return       {none}
*/
static void nvic_Rtc_configuration (void)
{
  nvic_priority_group_set(NVIC_PRIGROUP_PRE1_SUB3);
  nvic_irq_enable(RTC_IRQn,6,0);
}


/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       Set a default time and date
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int RtcConsoleDefault (int argCount, char **pArguments)
{
  struct tm DateTime;

  /*##-1- Configure the Date #################################################*/
  /* Set Date: Tuesday January 1st 2020 */
  DateTime.tm_mday = 01;
  DateTime.tm_mon  = 00;          /* tm_mon format is months since Jan (so Jan is 0) */
  DateTime.tm_year = 2020 - 1900; /* tm_year format is years since 1900 */
  DateTime.tm_wday = 6;  /* 1-7 for Monday to Sunday, 2000-1-1 is Saturday */

  /*##-2- Configure the Time #################################################*/
  /* Set Time: 02:00:00 */
  DateTime.tm_hour = 02;
  DateTime.tm_min  = 00;
  DateTime.tm_sec  = 00;

  RtcSetDateTime(DateTime, false);  /* no valid flag for this default RTC setting */

  /* indicate success */
  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       Set the current time and date
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int RtcConsoleSet (int argCount, char **pArguments)
{
  struct tm DateTime;
  int Hour;
  int Minute;
  int Second;
  int Day;
  int Month;
  int Year;
  int wDay = 6;   // Default week of day as Saturday if not provided (ref Default Date is 2000-1-1)  

  if ((2 == argCount) || (3 == argCount))
  {
    int inputWDay;

    /* get the date and time from the command line */
    sscanf(pArguments[0],  "%d-%d-%d", &Day, &Month, &Year);
    sscanf(pArguments[1],  "%d:%d:%d", &Hour, &Minute, &Second);

    DateTime.tm_hour = Hour;
    DateTime.tm_min  = Minute;
    DateTime.tm_sec  = Second;
    DateTime.tm_mday = Day;
    DateTime.tm_mon  = Month - 1;   /* tm_mon format is months since January (Jan = 0) */
    DateTime.tm_year = Year - 1900; /* tm_year format is years since 1900 */


    if (3 == argCount && sscanf(pArguments[2], "%d", &inputWDay) == 1)
    {
      /* validate the week of day */
      if ((inputWDay >= 1) && (inputWDay <= 7))
      {
        wDay = inputWDay;
      }
    }

    DateTime.tm_wday = wDay;

    RtcSetDateTime(DateTime, true);  /* set from console, mark it as valid */

    /* indicate success */
    return (CONSOLE_ERROR_OK);
  } 
  return (CONSOLE_ARG_NO);
}


/**
 * @brief       Get the current time and date
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int RtcConsoleGet (int argCount, char **pArguments)
{
  RtcPrintCurrentTime();
  ConsolePrintf("\n\r");
  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       Get the current timestamp (seconds since 1/1/2000 00:00:00)
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int RtcConsoleTimeStamp (int argCount, char **pArguments)
{
  /* get the current date and time */
  uint32_t TimeStamp = RtcGetTimeStamp();

  /* Display the timestamp value */
  ConsolePrintf("%d\n\r", TimeStamp);

  /* indicate success */
  return (CONSOLE_ERROR_OK);
}
