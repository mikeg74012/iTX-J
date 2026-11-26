/**
 **************************************************************************************************
 * @file   main.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   30/10/2019
 *
 * @brief  Main software application for the iTelematiX V4 Vehicle Telematics Device.  This version
 *         is for GigaDevice GD32F303RE MCU.
 *
 **************************************************************************************************
 */
#include "global.h"


/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */
//#define VECT_TAB_OFFSET 0x10000
#define VECT_TAB_OFFSET 0x0

#define RCU_MODIFY      {volatile uint32_t i; \
                         or_register (RCU, RCU_CFG0, (1 << 7)); \
                         for(i=0;i<20000;i++);}

/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */


/*
 **************************************************************************************************
 *                                         PUBLIC SECTION
 **************************************************************************************************
 */

unsigned long console_stack[CONSOLE_STACK_SIZE / 4];
unsigned long button_monitor_stack[BUTTON_MONITOR_STACK_SIZE / 4];
unsigned long battery_monitor_stack[BATTERY_MONITOR_STACK_SIZE / 4];
unsigned long motion_monitor_stack[MOTION_MONITOR_STACK_SIZE / 4];
unsigned long sequencer_stack[SEQUENCER_STACK_SIZE / 4];
unsigned long modem_stack[MODEM_STACK_SIZE / 4];
unsigned long gsm_comms_stack[GSM_COMMS_STACK_SIZE / 4];
unsigned long gnss_comms_stack[GNSS_COMMS_STACK_SIZE / 4];
unsigned long wifi_stack[WIFI_STACK_SIZE / 4];


volatile unsigned long ms_timer;

const unsigned long gpio_bases[] = {GPIOA, GPIOB, GPIOC, GPIOD, GPIOE,
                                    GPIOF, GPIOG};



void set_gpio_alternate(unsigned char val)
{
	unsigned long *d;
    unsigned long state;

	d = (unsigned long *) gpio_bases[val >> 4];

	/* Select mode CTL 2 and MD 1 for AFIO push pull output */

    
    state = cpu_is_irq_enabled();
    __disable_interrupts();
    
    val = val & 0xf;

    if (val & 0x8)
    {
        d[GPIO_CTL1 >> 2] &= ~(0xf << ((val & 0x7) << 2));
        d[GPIO_CTL1 >> 2] |= (9 << ((val & 0x7) << 2));
    }
    else
    {
        d[GPIO_CTL0 >> 2] &= ~(0xf << ((val & 0x7) << 2));
        d[GPIO_CTL0 >> 2] |= (9 << ((val & 0x7) << 2));
    }

    if (state)
    {
        __enable_interrupts();
    }
}

void	set_gpio_speed (unsigned char val, unsigned char mode)
{
}

void set_gpio_alt_map(unsigned char val, unsigned char mode)
{
}

void	set_gpio_analog (unsigned char val)
{
	unsigned long *d;

	d = (unsigned long *) gpio_bases[val >> 4];

	/* Select mode CTL 0 and MD 0 for analog input */

    val = val & 0xf;

    if (val & 0x8)
    {
        d[GPIO_CTL1 >> 2] &= ~(0xf << ((val & 0x7) << 2));
    }
    else
    {
        d[GPIO_CTL0 >> 2] &= ~(0xf << ((val & 0x7) << 2));
    }
}

void	set_gpio_output (unsigned char val)
{
	unsigned long *d;
    unsigned long state;

	d = (unsigned long *) gpio_bases[val >> 4];

	/* Select mode CTL 0 and MD 3 for push pull output */

    state = cpu_is_irq_enabled();
    __disable_interrupts();
 
    val = val & 0xf;

    if (val & 0x8)
    {
        d[GPIO_CTL1 >> 2] &= ~(0xf << ((val & 0x7) << 2));
        d[GPIO_CTL1 >> 2] |= (3 << ((val & 0x7) << 2));
    }
    else
    {
        d[GPIO_CTL0 >> 2] &= ~(0xf << ((val & 0x7) << 2));
        d[GPIO_CTL0 >> 2] |= (3 << ((val & 0x7) << 2));
    }

    if (state)
    {
        __enable_interrupts();
    }

}

void	set_gpio_open_drain (unsigned char val)
{
	unsigned long *d;
    unsigned long state;

	d = (unsigned long *) gpio_bases[val >> 4];

	/* Select mode CTL 0 and MD 3 for push pull output */

    state = cpu_is_irq_enabled();
    __disable_interrupts();
 
    val = val & 0xf;

    if (val & 0x8)
    {
        d[GPIO_CTL1 >> 2] &= ~(0xf << ((val & 0x7) << 2));
        d[GPIO_CTL1 >> 2] |= (7 << ((val & 0x7) << 2));
    }
    else
    {
        d[GPIO_CTL0 >> 2] &= ~(0xf << ((val & 0x7) << 2));
        d[GPIO_CTL0 >> 2] |= (7 << ((val & 0x7) << 2));
    }

    if (state)
    {
        __enable_interrupts();
    }

}

void	set_gpio_enable_pullup (unsigned char val)
{
	unsigned long *d;
    unsigned long state;
    
    set_gpio_high (val);

	d = (unsigned long *) gpio_bases[val >> 4];

    state = cpu_is_irq_enabled();
    __disable_interrupts();
 
    val = val & 0xf;

    if (val & 0x8)
    {
        d[GPIO_CTL1 >> 2] &= ~(0xf << ((val & 0x7) << 2));
        d[GPIO_CTL1 >> 2] |= (8 << ((val & 0x7) << 2));
    }
    else
    {
        d[GPIO_CTL0 >> 2] &= ~(0xf << ((val & 0x7) << 2));
        d[GPIO_CTL0 >> 2] |= (8 << ((val & 0x7) << 2));
    }

    if (state)
    {
        __enable_interrupts();
    }
}

void	set_gpio_enable_pulldown (unsigned char val)
{
	unsigned long *d;
    unsigned long state;
    
    set_gpio_low (val);

	d = (unsigned long *) gpio_bases[val >> 4];

    state = cpu_is_irq_enabled();
    __disable_interrupts();
 
    val = val & 0xf;

    if (val & 0x8)
    {
        d[GPIO_CTL1 >> 2] &= ~(0xf << ((val & 0x7) << 2));
        d[GPIO_CTL1 >> 2] |= (8 << ((val & 0x7) << 2));
    }
    else
    {
        d[GPIO_CTL0 >> 2] &= ~(0xf << ((val & 0x7) << 2));
        d[GPIO_CTL0 >> 2] |= (8 << ((val & 0x7) << 2));
    }

    if (state)
    {
        __enable_interrupts();
    }
}

void	set_gpio_high_speed (unsigned char val)
{
    /* All outputs are high speed by default */
    
}

void	set_gpio_disable_pull (unsigned char val)
{
	unsigned long *d;
    unsigned long state;
    
    set_gpio_high (val);

	d = (unsigned long *) gpio_bases[val >> 4];

    state = cpu_is_irq_enabled();
    __disable_interrupts();
 
    val = val & 0xf;

    if (val & 0x8)
    {
        d[GPIO_CTL1 >> 2] &= ~(0xf << ((val & 0x7) << 2));
        d[GPIO_CTL1 >> 2] |= (4 << ((val & 0x7) << 2));
    }
    else
    {
        d[GPIO_CTL0 >> 2] &= ~(0xf << ((val & 0x7) << 2));
        d[GPIO_CTL0 >> 2] |= (4 << ((val & 0x7) << 2));
    }

    if (state)
    {
        __enable_interrupts();
    }
}

void	set_gpio_pull_down (unsigned char val)
{
}

void	set_gpio_no_pull (unsigned char val)
{
}

void	set_gpio_input (unsigned char val)
{
	unsigned long *d;
    unsigned long state;

	d = (unsigned long *) gpio_bases[val >> 4];

	/* Select mode CTL 1 and MD 0 for push pull output */

    state = cpu_is_irq_enabled();
    __disable_interrupts();
 
    val = val & 0xf;

    if (val & 0x8)
    {
        d[GPIO_CTL1 >> 2] &= ~(0xf << ((val & 0x7) << 2));
        d[GPIO_CTL1 >> 2] |= (4 << ((val & 0x7) << 2));
    }
    else
    {
        d[GPIO_CTL0 >> 2] &= ~(0xf << ((val & 0x7) << 2));
        d[GPIO_CTL0 >> 2] |= (4 << ((val & 0x7) << 2));
    }

    if (state)
    {
        __enable_interrupts();
    }

}


void	set_gpio_high (unsigned char val)
{
	unsigned long *d;

	d = (unsigned long *) gpio_bases[val >> 4];

	d[GPIO_BOP >> 2] = ((unsigned long) 1) << (val & 0xf);
}

void	set_gpio_low (unsigned char val)
{
	unsigned long *d;

	d = (unsigned long *) gpio_bases[val >> 4];

	d[GPIO_BC >> 2] = ((unsigned long) 1) << (val & 0xf);
}

unsigned long read_gpio (unsigned char val)
{
	unsigned long *d;

	d = (unsigned long *) gpio_bases[val >> 4];

	return (unsigned long) (d[GPIO_ISTAT >> 2] & (1 << (val & 0xf)));
}

unsigned long read_gpio_output (unsigned char val)
{
	unsigned long *d;

	d = (unsigned long *) gpio_bases[val >> 4];

	return (unsigned long) (d[GPIO_OCTL >> 2] & (1 << (val & 0xf)));
}

static void system_clock_config(void)
{
    uint32_t timeout = 0U;
    uint32_t stab_flag = 0U;
    
    /* enable IRC8M */
    
    or_register (RCU, RCU_CTL, (1 << 0));

    /* wait until IRC8M is stable or the startup time is longer than IRC8M_STARTUP_TIMEOUT */
    do{
        timeout++;
        stab_flag = read_register (RCU, RCU_CTL) & (1 << 1);
    }
    while(0U == stab_flag);

    /* LDO output voltage high mode */
    or_register (RCU, RCU_APB1EN, (1 << 28));
    or_register (PMU, PMU_CTL, (3 << 14));

    /* IRC8M is stable */
    /* AHB = SYSCLK */
    //or_register (RCU, RCU_CFG0, RCU_AHB_CKSYS_DIV1);
    /* APB2 = AHB/1 */
    //or_register (RCU, RCU_CFG0, RCU_APB2_CKAHB_DIV1);
    /* APB1 = AHB/2 */
    or_register (RCU, RCU_CFG0, (4 << 8));

    /* CK_PLL = (CK_IRC8M/2) * 12 = 48 MHz */
    and_register (RCU, RCU_CFG0, ~((0xf << 18) | (1 << 27) | (1 << 30)));
    or_register (RCU, RCU_CFG0, (10 << 18));

    /* enable PLL */
    or_register (RCU, RCU_CTL, (1 << 24));

    /* wait until PLL is stable */
    while(0U == (read_register (RCU, RCU_CTL) & (1 << 25)))
    {
    }
    
    /* enable the high-drive to extend the clock frequency to 120 MHz */
    or_register (PMU, PMU_CTL, (1 << 16));
    
    while(0U == (read_register (PMU, PMU_CS) &  (1 << 16)))
    {
    }
    
    /* select the high-drive mode */
    or_register (PMU, PMU_CTL, (1 << 17));
    
    while(0U == (read_register (PMU, PMU_CS) &  (1 << 17)))
    {
    }
    
    /* select PLL as system clock */
    and_register (RCU, RCU_CFG0, ~(3 << 0));
    or_register (RCU, RCU_CFG0, 2);

    /* wait until PLL is selected as system clock */
    while(0U == (read_register (RCU, RCU_CFG0) & (2 << 2)))
    {
    }
}

void SystemInit (void)
{
    /* reset the RCU clock configuration to the default reset state */
    /* Set IRC8MEN bit */
    or_register (RCU, RCU_CTL, (1 << 0));

    RCU_MODIFY
    
    and_register (RCU, RCU_CFG0, ~(3 << 0));

    /* reset HXTALEN, CKMEN and PLLEN bits */
    and_register (RCU, RCU_CTL, ~((1 << 24) | (1 << 19) | (1 << 16)));
    
    /* disable all interrupts */
    write_register (RCU, RCU_INT, 0x009f0000U);

    /* reset HXTALBPS bit */
    
    and_register (RCU, RCU_CTL, ~(1 << 18));
    
    /* Reset CFG0 and CFG1 registers */
    write_register (RCU, RCU_CFG0, 0x00000000U);
    write_register (RCU, RCU_CFG1, 0x00000000U);

    /* configure the system clock source, PLL Multiplier, AHB/APBx prescalers and Flash settings */
    system_clock_config();
}


/**
 * @brief  The application entry point.
 * @param  none
 * @retval int (not used)
 */
int main (void)
{
  uartconfig_t    UartConfig;
  stream_t *      pStream;
  
  /* Check if the system has resumed from WWDG reset */
  eResetCause_t ResetCause = PowerGetResetCause();
  if ((ResetCause == RESET_WATCHDOGWIN) || (ResetCause == RESET_WATCHDOGIDG))
  {
    watchdogResetDetected = true;
  }

  /* Configure the system clock */
  SystemClockInit();

  /* Enable the Free Hardware watch Dog */
  SystemFreeWatchDogEnable();

  /* configure the systick handler priority */
  
// Should already be 0   NVIC_SetPriority(SysTick_IRQn, 0x00U);

  /* initialize the watchdog module */
  WatchdogInitialize();

  ms_timer = 0;
  
  /* initialise the real time clock */
  RtcInit();

  /* ied Init after RTC is ready */
  LedInit();

  if (ResetCause == RESET_UNKNOWN)
  {
    /* If still have an unknown reset cause, Read the RTC bkup register and reset afterward */
    ResetCause = PowerGetResetCauseFromRTCBkupRegister();
  }

  if ((ResetCause == RESET_HIBERNATE) || (ResetCause == RESET_FW_UPDATE))
  {
    /* reset the Standby sleep flag for a normal boot */
    ResetLPSleepStatus(RTC_BKUP_STANDBYSLEEP_FLAG);
  }

  /* Decrement the count of test mode */
  TestModeCountDown();

  /* Initialise the Console */
  UartConfig.BaudRate = 460800;
  UartConfig.DataBits = COM_DATABITS_8;
  UartConfig.StopBits = COM_STOPBITS_1;
  UartConfig.Parity   = COM_PARITY_NONE;
  UartConfig.FlowControl = COM_FLOWCONTROL_NONE;
  pStream = UARTInitialise(CONSOLE_UART, &UartConfig);
  
  /* Initialise the serial SRAM */
  SerialRamInit();

  /* SPI Ram Ready, restore the system data if it is boot from low power sleep */
  if (IsLowPowerSleepFlagUp())
  {
    ReadResetDataFromSpiRAM();  /* first read PSRAM after RAM SPI when it ready */

    /* Restore the Fitment related data*/
    FitmentDataHandler(MOUNT_SURVEY_IMPORT);
  }

  /* Init the fitment matrix */
  FitmentMatrixInit();

  /* Initialise status log and LED */
  StatusLogInitialize();

  /* set the exception vector table location */
  
    write_register (SCB, SCB_VTOR, 0x8000000 | VECT_TAB_OFFSET);
    __DSB();

  set_irq_priority (41, 6);
  enable_irq (41);          /* Enable RTC alarm interrupt */
  
  or_register (RCU, RCU_AHBEN, (1 << 6));       /* enable CRC clock */
  or_register (RCU, RCU_APB1EN, (1 << 28));     /* enable PMU clock */
  or_register (RCU, RCU_APB2EN, (1 << 0));      /* alternate function clock */

  or_register (PMU, PMU_CTL, (1 << 2));         /* clear wake up flag*/
  and_register (RTC, RTC_CTL, ~(1 << 1));       /* clear the RTC second interrupt flag*/

  /* Clear the Reset flag */
  if (ResetCause == RESET_LOW_BATTERY)
  {
    ConfigResetLowBatFlag();
  }

  /* On first boot, we initialise the configuration data store module */
  if (ConfigCheckValidData() != true)
  {
    /* Set some sensible defaults */
    ConfigSetDefaults();
  }

  /* Reset the customer ID to zero if boot from hibernate */
  if (ResetCause == RESET_HIBERNATE)
  {
    ConfigSetCustomer(0);
  }

  /* Initialise the device statistics */
  StatisticsInitialize();

  /* Initialise the GPRS Message Queue */
  MessageQueueInitialise();

  /* Carry out WIFI Wake-Up Self-Test if required */
  PostWakeUpSelfTest(WIFI);

  /* Initialise the I2C bus before the threads start */
  I2cInitialise(I2C_BUS_0, 400000);

  /* hardware version detection */
  HardwareVersionCheck();

  /* Reload the watchdog before the task creation */
  SystemFreeWatchDogReload();

  /*
   * Create the threads
   */

  init_threads ();
  
  /* Button Monitor Thread */
  add_thread ((void (*) (void)) ButtonMonitorTask, &button_monitor_stack[(sizeof (BUTTON_MONITOR_STACK_SIZE) / 4) - 1]);
  
  /* Battery Monitor Thread */
  add_thread ((void (*) (void)) BatteryMonitorTask, &battery_monitor_stack[(sizeof (BATTERY_MONITOR_STACK_SIZE) / 4) - 1]);

  /* Motion Monitor Thread */
  add_thread ((void (*) (void)) MotionMonitorTask, &motion_monitor_stack[(sizeof (MOTION_MONITOR_STACK_SIZE) / 4) - 1]);

  /* Sequencer Thread */
  add_thread ((void (*) (void)) SequencerTask, &sequencer_stack[(sizeof (SEQUENCER_STACK_SIZE) / 4) - 1]);

  /* GSM / GNSS Modem Threads */
  add_thread ((void (*) (void)) ModemTask, &modem_stack[(sizeof (MODEM_STACK_SIZE) / 4) - 1]);

  add_thread ((void (*) (void)) GsmCommsTask, &gsm_comms_stack[(sizeof (GSM_COMMS_STACK_SIZE) / 4) - 1]);

#if MODEM_SEPARATE_GNSS_UART
  add_thread ((void (*) (void)) GnssCommsTask, &gnss_comms_stack[(sizeof (GNSS_COMMS_STACK_SIZE) / 4) - 1]);
#endif

  /* WiFi Thread */

  add_thread ((void (*) (void)) WifiTask, &wifi_stack[(sizeof (WIFI_STACK_SIZE) / 4) - 1]);
  
  while (1)
  {
    sleep (0);
  }
}


/**
 * @brief  System Clock Configuration
 * @note   Called during system startup AND each time the device wakes from sleep
 * @retval None
 */
void SystemClockInit (void)
{
 /* GD will call the SystemInit() from the start up .S code, it include in system_gd32f30x.c file
 * it will setup the microcontroller system, initialize the system clock
 * inside the file, system clock is selected by the macro define in IRC8M section, From 8M to 120M from the PLL
 * __SYSTEM_CLOCK_48M_PLL_IRC8M is selected
 */
  SystemInit(); /* setup system clock */
}


/**
 * @brief  This function is executed in case of error occurrence.
 * @param  None
 * @retval None
 */
void Error_Handler (void)
{
  /* User can add his own implementation to report the HAL error return state */
}


/*
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed (char *file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
  tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
}
#endif /* USE_FULL_ASSERT */


void SysTick_Handler( void )
{
    ms_timer++;
}
