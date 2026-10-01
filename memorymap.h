/**
 **************************************************************************************************
 * @file   memorymap.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 * @date   2020-06-16
 *
 * @brief  Address map for the serial pSRAM chip and FLASH
 *
 **************************************************************************************************
 * @attention                   (c) Ripcord Designs Limited 2019
 **************************************************************************************************
 */
#ifndef MEMORYMAP_H
#define MEMORYMAP_H

/*
 **************************************************************************************************
 *                                       PUBLIC DEFINES
 **************************************************************************************************
 */

/*
 **************************************************************************************************
 *                                       FLASH MEMORY MAPPING
 **************************************************************************************************
 */
#define BOOTLOADER_ADDRESS                    0x08000000
#define APPLICATION_ADDRESS                   0x08010000
#define CONFIG_ADDRESS                        0x0807F800


/*
 **************************************************************************************************
 *                                       SRAM MEMORY MAPPING
 **************************************************************************************************
 */

/* Note: OTA memory map RAM areas and Common Area should not be altered as to not break OTA image copy in Bootloader Application */

/* Serial Ram Size (8192KB) */
#define SERIAL_RAM_DATA_SIZE                 0x800000

/*
 * OTA Image Data Storage (512KB)
 */
#define SERIAL_OTA_DATA_AREA_START            0x00000000
#define SERIAL_OTA_DATA_AREA_SIZE             0x80000
#define SERIAL_OTA_DATA_AREA_END              (SERIAL_OTA_DATA_AREA_START + SERIAL_OTA_DATA_AREA_SIZE - 1)

/*
 * OTA Image Info Storage (1KB)
 */
#define SERIAL_OTA_INFO_AREA_START            (SERIAL_OTA_DATA_AREA_END + 1)
#define SERIAL_OTA_INFO_AREA_SIZE             0x400
#define SERIAL_OTA_INFO_AREA_END              (SERIAL_OTA_INFO_AREA_START + SERIAL_OTA_INFO_AREA_SIZE - 1)

/*
 * OTA Manifest file Storage (8KB)
 */
#define SERIAL_OTA_MANIFEST_AREA_START        (SERIAL_OTA_INFO_AREA_END + 1)
#define SERIAL_OTA_MANIFEST_AREA_SIZE         0x2000
#define SERIAL_OTA_MANIFEST_AREA_END          (SERIAL_OTA_MANIFEST_AREA_START + SERIAL_OTA_MANIFEST_AREA_SIZE - 1)

/*
 * Bootloader App Common Area - used for passing info between App and Bootloader (1KB)
 */
#define SERIAL_RAM_COMMON_AREA_SIZE         0x400
#define SERIAL_RAM_COMMON_AREA_START        (SERIAL_RAM_DATA_SIZE - SERIAL_RAM_COMMON_AREA_SIZE)

/* Note: OTA memory map RAM areas and Common Area should not be altered as to not break OTA image copy in Bootloader Application */

/*
 * Mini-Event Storage (128K)
 */
#define SERIAL_RAM_MINI_EVENT_AREA_START      (SERIAL_OTA_MANIFEST_AREA_END + 1)
#define SERIAL_RAM_MINI_EVENT_AREA_SIZE       0x20000
#define SERIAL_RAM_MINI_EVENT_AREA_END        (SERIAL_RAM_MINI_EVENT_AREA_START + SERIAL_RAM_MINI_EVENT_AREA_SIZE - 1)

/*
 * Impact-Event Storage  (20KB)
 */
#define SERIAL_RAM_IMPACT_EVENT_AREA_START    (SERIAL_RAM_MINI_EVENT_AREA_END + 1)
#define SERIAL_RAM_IMPACT_EVENT_AREA_SIZE     0x5000
#define SERIAL_RAM_IMPACT_EVENT_AREA_END      (SERIAL_RAM_IMPACT_EVENT_AREA_START + SERIAL_RAM_IMPACT_EVENT_AREA_SIZE - 1)

/*
 * GNSS Event Data Storage (256KB)
 */
#define SERIAL_RAM_GNSS_EVENT_AREA_START      (SERIAL_RAM_IMPACT_EVENT_AREA_END + 1)
#define SERIAL_RAM_GNSS_EVENT_AREA_SIZE       0x40000
#define SERIAL_RAM_GNSS_EVENT_AREA_END        (SERIAL_RAM_GNSS_EVENT_AREA_START + SERIAL_RAM_GNSS_EVENT_AREA_SIZE - 1)

/*
 * GPRS Message Queue (Low Priority Non-Emergency Messages) (512 KB)
 * Roughly 350 x 1500 byte messages (or 11 hours of driving)
 */
#define SERIAL_RAM_GPRS_QUEUE_LP_AREA_START   (SERIAL_RAM_GNSS_EVENT_AREA_END + 1)
#define SERIAL_RAM_GPRS_QUEUE_LP_AREA_SIZE    0x80000
#define SERIAL_RAM_GPRS_QUEUE_LP_AREA_END     (SERIAL_RAM_GPRS_QUEUE_LP_AREA_START + SERIAL_RAM_GPRS_QUEUE_LP_AREA_SIZE - 1)

/*
 * GPRS Message Queue (High Priority Emergency Messages) (64 KB)
 * Roughly 40 x 1500 byte messages
 */
#define SERIAL_RAM_GPRS_QUEUE_HP_AREA_START   (SERIAL_RAM_GPRS_QUEUE_LP_AREA_END + 1)
#define SERIAL_RAM_GPRS_QUEUE_HP_AREA_SIZE    0x10000
#define SERIAL_RAM_GPRS_QUEUE_HP_AREA_END     (SERIAL_RAM_GPRS_QUEUE_HP_AREA_START + SERIAL_RAM_GPRS_QUEUE_HP_AREA_SIZE - 1)

/*
 * Task Level WatchDog Data Storage (1KB)
 */
#define SERIAL_RAM_WATCHDOG_DATA_AREA_START     (SERIAL_RAM_GPRS_QUEUE_HP_AREA_END + 1)
#define SERIAL_RAM_WATCHDOG_DATA_AREA_SIZE      0x400
#define SERIAL_RAM_WATCHDOG_DATA_AREA_END       (SERIAL_RAM_WATCHDOG_DATA_AREA_START + SERIAL_RAM_WATCHDOG_DATA_AREA_SIZE - 1)

/*
 * Test Area - used for RAM self-test (1KB)
 */
#define SERIAL_RAM_SELF_TEST_AREA_START         (SERIAL_RAM_WATCHDOG_DATA_AREA_END + 1)
#define SERIAL_RAM_SELF_TEST_AREA_SIZE          0x400
#define SERIAL_RAM_SELF_TEST_AREA_END           (SERIAL_RAM_SELF_TEST_AREA_START + SERIAL_RAM_SELF_TEST_AREA_SIZE - 1)


/*
 * Unit mounting survey data (100X2x4 1KB), 1 week plus [0] page (8KB)
 */
#define SERIAL_RAM_MOUNT_SURVEY_AREA_START      (SERIAL_RAM_SELF_TEST_AREA_END + 1)
#define SERIAL_RAM_MOUNT_SURVEY_PAGE_SIZE       0x400
#define SERIAL_RAM_MOUNT_SURVEY_PAGES           8
#define SERIAL_RAM_MOUNT_SURVEY_AREA_SIZE       SERIAL_RAM_MOUNT_SURVEY_PAGES * (SERIAL_RAM_MOUNT_SURVEY_PAGE_SIZE)
#define SERIAL_RAM_MOUNT_SURVEY_AREA_END        (SERIAL_RAM_MOUNT_SURVEY_AREA_START + SERIAL_RAM_MOUNT_SURVEY_AREA_SIZE - 1)

/*
 * Unit boot loader image (0x10000)
 */
#define SERIAL_OTA_BL_DATA_AREA_START           SERIAL_RAM_MOUNT_SURVEY_AREA_END+1
#define SERIAL_OTA_BL_DATA_AREA_SIZE            0x10000
#define SERIAL_OTA_BL_DATA_AREA_END             (SERIAL_OTA_BL_DATA_AREA_START + SERIAL_OTA_BL_DATA_AREA_SIZE - 1)

/*
 * OTA Image Info Storage (1KB)
 */
#define SERIAL_OTA_BL_INFO_AREA_START           (SERIAL_OTA_BL_DATA_AREA_END + 1)
#define SERIAL_OTA_BL_INFO_AREA_SIZE            0x400
#define SERIAL_OTA_BL_INFO_AREA_END             (SERIAL_OTA_BL_INFO_AREA_START + SERIAL_OTA_BL_INFO_AREA_SIZE - 1)

/*
 * OTA Manifest file Storage (4KB)
 */
#define SERIAL_OTA_BL_MANIFEST_AREA_START       (SERIAL_OTA_BL_INFO_AREA_END + 1)
#define SERIAL_OTA_BL_MANIFEST_AREA_SIZE        0x1000
#define SERIAL_OTA_BL_MANIFEST_AREA_END         (SERIAL_OTA_BL_MANIFEST_AREA_START + SERIAL_OTA_BL_MANIFEST_AREA_SIZE - 1)

/*
 * Event log back area - used for keep event log during LP sleep (2KB)
 */
#define SERIAL_RAM_EVEVT_LOG_AREA_START     (SERIAL_RAM_MOUNT_SURVEY_AREA_END + 1)
#define SERIAL_RAM_EVEVT_LOG_AREA_SIZE      0x800
#define SERIAL_RAM_EVEVT_LOG_AREA_END       (SERIAL_RAM_EVEVT_LOG_AREA_START + SERIAL_RAM_EVEVT_LOG_AREA_SIZE - 1)

/*
 * Low power sleep data backup area - used for keep values during LP sleep (1KB, 256 uint32_t value)
 */
#define SERIAL_RAM_LPSLEEP_KEEP_AREA_START   (SERIAL_RAM_EVEVT_LOG_AREA_END + 1)
#define SERIAL_RAM_LPSLEEP_KEEP_AREA_SIZE    0x400
#define SERIAL_RAM_LPSLEEP_KEEP_AREA_END     (SERIAL_RAM_LPSLEEP_KEEP_AREA_START + SERIAL_RAM_LPSLEEP_KEEP_AREA_SIZE - 1)

#endif /* MEMORYMAP_H */
