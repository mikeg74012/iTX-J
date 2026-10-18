#ifndef __hardware_h__
#define __hardware_h__

#define GD32F30X_HD

#include "GD32F30x.h"

#define ACCEL_SPI           SPI0
#define PSRAM_SPI           SPI1
#define GSM_UART            USART0
#define WIFI_UART           USART1
#define CONSOLE_UART        USART3
#define GNSS_UART           USART4



#define USB_CC2             defpin(PTA,1)
#define TX_TO_WIFI          defpin(PTA,2)
#define RX_FROM_WIFI        defpin(PTA,3)
#define WIFI_RESET          defpin(PTA,4)
#define WIFI_ENABLE         defpin(PTA,5)
#define WIFI_PROG           defpin(PTA,6)
#define GSM_POWER_EN        defpin(PTA,7)
#define GSM_POWERKEY        defpin(PTA,8)
#define TXD_TO_MODEM        defpin(PTA,9)
#define RXD_FROM_MODEM      defpin(PTA,10)
#define CTS_FROM_MODEM      defpin(PTA,11)
#define RTS_TO_MODEM        defpin(PTA,12)
#define CHRGR_PSEL          defpin(PTC,0)
#define USB_CC1             defpin(PTC,1)
#define USB_D_P             defpin(PTC,2)
#define USB_D_N             defpin(PTC,3)
#define HW_VER_EN           defpin(PTC,4)
#define DTR_TO_MODEM        defpin(PTC,5)
#define RI_FROM_MODEM       defpin(PTC,6)
#define DCD_FROM_MODEM      defpin(PTC,7)
#define USER_LED_GREEN      defpin(PTC,8)
#define USER_LED_RED        defpin(PTC,9)
#define DEBUG_TX            defpin(PTC,10)
#define DEBUG_RX            defpin(PTC,11)
#define IMU_INT1            defpin(PTC,12)
#define USB_DETECT          defpin(PTC,13)
#define GNSS_TX             defpin(PTD,2)
#define SPI1_MOSI           defpin(PTB,15)
#define SPI1_MISO           defpin(PTB,14)
#define SPI1_SCK            defpin(PTB,13)
#define SPI_CS_SRAM         defpin(PTB,12)
#define USER_BUTTON         defpin(PTB,11)
#define I2C1_SDA            defpin(PTB,9)
#define I2C1_SCL            defpin(PTB,8)
#define IMU_INT2            defpin(PTB,7)
#define SPI_CS_IMU          defpin(PTB,6)
#define SPI0_MOSI           defpin(PTB,5)   /* using spi0 */
#define SPI0_MISO           defpin(PTB,4)   /* using spi0 */
#define SPI0_SCK            defpin(PTB,3)   /* using spi0 */
#define BOARD_VER_DETECT    defpin(PTB,1)
#define GNSS_POWER_EN       defpin(PTB,0)



#endif
