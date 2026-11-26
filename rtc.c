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
     
    or_register (RCU, RCU_APB1EN, (3 << 27));

    /* allow access to BKP domain */
    or_register (PMU, PMU_CTL, (1 << 8));

    /* reset backup domain */
    
    or_register (RCU, RCU_BDCTL, (1 << 16));
    and_register (RCU, RCU_BDCTL, ~(1 << 16));
    
    /* enable LXTAL */
    
    or_register (RCU, RCU_BDCTL, (1 << 0));
    
    while (!(read_register (RCU, RCU_BDCTL) & (1 << 1)));

    /* select RCU_LXTAL as RTC clock source */
    
    write_register (RCU, RCU_BDCTL, (read_register (RCU, RCU_BDCTL) & ~(3 << 8)) | (1 << 8));
    
    /* enable RTC Clock */
    
    or_register (RCU, RCU_BDCTL, (1 << 15));
    
    /* wait for RTC registers synchronization */
    
    while (!(read_register (RTC, RTC_CTL) & (1 << 3)));
    
    /* wait until last write operation on RTC registers has finished */
    while (!(read_register (RTC, RTC_CTL) & (1 << 5)));
    
    /* enable the RTC alarm interrupt*/
    
    or_register (RTC, RTC_INTEN, (1 << 1));

    while (!(read_register (RTC, RTC_CTL) & (1 << 5)));
    
    /* set RTC prescaler: set RTC period to 1s */
    
    write_register (RTC, RTC_PSCL, 32767);
    write_register (RTC, RTC_PSCH, 0);
    
    while (!(read_register (RTC, RTC_CTL) & (1 << 5)));
    
    write_register (BKP, RTC_BACKUP_REGISTER_CLOCKSET, 0xA5A5); /* set flag */
  }
  else
  {
    /* allow write access to BKP domain */
    /* enable PMU and BKPI clocks */
     
    or_register (RCU, RCU_APB1EN, (3 << 27));

    /* allow access to BKP domain */
    or_register (PMU, PMU_CTL, (1 << 8));


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
  struct tm *result;

  timeStampUnix = (time_t)(timestamp + SECONDS_SINCE_1970_TO_2000);
  result = localtime(&timeStampUnix);
  
  memcpy (&DateTime, result, sizeof (struct tm));
  return DateTime;
}

void rtc_configuration_mode_enter(void)
{
    or_register (RTC, RTC_CTL, (1 << 4));
}

/*!
    \brief      exit RTC configuration mode 
    \param[in]  none
    \param[out] none
    \retval     none
*/
void rtc_configuration_mode_exit(void)
{
    and_register (RTC, RTC_CTL, ~(1 << 4));
}


void rtc_counter_set(uint32_t cnt)
{
    rtc_configuration_mode_enter();
    /* set the RTC counter high bits */
    
    write_register (RTC, RTC_CNTH, cnt >> 16);
    
    /* set the RTC counter low bits */
    write_register (RTC, RTC_CNTL, cnt & 0xffff);
    
    rtc_configuration_mode_exit();
}

uint32_t rtc_counter_get(void)
{
    uint32_t temp;
    
    temp = read_register (RTC, RTC_CNTL);
    
    temp |= read_register (RTC, RTC_CNTL) << 16;
    
    return temp;
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
    if (read_register (RTC, RTC_CTL) & (1 << 1))
    {
        or_register (PMU, PMU_CTL, (1 << 2));       /* Reset wakeup flag */
        
        and_register (RTC, RTC_CTL, ~(1 << 1));     /* clear the RTC second interrupt flag*/
        
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
  if (read_register (BKP, RTC_BACKUP_REGISTER_CLOCK_SYNCED) != RTC_CLOCK_SYNCED)  /* not set up yet, first time  */
  {
    return false;
  }
  return true;
}

void rtc_alarm_config(uint32_t alarm)
{
    rtc_configuration_mode_enter();
    /* set the alarm high bits */
    
    write_register (RTC, RTC_ALRMH, alarm >> 16);
    
/* set the alarm low bits */

    write_register (RTC, RTC_ALRML, alarm & 0xffff);
    
    rtc_configuration_mode_exit();
}

/**
 * @brief   Set the wakeup timer to wake the device after a delay
 * @param   WakeUpDelayInSeconds : wake up delay in seconds
 * @return  none
 */
void RtcSetWakeupTimer (uint32_t WakeUpDelayInSeconds)
{
  /* clear wakeup flag if previously set */
  if (read_register (RTC, RTC_CTL) & (1 << 1))
  {
    or_register (PMU, PMU_CTL, (1 << 2));       /* Reset wakeup flag */
    
    and_register (RTC, RTC_CTL, ~(1 << 1));
  }

  /* Set up new periodic wake-up alarm */
  
    and_register (RTC, RTC_INTEN, ~(1 << 1));   /* RTC alarm off */
    uint32_t temp = rtc_counter_get() + WakeUpDelayInSeconds;       /* get TS then set alarm  */
  
    and_register (RTC, RTC_CTL, ~(1 << 1));   /* Clear alarm flag */
  
    rtc_alarm_config(temp);                      /* set alarm */
  
    or_register (RTC, RTC_INTEN, (1 << 1));   /* RTC alarm on */

    /* set RTC alarm wake up event */

    and_register (EXTI, EXTI_INTEN, ~(1 << 17));
    and_register (EXTI, EXTI_EVEN, ~(1 << 17));
    and_register (EXTI, EXTI_RTEN, ~(1 << 17));
    and_register (EXTI, EXTI_FTEN, ~(1 << 17));
    
    or_register (EXTI, EXTI_EVEN, (1 << 17));
    or_register (EXTI, EXTI_RTEN, (1 << 17));

    
    write_register (EXTI, EXTI_PD, (1 << 17));
}


/**
 * @brief   Clear the wake up timer to prevent it triggering
 * @param   WakeUpDelayInSeconds : wake up delay in seconds
 * @return  none
 */
void RtcClearWakeupTimer (void)
{
    and_register (RTC, RTC_INTEN, ~(1 << 1));   /* RTC alarm off */
    and_register (RTC, RTC_CTL, ~(1 << 1));     /* clear the RTC alarm interrupt flag*/

    or_register (PMU, PMU_CTL, (1 << 2));       /* Reset wakeup flag */
}







/**
 * @brief   RTC interrupt call back handler
 * @param   none
 * @return  none
 */
void RtcFlagCallback (void)
{
    if (read_register (RTC, RTC_CTL) & (1 << 0))
    {
        /* clear the RTC second interrupt flag*/

        and_register (RTC, RTC_CTL, ~(1 << 0));
    }

    if (read_register (RTC, RTC_CTL) & (1 << 1))
    {
        /* clear the RTC alarm interrupt flag*/

        and_register (RTC, RTC_CTL, ~(1 << 1));
    }
}


/**
 * @brief   RTC interrupt call back handler
 * @param   none
 * @return  none
 */
void RtcEXTICallback (void)
{
    if (read_register (EXTI, EXTI_PD) & (1 << 17))
    {
        write_register (EXTI, EXTI_PD, (1 << 17));
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

#define NVIC_AIRCR_VECTKEY_MASK     ((uint32_t)0x05FA0000)
#define NVIC_PRIGROUP_PRE1_SUB3     ((uint32_t)0x600) /*!< 1 bits for pre-emption priority 3 bits for subpriority */

void nvic_irq_enable(uint8_t nvic_irq, uint8_t nvic_irq_pre_priority, 
                     uint8_t nvic_irq_sub_priority)
{
    uint32_t temp_priority = 0x00U, temp_pre = 0x00U, temp_sub = 0x00U;
    volatile unsigned char *d;
    
    temp_pre=1U;
    temp_sub=0x3U;
    
    /* get the temp_priority to fill the NVIC->IP register */
    
    temp_priority = (uint32_t)nvic_irq_pre_priority << (0x4U - temp_pre);
    temp_priority |= nvic_irq_sub_priority &(0x0FU >> (0x4U - temp_sub));
    temp_priority = temp_priority << 0x04U;
    
    d = (volatile unsigned char *) (NVIC + IPR0);
    
    d += nvic_irq;
    
    *d = (uint8_t) temp_priority;
    
    /* enable the selected IRQ */
    
    write_register (NVIC, ISER0 + ((nvic_irq >> 0x05U) << 2), (uint32_t)0x01U << (nvic_irq & (uint8_t)0x1FU));
}

static void nvic_Rtc_configuration (void)
{
    
    write_register (SCB, SCB_AIRCR, NVIC_AIRCR_VECTKEY_MASK | NVIC_PRIGROUP_PRE1_SUB3);

    nvic_irq_enable(3, 6, 0);       /* RTC_irq */

}


unsigned long RtcBkupReadRegister (unsigned long reg)
{
    return read_register (BKP, reg);
}

void RtcBkupWriteRegister (unsigned long reg, unsigned long val)
{
    write_register (BKP, reg, val);
}
