/**
 **************************************************************************************************
 * @file   flash.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  On-chip FLASH memory module for GD32.  Used to erase and write to the on-chip FLASH.
 *         It shares a common Ripcord API with SPI modules for other chips, so higher software
 *         layers can be easily moved between different MCU's.
 *
 **************************************************************************************************
 * @attention                      (c) Ripcord Designs Limited 2022
 **************************************************************************************************
 */
#include "global.h"



/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */
#define FLASH_DATATYPE           16
#define ONE_BYTE                 8

/* FLASH programming modes */
#define FLASH_TYPEPROGRAM_HALFWORD   (0x01U)  /*!<Program a half-word (16-bit) at a specified address.*/
#define FLASH_TYPEPROGRAM_WORD       (0x02U)  /*!<Program a word (32-bit) at a specified address.*/
#define FLASH_TYPEPROGRAM_DOUBLEWORD (0x03U)  /*!<Program a double word (64-bit) at a specified address*/



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
bool FlashProgram (uint32_t ProgramType, uint32_t Address, uint64_t Data);


/*
 **************************************************************************************************
 *                                   PUBLIC SECTION / INTERFACE
 **************************************************************************************************
 */

/**
 * @brief  Initialise this FLASH module
 * @param  None
 * @retval None
 */
bool FlashInit (void)
{
  /* Nothing to do on this chip */
  return true;
}




/**
 * @brief     Erase a section of FLASH
 * @param     Address:  Start address of the page to erase
 * @param     NoOfPages :  Number of pages to erase
 * @return    true on success, false on failure
 */
bool FlashErase (uint32_t PageAddress, uint32_t NoOfPages)
{
  unsigned char state = 1;
  
  /* Unlock flash */
  fmc_unlock();

  /* Clear any pending flags */
  
  write_register (FMC, FMC_STAT0, (1 << 5) | (1 << 4) | (1 << 2));

  /* erase the flash pages */
  for (int PageCounter = 0; PageCounter < NoOfPages; PageCounter++)
  {
    state = fmc_page_erase(PageAddress + (FLASH_PAGE_SIZE * PageCounter));
    
    write_register (FMC, FMC_STAT0, (1 << 5) | (1 << 4) | (1 << 2));
    
	if (state != 1) 
    {
        break;
    }
  }

  /* Lock flash */
  fmc_lock();

  return (state == 1);
}


/**
 * @brief     Write data to FLASH
 * @param     Address:  Start address of the page to erase
 * @param     Length :  Number of pages to erase
 * @return    true on success, false on failure
 */
bool FlashWrite (uint32_t Address, uint8_t * pData, uint32_t Length)
{
  bool retval = true;

  /* Unlock the flash To write the Contents */
  fmc_unlock();

  /*
   * Flash data type can be 16/32/64
   * so calculate the number of bytes to be written in flash w.r.t writeBuffer structure size and the Datatype
   */
  int sizeToWriteFlash = (Length * ONE_BYTE) / FLASH_DATATYPE;

  for (int i = 0; i < sizeToWriteFlash; i++)
  {
    if (!FlashProgram (FLASH_TYPEPROGRAM_HALFWORD, Address, ((uint16_t *)pData)[i]))
    {
      retval = false;
      break;
    }
    Address += 2;
  }

  /* Lock the flash after write */
  fmc_lock();
  
  return retval;
}


/*
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */

/**
* @brief  Program halfword, word or double word at a specified address
* @warning      The function HWL_FLASH_unlock() should be called before to unlock the FLASH interface
*               The function HWL_FLASH_lock() should be called after to lock the FLASH interface
*
* @note   If an erase and a program operations are requested simultaneously,
*         the erase operation is performed before the program one.
*
* @note   FLASH should be previously erased before new programming (only exception to this
*         is when 0x0000 is programmed)
*
* @param[in]  ProgramType   Indicate the way to program at a specified address.
*                       This parameter can be a value of @ref FLASH_Type_Program
* @param[in]  Address       Specify the address to be programmed.
* @param[in]  Data          Specify the data to be programmed
*
* @return true on success, false on failure
*/
bool FlashProgram (uint32_t ProgramType, uint32_t Address, uint64_t Data)
{
  uint32_t address = Address;
  uint32_t data0   = 0;
  
  unsigned char state = 1;
  
  if (ProgramType == FLASH_TYPEPROGRAM_HALFWORD)
  {
    /* program flash, a half word */
    data0 = Data & 0xffff;
    state = fmc_halfword_program(address, data0);
    if (1 == state)
    {
        write_register (FMC, FMC_STAT0, (1 << 5) | (1 << 4) | (1 << 2));
    }
  }
  else if (ProgramType == FLASH_TYPEPROGRAM_WORD)
  {
    /* program flash, a word */
    data0 = Data & 0xffffffff;
    state = fmc_word_program(address, data0);
    
        write_register (FMC, FMC_STAT0, (1 << 5) | (1 << 4) | (1 << 2));
  }
  return (1 == state);
}


