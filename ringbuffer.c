/**
 **************************************************************************************************
 * @file   ringbuffer.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  General purpose ring buffer implementation
 *
 **************************************************************************************************
 * @attention                   (c) Ripcord Designs Limited 2019
 **************************************************************************************************
 */

#include "global.h"


/*
 **************************************************************************************************
 *                                       PRIVATE DEFINES
 **************************************************************************************************
 */
#define NO_OF_BUFFERS  16

typedef struct
{
  unsigned char *pstart;
  int size;
  int read;
  int write;
  bool available;
} ringBuffer_t;


/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
static ringBuffer_t ringBuffer[NO_OF_BUFFERS];



/*
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */


/*
 **************************************************************************************************
 *                                   PUBLIC SECTION / INTERFACE
 **************************************************************************************************
 */
/**
  * @brief  Initialise The buffer to all available
  * @param  None
  * @retval None
  */
void ringbufferInit (void)
{
  static bool bufferInit = false;
  int bufferIndex;

  if (!bufferInit)
  {
    bufferInit = true;
    for (bufferIndex = 0; bufferIndex < NO_OF_BUFFERS; bufferIndex++)
    {
      ringBuffer[bufferIndex].available = true;
    }
  }
}


/**
  * @brief  Create a ring buffer with set parameters
  * @param  pBufStart  Pointer to start of the buffer
  *         Size       Size of the buffer in bytes
  *         DataSize   Size of data elements to be stored in the buffer
  * @retval index - buffer handle
  *         RINGBUF_ERROR_FULL  - if all buffers are occupied
  */
int ringbufferCreate (void *pBufStart, int Size)
{
  int bufferIndex;
  for (bufferIndex = 0; bufferIndex < NO_OF_BUFFERS; bufferIndex++ )
  {
    if(ringBuffer[bufferIndex].available == true)
    {
      ringBuffer[bufferIndex].available = false;
      ringBuffer[bufferIndex].pstart = pBufStart;
      ringBuffer[bufferIndex].size = Size;
      ringBuffer[bufferIndex].read = 0;
      ringBuffer[bufferIndex].write = 0;

      return bufferIndex;
    }
  }
  return RINGBUF_ERROR_FULL;
}


/**
  * @brief  Remove a previously allocated ring buffer
  * @param  index - buffer handle
  * @retval none
  */
void ringbufferDestroy (int bufferIndex)
{
  ringBuffer[bufferIndex].pstart = NULL;
  ringBuffer[bufferIndex].size = 0;
  ringBuffer[bufferIndex].read = 0;
  ringBuffer[bufferIndex].write = 0;
  ringBuffer[bufferIndex].available = true;
}


/**
  * @brief  Fill in the buffer with Data
  * @param  bufferHandle  Index of the buffer to use
  *         *pdata        Pointer to the data element to store
  * @retval true- if successfully written into the buffer
  *         false- if buffer is full
  */
bool ringbufferPut (int bufferHandle, uint8_t data)
{
    ringBuffer_t *r;
    
    r = &ringBuffer[bufferHandle];
    
    /* check if there is room in the buffer */
    if ((r->write + 1) % r->size == r->read)
    {
        return false;
    }

    r->pstart[r->write] =  data;

    /* increment the write pointer */
    r->write = (r->write + 1) % r->size;

    return true;
}


/**
  * @brief  Fetch the data from the buffer
  * @param  bufferHandle  Index of the buffer to use
  *         *pdata        Pointer to location to put the data in
  * @retval true- if data read
  *         false- if buffer empty
  */
bool ringbufferGet (int bufferHandle, unsigned char *pData)
{
    ringBuffer_t *r;
    
    r = &ringBuffer[bufferHandle];
    
  /* check if data is available (buffer not empty) */
  if (r->read != r->write)
  {
    /* calculate the source address */
    *pData = r->pstart[r->read];

    /* increment the read pointer */
    r->read = (r->read + 1) % r->size;
    
    return (true);
  }
  else
  {
    return (false);
  }
}


/**
  * @brief  Check if data is available in the buffer
  * @param  bufferHandle  Index of the buffer to use
  * @retval true- if data available
  *         false- if data not available
  */
bool ringbufferDataAvailable (int bufferHandle)
{
  if (ringBuffer[bufferHandle].read != ringBuffer[bufferHandle].write)
  {
    return (true);
  }
  return (false);
}

