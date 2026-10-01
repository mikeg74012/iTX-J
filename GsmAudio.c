/**
 **************************************************************************************************
 * @file   GsmAudio.c
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  Handles modem audio configuration and playing of tones
 *
 **************************************************************************************************
 * @attention                       (c) Ripcord Designs Limited
 **************************************************************************************************
 */

#include "global.h"


/*
 **************************************************************************************************
 *                                        PRIVATE DEFINES
 **************************************************************************************************
 */

#define AUDIO_FILETRANSFER_TIMEOUT_MS      120000

/* Maximum file size to the GSM modem (in bytes).  Limited by modem RAM size */
#define MAX_FILE_SIZE            (400 * 1024)

/*
 **************************************************************************************************
 *                                    PRIVATE MEMBER VARIABLES
 **************************************************************************************************
 */
static char AtCommand[MAX_CONSOLE_AT_LENGTH];



/*
 **************************************************************************************************
 *                                   PRIVATE FUNCTION PROTOTYPES
 **************************************************************************************************
 */

extern int  ModemFileSize;


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
/**
 * @brief   Configure the modem audio
 * @param   channel       : The audio channel to use (Normal, Headset, Speaker)
 * @param   volumePercent : The audio output level (0 to 100)
 * @retval  bool          : true on success, false on failure
 */
bool GsmAudioConfigure (AudioChannel_t channel, uint8_t volumePercent)
{
  eAtResult AtResult;

  TRACE_MODEM("Configure audio: Chan %d, Volume %d %% \n\r", channel, volumePercent);

  /* set audio channel to loudspeaker */
  snprintf(AtCommand, (size_t) MAX_CONSOLE_AT_LENGTH, "AT+QAUDCH=%d", channel);
  AtResult = gsmCommsSendAtCommand(AtCommand, DEFAULT_AT_TIMEOUT_MS);

  /* set loudspeaker volume */
  if (volumePercent > 100) volumePercent = 100;
  snprintf(AtCommand, (size_t) MAX_CONSOLE_AT_LENGTH, "AT+CLVL=%d", volumePercent);
  AtResult = gsmCommsSendAtCommand(AtCommand, DEFAULT_AT_TIMEOUT_MS);

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief   Play a single tone via the modem
 * @param   frequencyHz   : Frequency of the tone to play
 * @param   durationMs    : length of tone to play
 * @retval  bool          : true on success, false on failure
 */
bool GsmAudioSingleTone (int frequencyHz, int durationMs)
{
  eAtResult AtResult;

  TRACE_MODEM("Play tone %d Hz, %d mS\n\r", frequencyHz, durationMs);

  /* Build the command to generate the correct tone */
  snprintf(AtCommand, (size_t) MAX_CONSOLE_AT_LENGTH, "AT+QLTONE=1,%d,%d,0,%d", frequencyHz, durationMs, durationMs);

  /* send the AT commands to the modem to begin playing the tone */
  AtResult = gsmCommsSendAtCommand(AtCommand, DEFAULT_AT_TIMEOUT_MS);

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief   Play a repeating tone pattern via the modem
 * @param   mode          : 1 for start, 0 for stop play tone
*  @param   frequencyHz   : Frequency of the tone to play
 * @param   onMs          : length of each tone
 * @param   offMs         : length of pause between each tone
 * @param   repeatCount   : number of times to repeat the tone
 * @retval true on success, false on failure
 */
bool GsmAudioRepeatingTone (int mode, int frequencyHz, int onMs, int offMs, int repeatCount)
{
  eAtResult AtResult;

  TRACE_MODEM("Repeating tone %d Hz, %d mS, %d mS, %d\n\r", frequencyHz, onMs, offMs, repeatCount);

  /* Build the command to generate the correct tone pattern */
  snprintf(AtCommand, (size_t) MAX_CONSOLE_AT_LENGTH, "AT+QLTONE=%d,%d,%d,%d,%d",mode, frequencyHz, onMs, offMs, (repeatCount * (onMs + offMs)));

  /* send the AT commands to the modem to begin playing the repeating tone */
  AtResult = gsmCommsSendAtCommand(AtCommand, DEFAULT_AT_TIMEOUT_MS);

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief  Terminate the ongoing tone play
 * @param  None
 * @retval true on success, false on failure
 */
bool GsmAudioStopTones (void)
{
  TRACE_MODEM("Stop tones\r\n");

  eAtResult AtResult = gsmCommsSendAtCommand("AT+QLTONE=0", 1000);

  return ((AtResult == AT_COMPLETE) ? true : false);
}


/**
 * @brief     Play the filename in RAM: Drive of Modem
 * @param[in] file_name  : File name to be play
 * @return    bool     : true on success, false on failure
 */
bool GsmAudioPlayFile (char * file_name)
{
  eAtResult AtResult = AT_ERROR;

  /* Build the AT command */
  snprintf(AtCommand, (size_t) MAX_CONSOLE_AT_LENGTH, "at+qflst=\"RAM:%s\"", file_name);

  /* Check if the file exist */
  AtResult = gsmCommsSendAtCommand(AtCommand, AUDIO_FILETRANSFER_TIMEOUT_MS);

  if ((AtResult == AT_COMPLETE) && (ModemFileSize > 0))
  {
    snprintf(AtCommand, (size_t) MAX_CONSOLE_AT_LENGTH, "at+QAUDPLAY=\"RAM:%s\",0,100,2", file_name);

    /* Playback the file */
    AtResult = gsmCommsSendAtCommand(AtCommand, AUDIO_FILETRANSFER_TIMEOUT_MS);

    return ((AtResult == AT_COMPLETE) ? true : false);
  }
  else
  {
    return false;
  }

}

/**
 * @brief     Transfer the audio file buffer to the Modem File system
 * @param[in] maxSize  : maximum file size to download
 * @return    bool     : true on success, false on failure
 */
bool GsmAudioBufferTransferToModem (const uint8_t * pBuffer, char *fileName, uint32_t fileSize)
{
  eAtResult AtResult = AT_ERROR;

  /* Clip the file size */
  if (fileSize > MAX_FILE_SIZE)
  {
    fileSize = MAX_FILE_SIZE;
  }

  /* Build the AT command */
  snprintf(AtCommand, (size_t) MAX_CONSOLE_AT_LENGTH, "at+qfupl=\"RAM:%s\",%d",fileName, (int)fileSize);

  /* Request to upload a file to Modem */
  AtResult = gsmCommsSendAtCommand(AtCommand, DEFAULT_AT_TIMEOUT_MS);

  /* Send the file binary to thru the pBuffer*/
  if (AT_COMPLETE == AtResult)
  {
    AtResult = gsmCommsSendData((uint8_t *)pBuffer, fileSize, AUDIO_FILETRANSFER_TIMEOUT_MS);
  }

  return ((AtResult == AT_COMPLETE) ? true : false);
}

/**
 **************************************************************************************************
 *                                       PRIVATE FUNCTIONS
 **************************************************************************************************
 */
/**
 * @brief       Configure audio
 * @param[in]   argCount    : THe number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int GsmAudioConsoleConfigure (int argCount, char **pArguments)
{
  int channel;
  int volume;

  /* check for the correct number of arguments */
  if (2 == argCount)
  {
    /* get the channel and volume settings */
    sscanf(pArguments[0],  "%d", &channel);
    sscanf(pArguments[1],  "%d", &volume);

    /* check for valid data */
    if ((channel < 0) || (channel > 2) || (volume < 0) || (volume > 100))
    {
      return (CONSOLE_ARG_RANGE);
    }

    /* setup the modem audio */
    GsmAudioConfigure((AudioChannel_t)channel, (uint8_t)volume);

    return(CONSOLE_ERROR_OK);
   }

  return(CONSOLE_ARG_NO);
}


/**
 * @brief       Play an audio tone or repeating audio tones
 * @param[in]   argCount    : THe number of arguments passed from the Console
 * @param[in]   pArguments  : The list of arguments passed from the Console
 * @return      status code : CONSOLE_ERROR_OK on success, error code on failure
 */
int GsmAudioConsolePlay (int argCount, char **pArguments)
{
  int frequency;
  int onMs;
  int offMs;
  int repeatCount;
  int mode;

  /* check for the correct number of arguments */
  switch (argCount)
  {
    case 5:
       /* get the tone particulars */
       sscanf(pArguments[0],  "%d", &frequency);
       sscanf(pArguments[1],  "%d", &onMs);
       sscanf(pArguments[2],  "%d", &offMs);
       sscanf(pArguments[3],  "%d", &repeatCount);
       sscanf(pArguments[4],  "%d", &mode);
       GsmAudioRepeatingTone(mode, frequency, onMs, offMs, repeatCount);
       break;
    case 4:
       /* get the tone particulars */
       sscanf(pArguments[0],  "%d", &frequency);
       sscanf(pArguments[1],  "%d", &onMs);
       sscanf(pArguments[2],  "%d", &offMs);
       sscanf(pArguments[3],  "%d", &repeatCount);
       GsmAudioRepeatingTone(1, frequency, onMs, offMs, repeatCount);
       break;
    case 2:
      /* get the tone particulars */
      sscanf(pArguments[0],  "%d", &frequency);
      sscanf(pArguments[1],  "%d", &onMs);
      GsmAudioSingleTone(frequency, onMs);
      break;
    default:
      return(CONSOLE_ARG_NO);
  }
  return(CONSOLE_ERROR_OK);
}

