/**
 **************************************************************************************************
 * @file   motionbuffer.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  Motion Monitor buffers for Accelerometer, Gyro and GNSS data.
 *
 **************************************************************************************************
 */

#include "global.h"


/*
 **************************************************************************************************
 *                                         PRIVATE DEFINES
 **************************************************************************************************
 */


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
/* For buffer handling we must have byte-packed structures and arrays */
#pragma pack(1)
static ImuData_t ImuBuffer[ELEMENTS_IN_IMU_BUFFER];
#pragma pack()

/* Read and Write data indexes */
static uint32_t ReadIndex   = 0;
static uint32_t WriteIndex  = 0;
static uint32_t RecordCount = 0;

/* Flag to prevent writing when buffer is being read */
static bool BufferFrozen = false;


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */

/**
 * @brief     Initialise/Reset buffer
 * @param     None
 * @return    None
 */
void MotionBufferResetBuffer (void)
{
  /* reset the read and write indexes */
  ReadIndex = 0;
  WriteIndex = 0;
  RecordCount = 0;

  /* clear the buffer */
  memset(ImuBuffer, 0, (ELEMENTS_IN_IMU_BUFFER * sizeof(ImuData_t)));
}


/**
 * @brief     Add one element of IMU data
 * @param     Pointer to buffer containing the data to write into the motion buffer
 * @return    None
 */
void MotionBufferWriteSingle (ImuData_t *pBuffer)
{
  if (!BufferFrozen)
  {
    /* copy the data into the buffer at the current write index position */
    memcpy(&ImuBuffer[WriteIndex], pBuffer, sizeof(ImuData_t));

    /* increment the write index, and wrap if needed */
    if (++WriteIndex >= ELEMENTS_IN_IMU_BUFFER)
    {
      WriteIndex = 0;
    }

    /* if we have caught up with the readindex, increment in (lose oldest data) */
    if (WriteIndex == ReadIndex)
    {
      if (++ReadIndex >= ELEMENTS_IN_IMU_BUFFER)
      {
        ReadIndex = 0;
      }
    }
    else
    {
      RecordCount++;
    }
  }
}


/**
 * @brief     Read a single entry from the motion buffer (from current read pointer)
 * @param     Pointer to buffer to put the data into
 * @return    true on success, false on failure
 */
bool MotionBufferReadSingle (ImuData_t *pBuffer)
{
  /* check for empty buffer */
  if (ReadIndex == WriteIndex)
  {
    /* We should never get out of sync, but this is here for safety */
    RecordCount = 0;
    return false;
  }
  else
  {
    /* copy the data into the buffer from the current read index position */
    memcpy(pBuffer, &ImuBuffer[ReadIndex], sizeof(ImuData_t));

    /* increment the read index, wrap if necessary */
    if (++ReadIndex >= ELEMENTS_IN_IMU_BUFFER)
    {
      ReadIndex = 0;
    }
    RecordCount--;
  }
  return true;
}


/**
 * @brief     Get the number of (unread) elements in the buffer
 * @param     None
 * @return    Number of unread records in the buffer
 */
uint32_t MotionBufferGetRecordCount (void)
{
  return RecordCount;
}



/**
 * @brief     Get a number of recent records, without affecting the read pointer
 * @param     pBuffer      : pointer to a buffer to put the data into
 * @param     noOfElements : number of elements to copy
 * @return    none
 */
void MotionBufferGetLatestData (ImuData_t *pBuffer, uint16_t noOfElements)
{
  uint32_t GetIndex;

  /* calculate the starting index */
  if (WriteIndex >= noOfElements)
  {
    GetIndex = WriteIndex - noOfElements;
  }
  else
  {
    GetIndex = ELEMENTS_IN_IMU_BUFFER - (noOfElements - WriteIndex);
  }

  /* copy the data */
  while (noOfElements)
  {
    /* copy the data into the buffer from the current read index position */
    memcpy(pBuffer++, &ImuBuffer[GetIndex], sizeof(ImuData_t));
    if (++GetIndex >= ELEMENTS_IN_IMU_BUFFER)
    {
      GetIndex = 0;
    }
    noOfElements--;
  }
}


/**
 * @brief   Control whether the motion buffer is frozen.  Used to prevent writing to the buffer
 *          while it is being read
 * @param   freeze      : true = freeze buffer, false = un-freeze
 * @return  none
 */
void MotionBufferFreeze (bool freeze)
{
  BufferFrozen = freeze;
}
