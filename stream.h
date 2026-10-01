/**
 ******************************************************************************
 * @file Strean.h
 *
 * @author Adam Toner
 * @date   10/05/2019
 *
 * @brief  This header provides a simple abstraction layer for stream based
 *         communications over any interface (e.g. UART, SPI, CAN, USB etc)
 *
 ******************************************************************************
 * @attention           (c) Ripcord Designs Limited 2019
 ******************************************************************************
*/

#ifndef STREAM_H_
#define STREAM_H_

#include <stdint.h>

/*
 **************************************************************************************************
 *                                           PUBLIC DATA TYPES
 **************************************************************************************************
 */
typedef struct
{
  uint8_t  (*Get)            (void);
  uint8_t  (*GetWithTimeout) (uint32_t Delay, bool *Timeout);
  bool     (*DataAvailable)  (void);
  bool     (*Put)            (uint8_t Data);
  bool     (*Send)           (uint8_t *Data);
  uint32_t (*SendBuf)        (uint8_t *Data, uint32_t Len);  //Update this to return the actual number of bytes sent
} stream_t;

#endif /* STREAM_H_ */

