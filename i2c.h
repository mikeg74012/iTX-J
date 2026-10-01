/**
 **************************************************************************************************
 * @file   i2c.h
 *
 * @author Ripcord Designs
 *
 * @brief  This module provides an abstraction layer for the on-chip I2C interface.
 *         It shares a common Ripcord API with I2C modules for other chips, so higher software
 *         layers can be easily moved between different MCU's.
 *
 **************************************************************************************************
 * @attention                  (c) Ripcord Designs Limited 2022
 **************************************************************************************************
 */
#ifndef _I2C_H_
#define _I2C_H_


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
/*
 * I2C Busses available on this chip.  The application layer
 * must use one of these bus definitions when initialising the port
 */
typedef enum
{
  I2C_BUS_0 = 0,
  I2C_BUS_1,
} i2cbus_e;


/*
 **************************************************************************************************
 *                                        GLOBAL VARIABLES
 **************************************************************************************************
 */


/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */
/* API */
bool   I2cInitialise   (i2cbus_e I2cBus, uint32_t BitRate);
bool   I2cDeInitialise (i2cbus_e I2cBus);
bool   I2cTransmit     (i2cbus_e I2cBus, uint8_t ChipAddress, uint8_t * pTxData, uint16_t Size);
bool   I2cReceive      (i2cbus_e I2cBus, uint8_t ChipAddress, uint8_t * pRxData, uint16_t Size);

bool   I2c_read_GG_bytes  (i2cbus_e I2cBus, uint8_t bus_address, uint8_t* p_buffer, uint8_t read_address, uint16_t number_of_byte);


/* Console Commands */
int    I2cConsoleProbe (int ArgCount, char **pArguments);
int    I2cConsoleStats (int ArgCount, char **pArguments);
int    I2cConsoleClear (int ArgCount, char **pArguments);

#endif /* _I2C_H_ */
