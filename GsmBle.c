/**
 **************************************************************************************************
 * @file   GsmBle.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  BLE scanning module (no other Bluetooth functions)
 *
 **************************************************************************************************
 */
#include "global.h"


/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */
#define BLUETOOTH_AT_TIMEOUT_MS    3000
#define BLE_SCAN_DURATION_SECS     10

/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
static BleMac_t ScanResults[BLE_NO_OF_RESULTS];

/*
 **************************************************************************************************
 *                                   PRIVATE FUNCTION PROTOTYPES
 **************************************************************************************************
 */
static bool GsmBleStartScanning (void);
static bool GsmBleStopScanning  (void);
static bool GsmBleListScanning  (void);

/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */

/**
 * @brief   Perform a scan for Bluetooth devices
 * @param   none
 * @retval  bool          : true on success, false on failure
 */
bool GsmBleScan (void)
{
  /* clear scan results */
  memset(ScanResults, 0, sizeof(ScanResults));

  /* scan for fixed duration */
  GsmBleStartScanning();
  for (uint8_t i =0; i < BLE_SCAN_DURATION_SECS; i ++ )
  {
    if (isModemAbort())
    {
      break;
    }
    sleep(ONE_SECOND_IN_MS);
  }
  GsmBleStopScanning();

  return true;
}


/**
 * @brief   Stop the BLE scan for Bluetooth devices
 * @param   none
 * @retval  bool          : true on success, false on failure
 */
bool GsmStopBleScan (void)
{
  GsmBleStopScanning();
  return true;
}


/**
 * @brief   Decode a scan result
 * @param   scanMessage   : The AT scan message from the modem
 * @retval  bool          : true on success, false on failure
 */
bool GsmBleDecodeScanResult (char * scanMessage)
{
  int   Rssi;
  char  Mac[13] = {0};
  int   index;
  int   winnerIdx = -1, lastIdx = -1;

  if (sscanf(scanMessage, "+QBTGATCSCAN: \"B001\",%12s,%d", Mac, &Rssi) != 2)
  {
    return false;
  }
  /* signal strength -dBm = Rssi Byte-127, large RSSI  stronger signal */  

  /* Get the knowledge of the existing list first */ 
  for (int i = 0; i < BLE_NO_OF_RESULTS; i++) 
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

  /* sorting process to pick 5 strongest one */

  /* check if we already have this MAC stored in our results list */
  for (index = 0; index <= lastIdx; index++)
  {
    if (0 == memcmp(ScanResults[index].MacAddress, Mac, 12))
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
  for (index = 0; index < BLE_NO_OF_RESULTS; index++)
  {
    if (Rssi > ScanResults[index].Rssi)
    {
      winnerIdx = index;
      break;
    }
  }

  if (winnerIdx == -1) return true;   /* This candidate cannot win */
  /* ConsolePrintf ("BLE sort: [%d][%d] -> [%d][%d][%d][%d][%d] \r\n", winnerIdx, Rssi, ScanResults[0].Rssi, ScanResults[1].Rssi, ScanResults[2].Rssi , ScanResults[3].Rssi ,ScanResults[4].Rssi); */

  /* try to shift the weakest out, and squeeze the winner in */
  for (uint8_t i = (BLE_NO_OF_RESULTS -1); i > winnerIdx; i--)
  {
    ScanResults[i] = ScanResults[i - 1];
  }

  /* We got a new winner, copy it */
  ScanResults[winnerIdx].Rssi =  Rssi;  /* -dBm = byte-127 */
  memcpy(ScanResults[winnerIdx].MacAddress, Mac, 12);
  ScanResults[winnerIdx].MacAddress[12] = 0;  /* NULL terminator */ 

  /* ConsolePrintf ("--> BLE saved: index:%d,[%s][%d]\r\n", winnerIdx, ScanResults[winnerIdx].MacAddress, ScanResults[winnerIdx].Rssi); */
  return true;
}


/**
 * @brief   Get a scan result
 * @param   scanMessage   : The AT scan message from the modem
 * @retval  bool          : true on success, false on failure
 */
bool GsmBleGetScanResult (BleMac_t * pScanResult, int index)
{
  if (index < BLE_NO_OF_RESULTS)
  {
    memcpy(pScanResult, &ScanResults[index], sizeof(BleMac_t));
    memset(&ScanResults[index], 0, sizeof(BleMac_t));
    return true;
  }
  return false;
}


/**
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */
/**
 * @brief   Start scanning for Bluetooth devices
 * @param   none
 * @retval  bool          : true on success, false on failure
 */
static bool GsmBleStartScanning (void)
{
  eAtResult AtResult;

  TRACE_MODEM("Start Bluetooth scanning\n\r");

  /* power on the bluetooth radio */
  AtResult = gsmCommsSendAtCommand("AT+QBTPWR=1", BLUETOOTH_AT_TIMEOUT_MS);

  /* Register a GATT client */
  AtResult = gsmCommsSendAtCommand("AT+QBTGATCREG=1,\"B001\"", BLUETOOTH_AT_TIMEOUT_MS);

  /* Start scanning LE device */
  AtResult = gsmCommsSendAtCommand("AT+QBTGATCSCAN=1,\"B001\"", BLUETOOTH_AT_TIMEOUT_MS);

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief   Stop scanning for Bluetooth devices
 * @param   none
 * @retval  bool          : true on success, false on failure
 */
static bool GsmBleStopScanning (void)
{
  eAtResult AtResult;

  TRACE_MODEM("Stop Bluetooth scanning\n\r");

  /* Stop scanning LE device */
  AtResult = gsmCommsSendAtCommand("AT+QBTGATCSCAN=0,\"B001\"", BLUETOOTH_AT_TIMEOUT_MS);

  /* De-register a GATT client */
  AtResult = gsmCommsSendAtCommand("AT+QBTGATCREG=0,\"B001\"", BLUETOOTH_AT_TIMEOUT_MS);

  /* power off the bluetooth radio */
  AtResult = gsmCommsSendAtCommand("AT+QBTPWR=0", BLUETOOTH_AT_TIMEOUT_MS);

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief   List scanned Bluetooth devices
 * @param   none
 * @retval  bool          : true on success, false on failure
 */
static bool GsmBleListScanning (void)
{
  for (int index = 0; index < BLE_NO_OF_RESULTS; index++)
  {
    ConsolePrintf("BLE saved:[%d] %s, %d[%d]\r\n", index, ScanResults[index].MacAddress, ScanResults[index].Rssi, (ScanResults[index].Rssi-127));
  }
  sleep(1);
  return true;
}


/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       Configure audio
 * @param[in]   argCount    : THe number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int GsmBleConsoleScan (int argCount, char **pArguments)
{
  /* check for the correct number of arguments */
  if (1 == argCount)
  {
    if (!strcmp(pArguments[0], "on"))
    {
      GsmBleStartScanning();
    }
    else if (!strcmp(pArguments[0], "off"))
    {
      GsmBleStopScanning();
    }
    else
    {
      GsmBleListScanning();
    }
    return(CONSOLE_ERROR_OK);
  }
  else if (2 == argCount)
  {
    /* Sample string "+QBTGATCSCAN: "B001",FAA2665123DC,27,020106030201A20C1601A200440AA7C7127D94BB0309545919FFD007800300000100380CB134" */
    if (!strcmp(pArguments[0], "+QBTGATCSCAN:"))
    {
      /* Glue the pArguments back as if what return from the Modem */
      char combined[256];
      snprintf(combined, sizeof(combined), "%s %s", pArguments[0], pArguments[1]);
      GsmBleDecodeScanResult(combined);
      GsmBleListScanning();
      return(CONSOLE_ERROR_OK);
    }
  }
  return(CONSOLE_ARG_NO);
}

