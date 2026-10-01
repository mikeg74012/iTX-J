/**
 **************************************************************************************************
 * @file   console.h
 *
 * @author Ripcord Designs
 *
 * @brief  This module provides a simple Command Line Interface
 *
 **************************************************************************************************
 * @attention                   (c) Ripcord Designs Limited 2019
 **************************************************************************************************
 */
#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include <stdbool.h>
#include "stream.h"


/*
 **************************************************************************************************
 *                                       PUBLIC DEFINES
 **************************************************************************************************
 */
/* Console LOCK.  Provides atomic access to the console */
#define CONSOLE_USE_MUTEX


/* Foreground Colours for TRACE */
#define FG_DEFAULT  "\e[39m"
#define FG_BLACK    "\e[30m"
#define FG_RED      "\e[31m"
#define FG_GREEN    "\e[32m"
#define FG_YELLOW   "\e[33m"
#define FG_BLUE     "\e[34m"
#define FG_MAGENTA  "\e[35m"
#define FG_CYAN     "\e[36m"
#define FG_GRAY     "\e[37m"
#define FG_WHITE    "\e[97m"

/* Background Colours for TRACE */
#define BG_DEFAULT  "\e[49m"
#define BG_BLACK    "\e[40m"
#define BG_RED      "\e[41m"
#define BG_GREEN    "\e[42m"
#define BG_YELLOW   "\e[43m"
#define BG_BLUE     "\e[44m"
#define BG_MAGENTA  "\e[45m"
#define BG_CYAN     "\e[46m"
#define BG_GRAY     "\e[47m"
#define BG_WHITE    "\e[107m"

/* Disable all formatting */
#define NO_FORMAT   "\e[0m"

/*
 * TRACE macro for formatted console output
 * Should be used instead of ConsolePrintf for regular status output
 */
#ifdef CONSOLE_USE_MUTEX
  #define TRACE_(module_, ...)       \
  ({                                 \
    ConsoleLock();                   \
    RtcPrintCurrentTime();           \
    ConsolePrintf("[%s] ", module_); \
    ConsolePrintf(__VA_ARGS__);      \
    ConsoleUnlock();                 \
  })
#else
  #define TRACE_(module_, ...)       \
  ({                                 \
    RtcPrintCurrentTime();           \
    ConsolePrintf("[%s] ", module_); \
    ConsolePrintf(__VA_ARGS__);      \
  })
#endif

/*
 * TRACE_COLOUR macro for formatted console output with text and / or background colour
 * Should be used instead of ConsolePrintf for regular status output
 */
#ifdef CONSOLE_USE_MUTEX
  #define TRACE_COLOUR_(fgcolour_, bgcolour_, module_, ...)    \
  ({                                                           \
    ConsoleLock();                                             \
    ConsolePrintf("%s%s", fgcolour_, bgcolour_);               \
    RtcPrintCurrentTime();                                     \
    ConsolePrintf("[%s] ", module_);                           \
    ConsolePrintf(__VA_ARGS__);                                \
    ConsolePrintf(NO_FORMAT);                                  \
    ConsoleUnlock();                                           \
  })
#else
  #define TRACE_COLOUR_(fgcolour_, bgcolour_, module_, ...)    \
  ({                                                           \
    ConsolePrintf("%s%s", fgcolour_, bgcolour_);               \
    RtcPrintCurrentTime();                                     \
    ConsolePrintf("[%s] ", module_);                           \
    ConsolePrintf(__VA_ARGS__);                                \
    ConsolePrintf(NO_FORMAT);                                  \
  })
#endif


/* Error Codes */
#define CONSOLE_ERROR_OK      0
#define CONSOLE_CMD_UNKNOWN   1
#define CONSOLE_ARG_NO        2
#define CONSOLE_ARG_RANGE     3



/*
 **************************************************************************************************
 *                                       PUBLIC PROTOTYPES
 **************************************************************************************************
 */
void     ConsoleInitialise       (stream_t * pConsoleStream);
void     ConsoleTask             (void const * argument);
void     ConsolePrintf           (const char *format, ...);
void     ConsoleGets             (char *str, int MaxLen);
char     ConsoleGetc             (void);
void     ConsoleLock             (void);
void     ConsoleUnlock           (void);
uint32_t ConsoleGetLastInputTime (void);
void     ConsoleUpdatePrintable  (void);

#endif // _CONSOLE_H_

