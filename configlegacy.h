/**
 **************************************************************************************************
 * @file   configlegacy.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  Legacy config formats, to allow conversion from older to newer versions
 *
 **************************************************************************************************
 */
#ifndef CONFIGLEGACY_H
#define CONFIGLEGACY_H

#include "main.h"
#include <stdbool.h>


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */

/*
 * CONFIG VERSION 4
 */
#define CONFIG_MAX_FLAGS_V4  8
typedef struct
{
  /* the config store version - increments when the store changes */
  uint32_t ConfigVersion;
  uint16_t WustFlag;                                     /* indicates whether Wake-Up Self-Test has occurred since hibernation */

  /* communication related */
  char     VoiceCallNumber[MAX_MOBILE_BUF_SIZE];         /* Voice call number used in case of emergency event (impact or red button press) */
  char     SmsNumber[MAX_MOBILE_BUF_SIZE];               /* SMS number used to send emergency SMS data to.  Also used for debug and status SMS */
  char     GprsUrl[MAX_URL_BUF_SIZE];                    /* URL of server to connect to if HTTP(s) communications are specified */
  char     FwUpdateUrl[MAX_URL_BUF_SIZE];                /* URL of firmware update server */
  char     IpAddress[MAX_IP_ADDRESS_BUF_SIZE];           /* IP address of server to connect to if raw TCP communications are specified */
  uint32_t IpPort;                                       /* IP port of server to connect to if raw TCP communications are specified */

  /* debug flags */
  bool     Flags[CONFIG_MAX_FLAGS_V4];
} configv4_t;



/*
 * CONFIG VERSION 5
 */
#define CONFIG_MAX_FLAGS_V5  8
typedef struct
{
  /* the config store version - increments when the store changes */
  uint32_t ConfigVersion;
  uint16_t WustFlag;                                     /* indicates whether Wake-Up Self-Test has occurred since hibernation */

  /* Access Point details */
  char     ApnUrl[MAX_URL_SIZE];
  char     ApnUsername[MAX_USERNAME_SIZE];
  char     ApnPassword[MAX_PASSWORD_SIZE];

  /* communication related */
  char     VoiceCallNumber[MAX_MOBILE_BUF_SIZE];         /* Voice call number used in case of emergency event (impact or red button press) */
  char     SmsNumber[MAX_MOBILE_BUF_SIZE];               /* SMS number used to send emergency SMS data to.  Also used for debug and status SMS */
  char     GprsUrl[MAX_URL_BUF_SIZE];                    /* URL of server to connect to if HTTP(s) communications are specified */
  char     FwUpdateUrl[MAX_URL_BUF_SIZE];                /* URL of firmware update server */
  char     IpAddress[MAX_IP_ADDRESS_BUF_SIZE];           /* IP address of server to connect to if raw TCP communications are specified */
  uint32_t IpPort;                                       /* IP port of server to connect to if raw TCP communications are specified */

  /* debug flags */
  bool     Flags[CONFIG_MAX_FLAGS_V5];
} configv5_t;


/*
 * CONFIG VERSION 6
 */
#define CONFIG_MAX_FLAGS_V6  8
typedef struct
{
  /* the config store version - increments when the store changes */
  uint32_t ConfigVersion;
  uint16_t WustFlag;                                     /* Indicates whether Wake-Up Self-Test has occurred since hibernation */
  bool     TestMode;                                     /* Factory test mode - must NEVER be set in normal operation */

  /* Access Point details */
  char     ApnUrl[MAX_URL_SIZE];
  char     ApnUsername[MAX_USERNAME_SIZE];
  char     ApnPassword[MAX_PASSWORD_SIZE];

  /* communication related */
  char     VoiceCallNumber[MAX_MOBILE_BUF_SIZE];         /* Voice call number used in case of emergency event (impact or red button press) */
  char     SmsNumber[MAX_MOBILE_BUF_SIZE];               /* SMS number used to send emergency SMS data to.  Also used for debug and status SMS */
  char     GprsUrl[MAX_URL_BUF_SIZE];                    /* URL of server to connect to if HTTP(s) communications are specified */
  char     FwUpdateUrl[MAX_URL_BUF_SIZE];                /* URL of firmware update server */
  char     IpAddress[MAX_IP_ADDRESS_BUF_SIZE];           /* IP address of server to connect to if raw TCP communications are specified */
  uint32_t IpPort;                                       /* IP port of server to connect to if raw TCP communications are specified */

  /* debug flags */
  bool     Flags[CONFIG_MAX_FLAGS_V6];

  /* CRC32 checksum across entire config store.  Must be the last item! */
  uint32_t Checksum;

} configv6_t;



/*
 * CONFIG VERSION 7
 */
#define CONFIG_MAX_FLAGS_V7  8
typedef struct
{
  /* the config store version - increments when the store changes */
  uint32_t ConfigVersion;
  uint16_t WustFlag;                                     /* Indicates whether Wake-Up Self-Test has occurred since hibernation */
  bool     TestMode;                                     /* Factory test mode - must NEVER be set in normal operation */

  /* Access Point details */
  char     ApnUrl[MAX_URL_SIZE];
  char     ApnUsername[MAX_USERNAME_SIZE];
  char     ApnPassword[MAX_PASSWORD_SIZE];

  /* communication related */
  char     VoiceCallNumber[MAX_MOBILE_BUF_SIZE];         /* Voice call number used in case of emergency event (impact or red button press) */
  char     SmsNumber[MAX_MOBILE_BUF_SIZE];               /* SMS number used to send emergency SMS data to.  Also used for debug and status SMS */
  char     GprsUrl[MAX_URL_BUF_SIZE];                    /* URL of server to connect to if HTTP(s) communications are specified */
  char     FwUpdateUrl[MAX_URL_BUF_SIZE];                /* URL of firmware update server */
  char     IpAddress[MAX_IP_ADDRESS_BUF_SIZE];           /* IP address of server to connect to if raw TCP communications are specified */
  uint32_t IpPort;                                       /* IP port of server to connect to if raw TCP communications are specified */
  uint8_t  Customer;                                     /* Customer ID */

  /* Option flags */
  bool     Flags[CONFIG_MAX_FLAGS_V7];

  /* CRC32 checksum across entire config store.  Must be the last item! */
  uint32_t Checksum;

} configv7_t;


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */


#endif /* CONFIGLEGACY_H */
