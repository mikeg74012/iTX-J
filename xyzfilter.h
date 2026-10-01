/**
 **************************************************************************************************
 * @file   xyzfilter.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   11/12/2019
 *
 * @brief  XYZ Filter for orientation assessment.
 *
 **************************************************************************************************
 */
#ifndef XYZ_FILTER_H
#define XYZ_FILTER_H

#include "motionbuffer.h"

/* its-136 define the limit for the xyz to be a valid range */
#define ERROR_XYZ                     2000

/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */
/* API */
void            XyzFilterAddToFifoMean          (AccelerationData_t *pAccelData);
void            XyzFilterAddFifoMeanIntoFilter  (void);
void            XyzGetFilteredData              (AccelerationData_t *pAccelData);
void            XyzGetOrientation               (int * pPitch, int * pRoll);

/* Helper to calculate the roll and pitch */
void            CalculateOrientation            (float X, float Y, float Z, int * pPitch, int * pRoll);

/* Console Commands */
int             XyzConsoleXyz                   (int ArgCount, char **pArguments);

#endif /*XYZ_FILTER_H */
