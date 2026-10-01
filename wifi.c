/**
 **************************************************************************************************
 * @file   wifi.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief   Wifi Driver source file
 *
 **************************************************************************************************
 */
#include "global.h"


/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */
/* Debug trace for GNSS Comms */

#define TRACE_WIFI(...)

/*
#if TRACE_WIFI_ENABLE
  #define TRACE_WIFI(...) TRACE_COLOUR_(FG_CYAN, BG_DEFAULT, "WIFI", __VA_ARGS__)
#else
  #define TRACE_WIFI(...)
#endif
*/

/* ASCII control characters use when decoding AT responses from the modem */
#define CARRIAGE_RETURN           13
#define NEW_LINE                  10

/* max buffer size for receiving WIFI data from the modem */
#define WIFI_BUF_SIZE             80

/* Boot time for the wifi program */
#define WIFI_MEM_BOOT_TIME        280 

/*
 **************************************************************************************************
 *                                   PRIVATE FUNCTION PROTOTYPES
 **************************************************************************************************
 */
static void WifiInitGpio         (void);
static void WifiInitWifiUart     (void);
static void WifiPowerUp          (void);
static void WifiPowerDown        (void);
static void WifiProcessData      (char inChar);
static bool WifiDecodeScanResult (char * pMessage);


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
/* UART access */
static stream_t     * pWifiStream;

/* buffers for storage of MAC address data from the WiFi chip */
static char       WifiDataBuf[WIFI_BUF_SIZE];
static WifiMac_t  ScanResults[WIFI_NO_OF_RESULTS];
static bool       isWifiEnabled = false;


/*
 **************************************************************************************************
 *                                   PUBLIC SECTION / INTERFACE
 **************************************************************************************************
 */

/**
 * @brief      Top level WiFi Task.
 * @param      None
 * @return     None
 */
void WifiTask (void const * argument)
{
  uint8_t RxData;

  /* initialise the GPIO pins */
  WifiInitGpio();

  /* Initialise the UART */
  WifiInitWifiUart();

  /* For now, we keep the WiFi module Powered Down */
  WifiPowerDown();

  /* Main thread loop */
  for (;;)
  {
    /*
     * Receive next character from the serial port
     * NOTE: this waits for next character to be received
     */
    RxData = pWifiStream->Get();
    WifiProcessData((char)RxData);
  }
}


/**
 * @brief   Initial programming of WiFi chip. Put the WiFi module into programming mode, then open a tunnel to the console port
 * @param   None
 * @retval  None
 * @warning The board will require a full reset to exit from tunnel mode
 */
void WifiProgram    (void)
{
  /* Make sure the module is powered down */
  WifiPowerDown();

  /* Pull WIFI prog line low to make sure we enter programming mode */
  
  set_gpio_low (WIFI_PROG);
  
  sleep(1000);

  /* Power up the WIFI module */
  WifiPowerUp();

  /* Release WIFI prog line  */
  sleep(1000);
  
  set_gpio_high (WIFI_PROG);
  
}

/**
 * @brief   Start the WiFi chip to output sniffed MAC addresses
 * @param   None
 * @retval  None
 */
void WifiStart (void)
{
  TRACE_WIFI("Start WiFi scanning\n\r");
  
  /* clear scan results */
  memset(ScanResults, 0, sizeof(ScanResults));

  /* Make sure the module is powered down */
  WifiPowerDown();

  /* Release WIFI prog line to make sure we boot from FLASH */
  
  set_gpio_high (WIFI_PROG);
  
  sleep(1000);

  /* Power up the WIFI module */
  WifiPowerUp();
}


/**
 * @brief   Stop the WiFi chip and put it into a low power state
 * @param   None
 * @retval  None
 */
void WifiStop (void)
{
  TRACE_WIFI("Stop WiFi scanning\n\r");
  /* Make sure the module is powered down */
  WifiPowerDown();
}


/**
 * @brief     Abort current activity and return to idle state
 * @param     None
 * @return    None
 */
void WifiAbort (void)
{
  if (isWifiEnabled)
  {
    WifiStop();
  }
}


/**
 * @brief   Get a scan result
 * @param   pScanResult   : pointer to storage for the result
 * @retval  bool          : true on success, false on failure
 */
bool WifiGetScanResult (WifiMac_t * pScanResult, int index)
{
  if (index < WIFI_NO_OF_RESULTS)
  {
    memcpy(pScanResult, &ScanResults[index], sizeof(WifiMac_t));
    memset(&ScanResults[index], 0, sizeof(WifiMac_t));
    return true;
  }
  return false;
}


/**
 * @brief     Wifi chip fix for CE of Memory 
 *            Ticket its110, ensure the CS of MX25V1635F at the High state
 *            such that it will always end at disabled state
 *            Wifi Chips boot times need WIFI_MEM_BOOT_TIME ms
 * @param     None
 * @return    true on success, false on faliure
 */
bool WifiMemCEFix (void)
{
  int hwVersion = GetHardwareVersion();
  
  if (hwVersion >= HW_PCB_VERSION_A && hwVersion <= HW_PCB_VERSION_F)
  {
    /* Power up the WIFI module, it already had 50ms in the funcitions */

    set_gpio_high (WIFI_ENABLE);
    
    sleep(WIFI_MEM_BOOT_TIME);
    
    set_gpio_low (WIFI_ENABLE);
  }
  else
  {
    TRACE_WIFI("No WiFi patch needed\r\n");
  }

  return true;
}


/**
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */

/**
  * @brief  Process incoming data from the WiFi module
  * @param  inChar
  * @retval None
  */
static void WifiProcessData (char inChar)
{
  static int bufIndex = 0;

  /* add character to the receive buffer if there is room */
  if (bufIndex < WIFI_BUF_SIZE)
  {
    WifiDataBuf[bufIndex++] = inChar;
  }

  /* if it was a carriage return, decode the string */
  if (CARRIAGE_RETURN == inChar)
  {
    if (bufIndex < WIFI_BUF_SIZE)
    {
      WifiDataBuf[bufIndex++] = 0;
      TRACE_WIFI(WifiDataBuf);
      WifiDecodeScanResult(WifiDataBuf);
    }
    bufIndex = 0;
  }
}


/**
 * @brief  Initialize the GPIO pins used to control the Wifi modem
 * @param  None
 * @retval None
 */
static void WifiInitGpio (void)
{
  /* Ensure the WiFi Module is disabled when first init */
  
  set_gpio_low (WIFI_ENABLE);
  set_gpio_output (WIFI_ENABLE);
  
  set_gpio_high (WIFI_RESET);
  set_gpio_high (WIFI_PROG);

  set_gpio_open_drain (WIFI_RESET);
  set_gpio_open_drain (WIFI_PROG);
    
}




/**
 * @brief  Initialise the WiFi UART
 *         NOTE: we only use the Rx Pin.  We leave the Tx floating so the device can be programmed
 *               by an external programmer.  See stm32fxx_hal_msp.c for Uart GPIO pin setup
 * @param  None
 * @retval None
 */
static void WifiInitWifiUart (void)
{
  uartconfig_t UartConfig;

  UartConfig.BaudRate = 115200;
  UartConfig.DataBits = COM_DATABITS_8;
  UartConfig.StopBits = COM_STOPBITS_1;
  UartConfig.Parity   = COM_PARITY_NONE;
  UartConfig.FlowControl = COM_FLOWCONTROL_NONE;
  pWifiStream = UARTInitialise(WIFI_UART, &UartConfig);
}


/**
 * @brief  Power up the Wifi Modem
 * @param  None
 * @retval None
 */
static void WifiPowerUp (void)
{
  /* Enable the wifi module */
  isWifiEnabled = true;
  
  set_gpio_high (WIFI_ENABLE);
  
  sleep(50);
}


/**
 * @brief  Power down the Wifi Modem
 * @param  None
 * @retval None
 */
static void WifiPowerDown (void)
{
  /* Disable the wifi module */
  isWifiEnabled = false;
  
  set_gpio_low (WIFI_ENABLE);

  sleep(50);
}


/**
 * @brief   Decode a scan result
 * @param   scanMessage   : The scan message from the wifi modem
 * @retval  bool          : true on success, false on failure
 */
static bool WifiDecodeScanResult (char * pMessage)
{
  int   tmpRssi;
  int   Channel;
  char  MacString[18];
  char  MacAddress[13] ={0};
  int   index;
  int   winnerIdx = -1, lastIdx = -1;
  uint8_t Rssi;

  /* find the RSSI and MAC */
  if (sscanf(pMessage,"RSSI: %d Ch: %d MAC: %17s", &tmpRssi, &Channel, MacString) != 3)
  {
    return(false);
  }
  /* signal strength -dBm = Rssi Byte-256, large RSSI  stronger signal */


  /* convert the MAC into our internal format (remove the colons) */
  MacAddress[0]  = MacString[0];
  MacAddress[1]  = MacString[1];
  MacAddress[2]  = MacString[3];
  MacAddress[3]  = MacString[4];
  MacAddress[4]  = MacString[6];
  MacAddress[5]  = MacString[7];
  MacAddress[6]  = MacString[9];
  MacAddress[7]  = MacString[10];
  MacAddress[8]  = MacString[12];
  MacAddress[9]  = MacString[13];
  MacAddress[10] = MacString[15];
  MacAddress[11] = MacString[16];
  MacAddress[12] = 0;

  /* Cast the Rssi to the uint8_t for ease of comparison within the Scan Result, as if BLE */
  Rssi = (uint8_t) tmpRssi;

  /* sorting process to pick 5 strongest one */  
  /* Get the knowledge of the existing list first */ 
  for (int i = 0; i < WIFI_NO_OF_RESULTS; i++) 
  {
    if (ScanResults[i].Rssi != 0) 
    {
      lastIdx = i;
    }
    else 
    {
      break;
    }
  }

  /* check if we already have this MAC stored in our results list */
  for (index = 0; index <= lastIdx; index++)
  {
    if (0 == memcmp(ScanResults[index].MacAddress, MacAddress, 12))
    {
      /* we already have it */
      if (Rssi > ScanResults[index].Rssi)  /* Is it stronger this time */
      {
        /* Let's destroy the duplicate weak record, and move others up, and then insert the new stronger record later */
        for (uint8_t i = index; i < lastIdx; i++)
        {
          ScanResults[i] = ScanResults[i+1];
        }
        /* Clear the last one (destroyed one) */
        ScanResults[lastIdx].Rssi = 0;
        memset(ScanResults[lastIdx].MacAddress, 0, sizeof(ScanResults[lastIdx].MacAddress));
        lastIdx--;
        break;
      }
      else
      {
        return true;    /* It is not stronger, nothing to do */
      }
    }
  }

  /* filter large value (strong signal) in order, loop the whole list, such that it will go into the list if there is empty slot */
  for (index = 0; index < WIFI_NO_OF_RESULTS; index++)
  {
    if (Rssi > ScanResults[index].Rssi)
    {
      winnerIdx = index;
      break;
    }
  }

  if (winnerIdx == -1) return true;   /* This candidate cannot win */

  /* ConsolePrintf ("Wifi sort: [%d][%d] -> [%d][%d][%d][%d][%d] \r\n", winnerIdx, Rssi, ScanResults[0].Rssi, ScanResults[1].Rssi, ScanResults[2].Rssi , ScanResults[3].Rssi ,ScanResults[4].Rssi); */

  /* try to shift the weakest out, and squeeze the winner in */
  for (uint8_t i = (WIFI_NO_OF_RESULTS -1); i > winnerIdx; i--)
  {
    ScanResults[i] = ScanResults[i - 1];
  }

  /* We got a new winner, copy it */
  ScanResults[winnerIdx].Rssi =  Rssi;  /* -dBm = byte-256 */
  memcpy(ScanResults[winnerIdx].MacAddress, MacAddress, 12);
  ScanResults[winnerIdx].MacAddress[12] = 0;  /* NULL terminator */ 
  
  /* ConsolePrintf ("--> Wifi saved: index:%d,[%s][%d]\r\n", winnerIdx, ScanResults[winnerIdx].MacAddress, ScanResults[winnerIdx].Rssi); */  
  return true;
}


/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       Put WiFi module into programming mode
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int WifiConsoleProg (int argCount, char **pArguments)
{
  WifiProgram();

  /* indicate success */
  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       Start the WiFi module / start logging MAC addresses
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int WifiConsoleStart (int argCount, char **pArguments)
{
  WifiStart();

  /* indicate success */
  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       Stop the WiFi module / stop logging MAC addresses
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int WifiConsoleStop (int argCount, char **pArguments)
{
  WifiStop();

  /* indicate success */
  return (CONSOLE_ERROR_OK);
}

/**
 * @brief       Console to trigger Wifi Mem CE Pin patch
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int WifiConsoleFix (int argCount, char **pArguments)
{
  uint16_t failed = 0;
  TRACE_WIFI("WiFi Mem CE Fix Test\n\r");
  for (uint32_t i =0; i < 50000; i++)
  {
    WifiMemCEFix();
    /* Take time to average out the WiFi On Current */

    sleep(4000);
    WatchdogSetActive(CONSOLETASK);
    int16_t current = BatteryMonitorGetCurrent();
    
    if (current <= -11)
    {
      uint32_t j = 0;
      int last_current = current;
      ConsolePrintf("struggiling with current %d \r\n",current);
      /* Give time for the current average outs*/
      for (j =0; j < 2000; j++)
      {
        sleep(100);
        current = BatteryMonitorGetCurrent();
        if (current != last_current)
        {
          ConsolePrintf("updated with current %d \r\n",current);
          last_current = current;
        }
        if (current > -11)
        {
            /* okay, we are still good, carry on the test */
            ConsolePrintf("Still good, carry on with current %d mA\r\n", current);
            break;
        }
        WatchdogSetActive(CONSOLETASK);
      }

      if (j == 2000)
      {
        failed ++;
        ConsolePrintf("Patch Not work with %d mA (%d/%d) failed/tested\r\n", current, failed, i);
        if (failed > 5)
        {
          /* Failed case meet, the patch not work stop the testing */
          break;
        }
      }
    }
    if ((i%40) == 0)
    {
      ConsolePrintf("Patch tested with %d mA (%d/%d) failed/tested\r\n", current, failed, i);
    }
    /* This test take long, keep the watchdog happy */
    WatchdogSetActive(CONSOLETASK);
  }

  /* indicate success */
  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       List the WiFi module MAC addresses and RSSI
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int WifiConsoleList (int argCount, char **pArguments)
{
  if (3 == argCount)
  {
    /* Sample string from Modem "RSSI: -77 Ch: 6 MAC: 00:26:ab:14:e5:d9" */
    if (!strcmp(pArguments[0], "add"))
    {
      /* Glue the pArguments back as if what return from the Modem */
      char combined[256];
      snprintf(combined, sizeof(combined), "RSSI: %s Ch: 6 MAC: %s", pArguments[1], pArguments[2]);
      WifiDecodeScanResult(combined);
    }
  }

  for (int index = 0; index < WIFI_NO_OF_RESULTS; index++)
  {
    ConsolePrintf("Wifi saved:[%d] %s, RSSI = %d\r\n", index, ScanResults[index].MacAddress, (ScanResults[index].Rssi-256));
  }
  sleep(1);
  /* indicate success */
  return (CONSOLE_ERROR_OK);
}
