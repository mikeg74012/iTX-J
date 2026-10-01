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

#ifndef _GSMAUDIO_H_
#define _GSMAUDIO_H_


/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */
typedef enum
{
  AUDIO_CHANNEL_NORMAL      = 0,
  AUDIO_CHANNEL_HEADSET     = 1,
  AUDIO_CHANNEL_LOUDSPEAKER = 2,
} AudioChannel_t;


/**
 **************************************************************************************************
 *                                  PUBLIC FUNCTIONS / INTERFACE
 **************************************************************************************************
 */
/* API */
bool GsmAudioConfigure                 (AudioChannel_t channel, uint8_t volumePercent);
bool GsmAudioSingleTone                (int frequencyHz, int durationMs);
bool GsmAudioRepeatingTone             (int mode, int frequencyHz, int onMs, int offMs, int repeatCount);
bool GsmAudioStopTones                 (void);
bool GsmAudioPlayFile                  (char * file_name);
bool GsmAudioBufferTransferToModem     (const uint8_t * pBuffer, char * file_name, uint32_t fileSize);

/* Console Commands */
int GsmAudioConsoleConfigure            (int argCount, char **pArguments);
int GsmAudioConsolePlay                 (int argCount, char **pArguments);
int GsmAudioConsoleFileUpload           (int argCount, char **pArguments);
int GsmAudioConsoleFilePlay             (int argCount, char **pArguments);


#endif /* _GSMAUDIO_H_ */
