/**
 **************************************************************************************************
 * @file   console.c
 *
 * @author Ripcord Designs
 *
 * @brief  This module provides a simple Command Line Interface
 *
 **************************************************************************************************
 * @attention                   (c) Ripcord Designs Limited 2019
 **************************************************************************************************
 */
#include "global.h"


/*
 **************************************************************************************************
 *                                         PRIVATE DEFINES
 **************************************************************************************************
 */
/* token string defines */
#define MaxTokenLength          8
#define MaxTokens               8
#define MaxCommmandLength       ((MaxTokenLength + 1) * MaxTokens)
#define MaxHistory              4

/* ASCII control characters */
#define ASCII_BACKSPACE         8
#define ASCII_CR                13
#define ASCII_ESCAPE            27

/* ASCII escape characters */
#define ASCII_EC_START          91
#define ASCII_EC_CURSOR_UP      'A'
#define ASCII_EC_CURSOR_DOWN    'B'

/* ASCII escape sequence states */
#define ESCAPE_SEQUENCE_NONE    0
#define ESCAPE_SEQUENCE_PART    1
#define ESCAPE_SEQUENCE_FULL    2


/*
 * Command tree dimensions
 * - Leaves are commands that can be executed
 * - Branches are sub-tables which can be used to organise commands into groups
 */
#define MaxLeaves   16
#define MaxBranches 16

/* command table definitions */
typedef struct
{
  char  *Name;
  int  (*pFunction)(int ArgCount, char **pArguments);
  char  *HelpString;
} T_ConsoleLeaf;


typedef const struct S_ConsoleTable
{
  char                  * Name;
  T_ConsoleLeaf           Leaf  [MaxLeaves];
  const struct S_ConsoleTable * pBranch[MaxBranches];
} T_ConsoleTable;



/**
 **************************************************************************************************
 *                                       PRIVATE PROTOTYPES
 **************************************************************************************************
 */
static bool             ConsoleParseString      (T_ConsoleTable *pTable, char *CommandString);
static T_ConsoleLeaf   *FindLeaf                (T_ConsoleTable *pTable, char *LeafName);
static T_ConsoleTable  *FindBranch              (T_ConsoleTable *pTable, char *BranchName);
static void             ConsolePrintGreeting    (void);
static void             ConsolePrintVersion     (void);
static void             ConsoleBuildPrompt      (T_ConsoleTable *pTable, char *str);
static void             ConsoleDisplayError     (int errorCode);
static int              ConsoleTokenise         (char *inputLine, char **tokenList);
static void             ConsoleSetLastInputTime (void);

// System Commands
static int              ConsoleHelp             (int argCount, char **pArguments);
static int              ConsoleAbout            (int argCount, char **pArguments);
static int              ConsoleEcho             (int argCount, char **pArguments);
static int              ConsoleLs               (int argCount, char **pArguments);
static int              ConsoleBack             (int argCount, char **pArguments);


/**
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
#ifdef CONSOLE_USE_MUTEX
static SemaphoreHandle_t  ConsoleMutex;
#endif

/* Command tables containing all possible console commands */
#include "consoletables.h"

/* buffer for the current received command */
static char CommandBuffer[MaxCommmandLength];
static char HistoryBuffer[MaxHistory][MaxCommmandLength];
static int  HistoryReplayIndex = 0;
static int  HistoryNowIndex    = 0;
static int  CommandIndex       = 0;
static int  LastInputTime      = 0;

/* local echo */
static bool Echo = true;

/* current table */
static T_ConsoleTable *pCurrentTable = &ConsoleRootTable;

/* storage for the current prompt */
static char ConsolePrompt[32];

/* UART access */
static stream_t     * pStream;

static bool print_flag = false;


/**
 **************************************************************************************************
 *                                   PUBLIC SECTION / INTERFACE
 **************************************************************************************************
 */

/**
 * @brief      Initialise the Command Line Interface module
 *
 * @param      pConsoleStream : stream pointer to a serial communications port (UART, USB etc)
 * @return     None
 */
void ConsoleInitialise (stream_t * pConsoleStream)
{
  /* take a local copy of the stream pointer */
  pStream = pConsoleStream;

  /* create lock if needed */
#ifdef CONSOLE_USE_MUTEX
  ConsoleMutex = xSemaphoreCreateMutex();
  xSemaphoreGive(ConsoleMutex);
#endif

  ConsoleUpdatePrintable();
  /* Display the welcome banner */
  ConsolePrintf("\n\r");
  ConsolePrintGreeting();
  ConsolePrintVersion();
  ConsolePrintf("\n\r");

  /* Point to the root table */
  pCurrentTable = &ConsoleRootTable;
}


/**
 * @brief      Top level Console Task. Should be called regularly from main loop in order
 *             to provide a responsive user experience.
 * @param      None
 * @return     None
 */
void ConsoleTask (void const * argument)
{
  uint8_t         Data;
  static uint8_t  EscapeSequence = ESCAPE_SEQUENCE_NONE;

  /* indicate we are starting up */
  ConsolePrintf("Console - Task starting\n\r");

  /* display the prompt */
  ConsoleBuildPrompt(pCurrentTable, ConsolePrompt);
  ConsolePrintf(ConsolePrompt);

  /*
   *  Main task body
   */
  while (1)
  {
    /* get next byte, or sleep until available */
    WatchdogSetWaiting(CONSOLETASK);
    Data = (char) pStream->Get();
    WatchdogSetActive(CONSOLETASK);

    /* Record the time our last input was received.  Can be used to decide whether the system can sleep */
    ConsoleSetLastInputTime();

    /* Check for escape sequence */
    if (ESCAPE_SEQUENCE_PART == EscapeSequence)
    {
      if (ASCII_EC_START == Data)
      {
        EscapeSequence = ESCAPE_SEQUENCE_FULL;
      }
      else
      {
        EscapeSequence = ESCAPE_SEQUENCE_NONE;
      }
    }
    else
    {
      switch (Data)
      {
      case ASCII_BACKSPACE:
        if (CommandIndex > 0)
        {
          /* if it is a backspace, remove the last character from the buffer */
          CommandIndex--;
           /* clear the character from the display */
          if (true == Echo)
          {
            pStream->Put((char)8);
            pStream->Put(' ');
            pStream->Put((char)8);
          }
        }
        break;
      case ASCII_CR:
        /* return, the process the command */
        ConsolePrintf("\n\r");
        CommandBuffer[CommandIndex] = (char)0;
        strncpy(&HistoryBuffer[HistoryNowIndex][0], &CommandBuffer[0], MaxCommmandLength);
        HistoryNowIndex = (HistoryNowIndex + 1) % MaxHistory;
        HistoryReplayIndex = HistoryNowIndex;
        CommandIndex = 0;
        /* parse the command */
        ConsoleParseString(pCurrentTable, &CommandBuffer[0]);
        ConsolePrintf(ConsolePrompt);
        break;
      case ASCII_ESCAPE:
        /* escape sequence started */
        EscapeSequence = ESCAPE_SEQUENCE_PART;
        break;
      default:
        if (ESCAPE_SEQUENCE_FULL == EscapeSequence)
        {
          /* wipe out what we were doing before */
          while (CommandIndex > 0)
          {
            CommandIndex--;
            if (true == Echo)
            {
              pStream->Put((char)8);
              pStream->Put(' ');
              pStream->Put((char)8);
            }
          }
          switch ((int)Data)
          {
          case ASCII_EC_CURSOR_UP:
            if (0 < HistoryReplayIndex)
            {
              HistoryReplayIndex--;
            }
            else
            {
              HistoryReplayIndex = (MaxHistory - 1);
            }
            strncpy(&CommandBuffer[0], &HistoryBuffer[HistoryReplayIndex][0], MaxCommmandLength);
            CommandBuffer[MaxCommmandLength-1] = '\0';
            ConsolePrintf(&CommandBuffer[0]);
            CommandIndex = strlen(&CommandBuffer[0]);
            break;
          case ASCII_EC_CURSOR_DOWN:
            if (HistoryNowIndex != HistoryReplayIndex)
            {
              HistoryReplayIndex = (HistoryReplayIndex + 1) % MaxHistory;
            }
            strncpy(&CommandBuffer[0], &HistoryBuffer[HistoryReplayIndex][0], MaxCommmandLength);
            CommandBuffer[MaxCommmandLength-1] = '\0';
            ConsolePrintf(&CommandBuffer[0]);
            CommandIndex = strlen(&CommandBuffer[0]);
            break;
          }
          EscapeSequence = ESCAPE_SEQUENCE_NONE;
        }
        else
        {
          /*
           * add the data to the command buffer
           * (if there is room!)
           */
          if (CommandIndex < MaxCommmandLength)
          {
            CommandBuffer[CommandIndex++] = Data;
          }
          /* echo the character to screen if necessary */
          if (true == Echo)
          {
            pStream->Put(Data);
          }
        }
      }
    }
  }
}

/**
 * @brief     update the printable flag from Config and TestMode
 * @param     none
 * @param     none
 * @return    none
 */
void ConsoleUpdatePrintable()
{
  print_flag = (ConfigGetFlag(CONFIG_FLAG_CONSOLE) || TestModeGet());
}

/**
 * @brief    This function prints a string to the console.  It takes standard printf parameters
 * @param      format :  The format string
 * @param      ...    :  The list of data to print
 * @return     none
 */
void ConsolePrintf (const char *format, ...)
{
  va_list argptr;
  static char printf_str[256];  /* static so we use heap rather than stack */

#ifndef DEBUG
  if (print_flag)
#endif
  {
    /* initialise the list of arguments */
    va_start(argptr, format);

    vsprintf(printf_str, format, argptr);

    /* Output the string to the console */
    pStream->Send((uint8_t *) printf_str);

    /* orderly shutdown */
    va_end(argptr);
  }
}


/**
 * @brief       Get a null terminated string from the command line interface.  Will copy
 *              up to buffSize characters max, or stop at a null character.
 * @param[out]  pStr     :  Storage for the returned string
 * @param[in]   buffSize :  the buffer size available in pStr
 * @return      none
 */
void ConsoleGets (char *pStr, int buffSize)
{
  uint8_t data;

  while (buffSize > 0)
  {
    data = tolower(pStream->Get());
    if ('\r' == data)
    {
      *pStr = 0;
      return;
    }
    else
    {
      *pStr++ = data;

      /* echo the character to screen if necessary */
      if (true == Echo)
        pStream->Put(data);
      buffSize--;
    }
  }

   *pStr = 0;
}


/**
 * @brief       Get a single character from the command line interface.
 * @param       none
 * @return      none
 */
char ConsoleGetc (void)
{
  return (tolower(pStream->Get()));
}


/**
 * @brief       Provides command line help to the operator.  Called from the Console, it may include
 *              command line arguments to specify which Console command help is required for.
 * @param[in]   argCount    : THe number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int ConsoleHelp (int argCount, char **pArguments)
{
  T_ConsoleLeaf *pLeaf;

  /* general help? */
  if (0 == argCount)
  {
    /* print the help */
    ConsolePrintf("Usage: Help <command>\n\r");
  }
  else
  {
    /* find the command requested and print the help string */
    pLeaf = FindLeaf(pCurrentTable, pArguments[0]);
    if (NULL == pLeaf) pLeaf = FindLeaf(&ConsoleRootTable, pArguments[0]);
    if (NULL != pLeaf)
    {
      ConsolePrintf(pLeaf->HelpString);
      ConsolePrintf("\n\r");
    }
    else
    {
      ConsolePrintf("No such command\n\r");
    }
  }
  return (CONSOLE_ERROR_OK);
 }


/**
 * @brief       Print the system greeting message to the console.
 * @param[in]   argCount    : THe number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int ConsoleAbout (int argCount, char **pArguments)
{
  ConsolePrintf("\n\r");
  ConsolePrintGreeting();
  ConsolePrintVersion();
  ConsolePrintf("HW Version: %s\n\r", HW_VERSION_NAME(GetHardwareVersion()));
  ConsolePrintf("\n\r");
  return(CONSOLE_ERROR_OK);
}


/**
 * @brief       Turn echo on or off.  When echo is on, characters typed by the
 *              user are echoed back.
 * @param[in]   argCount    : THe number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int ConsoleEcho (int argCount, char **pArguments)
{
  if (1 == argCount)
  {
    if (!strcmp(pArguments[0], "on"))
    {
      Echo = true;
      ConsolePrintf("echo on - OK\n\r");
    }
    if (!strcmp(pArguments[0], "off"))
    {
      Echo = false;
      ConsolePrintf("echo off - OK\r\n");
    }
  }
  else
  {
    ConsolePrintf("Usage: echo <on/off>\n\r");
  }
  return(CONSOLE_ERROR_OK);
 }


/**
 * @brief       This function sets the Console current table to the parent table, therefore traversing one
 *              level up the tree, or "back".
 * @param[in]   argCount    : THe number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int ConsoleBack (int argCount, char **pArguments)
{
  pCurrentTable = &ConsoleRootTable;
  ConsoleBuildPrompt(pCurrentTable, ConsolePrompt);

  return (CONSOLE_ERROR_OK);
}


/**
 * @brief       List the available commands.  This depends on the current Console table.
 * @param[in]   argCount    : THe number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int ConsoleLs (int argCount, char **pArguments)
{
  unsigned int Count;

  /* print the names of all commands at this level */
  for (Count = 0; Count < MaxLeaves; Count++)
  {
    if (NULL == pCurrentTable->Leaf[Count].pFunction) break;
    if ((0 != Count) && (0 == (Count % 6)))
    {
      ConsolePrintf("\n\r");
    }
    ConsolePrintf("  ");
    ConsolePrintf(pCurrentTable->Leaf[Count].Name);
  }

  /* print the names of all branches */
  for (Count = 0; Count < MaxBranches; Count++)
  {
    if (NULL == pCurrentTable->pBranch[Count]) break;
    if (0 == (Count % 6))
    {
      ConsolePrintf("\n\r");
    }
    ConsolePrintf("  <");
    ConsolePrintf(pCurrentTable->pBranch[Count]->Name);
    ConsolePrintf(">");
  }

  ConsolePrintf("\n\r");
  return (CONSOLE_ERROR_OK);
}


void ConsoleLock (void)
{
#ifdef CONSOLE_USE_MUTEX
  xSemaphoreTake(ConsoleMutex, portMAX_DELAY);
#endif
}


void ConsoleUnlock (void)
{
#ifdef CONSOLE_USE_MUTEX
  xSemaphoreGive(ConsoleMutex);
#endif
}


/**
 * @brief      Get the value of the "LastInputTime" member variable. This is the time of the
 *             last user input on the console (does not include output to console)
 * @param      None
 * @return     Time stamp of the last user input
 */
uint32_t ConsoleGetLastInputTime (void)
{
  return LastInputTime;
}


/*
 **************************************************************************************************
 *                                        PRIVATE SECTION
 **************************************************************************************************
*/
/**
 * @brief     Separate line into space-separated tokens
 * @param     The console line to parse
 * @param     The token structure to populate
 * @return    The Number of tokens found
 */
static int ConsoleTokenise (char *inputLine, char **tokenList)
{
  int noOfTokens = 0;
  bool handlingToken = false;

  for (int i = 0; i < MaxCommmandLength; i++)
  {
    if (inputLine[i] == 0)
    {
      /* Reached end of console line */
      break;
    }
    else if (inputLine[i] == ' ')
    {
      /* Space */
      if (handlingToken)
      {
        /* We have reached the end of the token */
        handlingToken = false;
        /* Put terminator in place of space */
        inputLine[i] = 0;
      }
      /* If we weren't handling a token ignore this space and continue */
    }
    else
    {
      /* Non-space */
      if (!handlingToken)
      {
        /* We are starting a new token */
        if (noOfTokens == MaxTokens)
        {
          break;
        }
        else
        {
          handlingToken = true;
          tokenList[noOfTokens] = &inputLine[i];
          noOfTokens++;
        }
      }
      /* If we are already handling a token ignore this character and continue */
    }
  }
  return noOfTokens;
}


/**
 * @brief       This function parses a string received from the console.  The current table is searched
 *              for a command match, and the command is processed if a match is found.  If no match is
 *              found then we search for a sub-table match.
 * @param[in]   pTable        : THe current table to start the search with
 * @param[in]   CommandString : The command string to search for
 * @return      bool          : true if table entry found, false if not found
 */
static bool ConsoleParseString (T_ConsoleTable *pTable, char *CommandString)
{
  int         TokenCount;
  T_ConsoleLeaf  *pLeaf;
  T_ConsoleTable *pBranch;
  char       *TokenList[MaxTokens];
  int         StringLength;
  int         ErrorCode;

  StringLength = strlen(CommandString);

  /* break the console input into tokens */
  TokenCount = ConsoleTokenise(CommandString, TokenList);

  if (TokenCount > 0)
  {
    /* look for leaf */
    pLeaf = FindLeaf(pTable, TokenList[0]);
    if (NULL == pLeaf) pLeaf = FindLeaf(&ConsoleRootTable, TokenList[0]);
    if (NULL != pLeaf)
    {
      /* execute the command */
      ErrorCode = pLeaf->pFunction(TokenCount-1, &TokenList[1]);
      if (CONSOLE_ERROR_OK != ErrorCode)
      {
        ConsoleDisplayError(ErrorCode);
      }

      /* indicate success */
      return(true);
    }

    /* look for branch */
    pBranch = FindBranch(pTable, TokenList[0]);
    if (NULL != pBranch)
    {
      /*
       * If there are arguments, pass the remainder of the command into
       * the new table without actually traversing into the new table.
       * i.e. execute from the current level
       */
      if (StringLength > strlen(TokenList[0]))
      {
        ConsoleParseString(pBranch, CommandString + strlen(TokenList[0]) + 1);
      }
      else
      {
        /* there are no arguments just traverse into the new table */
        pCurrentTable = pBranch;
        ConsoleBuildPrompt(pCurrentTable, ConsolePrompt);
      }

      /* indicate success */
      return(true);
    }

    /* indicate failure */
    ConsolePrintf("Unknown command ");
    ConsolePrintf(TokenList[0]);
    ConsolePrintf("\n\r");
  }
  return(false);
}


/**
 * @brief       Search the specified table for the specified leaf node by name, and return a pointer to
 *              the leaf node.
 * @param[in]   pTable      : The table to search
 * @param[in]   LeafName    : The name of the leaf node to search for
 * @return      T_ConsoleLeaf * : pointer to the leaf node, or null if not found
 */
static T_ConsoleLeaf *FindLeaf (T_ConsoleTable *pTable, char *LeafName)
{
  uint8_t Count;

  /* search leaf nodes for a match */
  for (Count = 0; Count < MaxLeaves; Count++)
  {
    if (NULL == pTable->Leaf[Count].pFunction) break;
    if (!strcmp(pTable->Leaf[Count].Name, LeafName))
    {
      /* return pointer to the leaf */
      return ((T_ConsoleLeaf *) &pTable->Leaf[Count]);
    }
  }
  return(NULL);
}



/**
 * @brief       Serach the specified table for the specified branch node by name, and return a pointer to
 *              the branch node.
 * @param[in]   pTable      : The table to search
 * @param[in]   LeafName    : The name of the branch node to search for
 * @return      T_ConsoleLeaf * : pointer to the branch node, or null if not found
 */
static T_ConsoleTable *FindBranch (T_ConsoleTable *pTable, char *BranchName)
{
  uint8_t Count;

  /* search branches for a match */
  for (Count = 0; Count < MaxBranches; Count++)
  {
    if (NULL == pTable->pBranch[Count]) break;
    if (!strcmp(pTable->pBranch[Count]->Name, BranchName))
    {
      /* indicate success */
      return(pTable->pBranch[Count]);
    }
  }
  return(NULL);
}


/**
 * @brief       This function prints a greeting message to the console
 * @param[in]   none
 * @return      none
 */
static void ConsolePrintGreeting (void)
{
  ConsolePrintf(CONSOLE_GREETING_LINE1);
  ConsolePrintf(CONSOLE_GREETING_LINE2);
  ConsolePrintf(CONSOLE_GREETING_LINE3);
  ConsolePrintf(CONSOLE_GREETING_LINE4);
  ConsolePrintf(CONSOLE_GREETING_LINE5);
}


/**
 * @brief       This function prints the version string to the console
 * @param[in]   none
 * @return      none
 */
static void ConsolePrintVersion (void)
{
  ConsolePrintf("Firmware Version ");
  ConsolePrintf(FIRMWARE_VERSION);
  ConsolePrintf("\n\r");
  ConsolePrintf("FreeRTOS Version ");
  ConsolePrintf(tskKERNEL_VERSION_NUMBER);
  ConsolePrintf("\n\r");
}


/**
 * @brief       Print the specified error code to screen in stringified form
 * @param[in]   errorCode :  The error code to display
 * @return      none
 */
static void ConsoleDisplayError (int errorCode)
{
  switch (errorCode)
  {
    case CONSOLE_ERROR_OK:
      ConsolePrintf("OK\n\r");
      break;
    case CONSOLE_CMD_UNKNOWN:
      ConsolePrintf("Unknown Command\n\r");
      break;
    case CONSOLE_ARG_NO:
      ConsolePrintf("Wrong number of arguments\n\r");
      break;
    case CONSOLE_ARG_RANGE:
      ConsolePrintf("Argument out of range\n\r");
      break;
    default:
      ConsolePrintf("Unknown Error: 0x%x\n\r", errorCode);
  }
}


/**
 * @brief       This function creates a prompt string from the current table name and path
 * @param[in]   pTable :  pointer to the current table
 * @param[out]  str    :  buffer to hold the generated prompt
 * @return      none
 */
static void ConsoleBuildPrompt (T_ConsoleTable *pTable, char *str)
{
  *str = (char)0;
  strcat(str, pTable->Name);
  strcat(str, ">");
}


/**
 * @brief      Store the current time to the "LastInputTime" member variable. This is the time of the
 *             last user input on the console (does not include output to console).  Can be used by the
 *             system to determine whether it is OK to sleep.
 * @param      None
 * @return     None
 */
static void ConsoleSetLastInputTime (void)
{
  LastInputTime = RtcGetTimeStamp();
}

