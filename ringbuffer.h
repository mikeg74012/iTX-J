/**
 **************************************************************************************************
 * @file   ringbuffer.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  General purpose ring buffer implementation.  Public Interface.
 *
 **************************************************************************************************
 * @attention                   (c) Ripcord Designs Limited 2019
 **************************************************************************************************
 */

#ifndef _RINGBUFFER_H_
#define _RINGBUFFER_H_

#include <stdbool.h>


/*
 **************************************************************************************************
 *                                       PUBLIC DEFINES
 **************************************************************************************************
 */
#define RINGBUF_ERROR_FULL  0xFFFF



/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */
void ringbufferInit          (void);
int  ringbufferCreate        (void *pBufStart, int Size, int DataSize);
void ringbufferDestroy       (int bufferIndex);
bool ringbufferPut           (int bufferHandle, void *pdata);
bool ringbufferGet           (int bufferHandle, void *pData);
bool ringbufferDataAvailable (int bufferHandle);


#endif /* _RINGBUFFER_H_ */
