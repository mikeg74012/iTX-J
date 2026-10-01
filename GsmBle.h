/**
 **************************************************************************************************
 * @file   GsmBle.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  BLE scanning module (no other Bluetooth functions)
 *
 **************************************************************************************************
 */
#ifndef _GSMBLE_H_
#define _GSMBLE_H_


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
typedef struct
{
  char MacAddress[13];
  uint8_t Rssi;
} BleMac_t;

/* Number of BLE scan results stored */
#define BLE_NO_OF_RESULTS          5


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
/* API */
bool GsmBleScan             (void);
bool GsmStopBleScan         (void);
bool GsmBleDecodeScanResult (char * scanMessage);
bool GsmBleGetScanResult    (BleMac_t * pScanResult, int index);

/* Console Commands */
int GsmBleConsoleScan    (int argCount, char **pArguments);

#endif /* _GSMBLE_H_ */

