/**
 **************************************************************************************************
 * @file   crcdriver.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   25/03/2020
 *
 * @brief  CRC Driver.
 *
 **************************************************************************************************
 * @attention                      (c) Ripcord Designs Limited 2022
 **************************************************************************************************
 */
#include "crcdriver.h"
//#include "hwl_define.h"
#include "Crc32.h"
#include "Crc16.h"


/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */


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



/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
/**
 * @brief     Initialise the CRC module
 * @return    none
 */
void CrcDriverInitialise (void)
{
  /* initialise the software CRC-32 module */
  crcInit();
}


/**
 * @brief     Calculate a CRC16 from a string
 * @param     Buffer containing data on which the CRC is to be calculated
 * @param     The length of the data in bytes
 * @return    The CRC16 value
 */
uint16_t CrcDriverCalculateCrc16 (uint8_t * pBuffer, uint32_t length)
{
  /* now calculate the CRC */
  uint32_t result = CRC16Calculate(pBuffer, length); 
  
  return (uint16_t)result;
}


/**
 * @brief     Calculate a CRC32 across a buffer
 * @param     pBuffer   : pointer to buffer containing data on which the CRC is to be calculated
 * @param     length    : number of data bytes in the buffer
 * @return    CRC       : The calculated CRC32 value
 */
uint32_t CrcDriverCalculateCrc32 (uint8_t * pBuffer, uint32_t length)
{
  uint32_t result = 0;

  /* initialise the hardware CRC engine for CRC-32 */
  crcInit();

  /* now calculate the CRC */
  result = crcFast(pBuffer, length);

  return result;
}


/**
 * @brief     Initialize CRC32 for calculation across multiple calls
 * @param     none
 * @return    none
 *
 * @WARNING - not thread safe
 */
void CrcDriverInitCrc32Multi (void)
{
  /* initialise the CRC-32 engine for computation across multiple calls */
  crcMultipleInit();
}

/**
 * @brief     Calculate a CRC32 across multiple calls.
 * @param     pBuffer   : pointer to buffer containing data on which the CRC is to be calculated
 * @param     length    : number of data bytes in the buffer
 * @return    CRC       : The calculated CRC32 value
 *
 * @WARNING - not thread safe
 */
uint32_t CrcDriverCalculateCrc32Multi (uint8_t * pBuffer, uint32_t length)
{
  uint32_t result = 0;

  result = crcMultipleCalculate (pBuffer, length);

  return result;
}


/**
 * @brief     Final inversion of CRC value when all calculations are complete
 * @param     The current CRC value
 * @return    The final CRC value
 *
 * @WARNING - not thread safe
 */
uint32_t CrcDriverFinishCrc32Multi (uint32_t crc)
{
  return (crcMultipleFinalize());
}


/*
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */
