/**
 **************************************************************************************************
 * @file   motionbuffer.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  Motion Monitor buffers for Accelerometer, Gyro and GNSS data.
 *
 **************************************************************************************************
 */

#ifndef MOTIONBUFFER_H
#define MOTIONBUFFER_H

#include "main.h"


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
/* For buffer handling we must have byte-packed structures and arrays */
#pragma pack(1)

typedef struct AccelerationData
{
  int16_t X;
  int16_t Y;
  int16_t Z;
} AccelerationData_t;

typedef struct GyroData
{
  int16_t X;
  int16_t Y;
  int16_t Z;
} GyroData_t;

/* Main IMU data structure used by motion monitor */
typedef struct
{
  AccelerationData_t accel;
  GyroData_t         gyro;
} ImuData_t;

#pragma pack()

#define IMU_FREQUENCY_HZ             100
#define IMU_BUFFER_SIZE_IN_SECONDS   10
#define ELEMENTS_IN_IMU_BUFFER       (IMU_FREQUENCY_HZ * IMU_BUFFER_SIZE_IN_SECONDS)

/* Impact Message details */
#define MESSAGE_IMPACT_RECORDS_TOTAL  1000
#define MESSAGE_IMPACT_RECORDS_FRAG   95


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
void     MotionBufferResetBuffer    (void);
void     MotionBufferWriteSingle    (ImuData_t *pBuffer);
bool     MotionBufferReadSingle     (ImuData_t *pBuffer);
uint32_t MotionBufferGetRecordCount (void);
void     MotionBufferGetLatestData  (ImuData_t *pBuffer, uint16_t noOfElements);
void     MotionBufferFreeze         (bool freeze);
void     MotionBufferReadFromPSRAM  (void);
void     MotionBufferSaveToPSRAM    (void);

#endif /* MOTIONBUFFER_H */
