/**
 **************************************************************************************************
 * @file   xyzfilter.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   11/12/2019
 *
 * @brief  XYZ Filter for orientation assessment and matrix rotation process.
 *
 **************************************************************************************************
 */

#include "global.h"


/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */

/* #define XYZ_MAX_FILTER_FACTOR 1000  */                /* 200 seconds */
#define XYZ_MAX_FILTER_FACTOR           150              /* 30 seconds */

/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
static int32_t fifoMeanTotalX = 0;
static int32_t fifoMeanTotalY = 0;
static int32_t fifoMeanTotalZ = 0;
static int32_t fifoDataCount  = 0;

/*
 * We store the filtered data as 32-bit data. The filter calculations are
 * also done using 32-bit data. This ensures that we do not lose precision
 * when dividing by large numbers.
 */
static int32_t filteredX   = 0;
static int32_t filteredY   = 0;
static int32_t filteredZ   = 0;
static int32_t filterCount = 0;

/**
 **************************************************************************************************
 *                                   PUBLIC SECTION / INTERFACE
 **************************************************************************************************
 */
/**
 * @brief     Add a set of acceleration data into the buffer mean calculation
 * @param     pAccelData  :Pointer to Acceleration data
 * @return    None
 */
void XyzFilterAddToFifoMean (AccelerationData_t *pAccelData)
{
  fifoMeanTotalX += (int32_t)pAccelData->X;
  fifoMeanTotalY += (int32_t)pAccelData->Y;
  fifoMeanTotalZ += (int32_t)pAccelData->Z;
  fifoDataCount++;
}


/**
 * @brief     Finish off the buffer mean calculation and add into the filter
 * @param     None
 * @return    None
 */
void XyzFilterAddFifoMeanIntoFilter (void)
{
  if (fifoDataCount != 0)
  {
    /* Calculate the mean values */
    fifoMeanTotalX /= fifoDataCount;
    fifoMeanTotalY /= fifoDataCount;
    fifoMeanTotalZ /= fifoDataCount;

    /* Now add the mean values into the filter */
    if (filterCount == 0)
    {
      /* The filter is empty, just use the new data */
      filteredX = ((int32_t)fifoMeanTotalX)<<16;
      filteredY = ((int32_t)fifoMeanTotalY)<<16;
      filteredZ = ((int32_t)fifoMeanTotalZ)<<16;
      filterCount++;
    }
    else
    {
      if (filterCount < XYZ_MAX_FILTER_FACTOR)
      {
        filterCount++;
      }
      /*
       * New filtered value =
       *   Old filtered value * (n-1)/n  +
       *   new data * (1/n)
       */
      filteredX = (filteredX/filterCount)*(filterCount-1) + (((int32_t)fifoMeanTotalX)<<16)/filterCount;
      filteredY = (filteredY/filterCount)*(filterCount-1) + (((int32_t)fifoMeanTotalY)<<16)/filterCount;
      filteredZ = (filteredZ/filterCount)*(filterCount-1) + (((int32_t)fifoMeanTotalZ)<<16)/filterCount;
    }
    /* Clear data ready for next FIFO buffer */
    fifoMeanTotalX = 0;
    fifoMeanTotalY = 0;
    fifoMeanTotalZ = 0;
    fifoDataCount = 0;
  }
}


/**
 * @brief     Get current XYZ filtered values
 * @param     pAccelData  : Pointer to AccelData structure
 * @return    None
 */
void XyzGetFilteredData (AccelerationData_t *pAccelData)
{
  /* Supply the values, removing extra precision first */
  pAccelData->X = (int16_t)(filteredX >> 16);
  pAccelData->Y = (int16_t)(filteredY >> 16);
  pAccelData->Z = (int16_t)(filteredZ >> 16);
}


/**
 * @brief     Helper function to calculate pitch and roll
 * @param     aX - X-axis value (float)
 * @param     aY - Y-axis value (float)
 * @param     aZ - Z-axis value (float)
 * @param     pPitch - Address to store pitch in (int pointer)
 * @param     pRoll - Address to store roll in (int pointer)
 * @return    None
 */
void CalculateOrientation (float aX, float aY, float aZ, int *pPitch, int *pRoll)
{
  *pPitch = (int) (acos(-aZ/ sqrt((aX * aX) + (aY * aY) + (aZ * aZ))) * (180.0 / M_PI));
  *pRoll  = (int) (atan2(aY, aX) * (180.0 / M_PI));
}


/**
 * @brief     Get pitch and roll from the filtered XYZ
 * @param     pPitch -  : address to store pitch in
 * @param     pRoll -   : address to store roll in
 * @return    None
 */
void XyzGetOrientation (int * pPitch, int * pRoll)
{
  CalculateOrientation(filteredX, filteredY, filteredZ, pPitch, pRoll);
}


/*
 **************************************************************************************************
 *                                     PRIVATE FUNCTIONS
 **************************************************************************************************
 */

/*
 **************************************************************************************************
 *                                     COMMAND LINE INTERFACE
 **************************************************************************************************
 */
/**
 * @brief       Get XYZ data
 * @param[in]   argCount    : The number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int XyzConsoleXyz (int ArgCount, char **pArguments)
{
  AccelerationData_t accelData;
  int pitch;
  int roll;

  /* get the filtered acceleraion data */
  XyzGetFilteredData(&accelData);

  /* get the orientation */
  XyzGetOrientation(&pitch, &roll);

  /* output the data to console */
  ConsolePrintf("XYZ X=%dmg,Y=%dmg,Z=%dmg\n\r", accelData.X, accelData.Y, accelData.Z);
  ConsolePrintf("XYZ pitch=%d, roll=%d\n\r", pitch, roll);

  /* indicate success */
  return (CONSOLE_ERROR_OK);
}


