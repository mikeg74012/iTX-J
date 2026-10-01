/**
 **************************************************************************************************
 * @file   ConsoleTables.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  User commands for the console.  Add new console commands here
 *
 *         NOTE: You do NOT need to include this file in your module
 *
 **************************************************************************************************
 * @attention                     (c) Ripcord Designs Limited 2019
 **************************************************************************************************
 */

#ifndef _CONSOLETABLES_H_
#define _CONSOLETABLES_H_


/* Version Information */
#define FIRMWARE_VERSION        FIRMWARE_VERSION_STRING

/* Greeting Message */
#define CONSOLE_GREETING_LINE1  "*************************************\n\r"
#define CONSOLE_GREETING_LINE2  "** iTelematiX v4 Telematics Device **\n\r"
#ifdef DEBUG
#define CONSOLE_GREETING_LINE3  "**           DEBUG BUILD           **\n\r"
#else
#define CONSOLE_GREETING_LINE3  "**          RELEASE BUILD          **\n\r"
#endif
#define CONSOLE_GREETING_LINE4  "*************************************\n\r"
#define CONSOLE_GREETING_LINE5  ""


/*
 **************************************************************************************************
 *                                      USER COMMAND TABLES
 **************************************************************************************************
 *
 * Additional command tables should be added here.
 * Related commands should be grouped together into tables and given a meaningful
 * name to access them by.
 *
 * NOTE: Remember to also add the name of your table to the "root" table below
 *
 * WARNING - All tables MUST end with a blank / NULL entry
 *
 **************************************************************************************************
 */
T_ConsoleTable const ConsoleUARTTable =
{
  "uart",
  {
    {"stats",  UARTConsoleStats,  "Display the current UART stats.  Usage: stats"},
    {"clear",  UARTConsoleClear,  "Clears the stats.  Usage: clear"},
    {"",       NULL,              ""}
  },
  // No sub-tables
  {NULL}
};


T_ConsoleTable const ConsoleImuTable =
{
  "imu",
  {
    {"spoof",       MotionMonitorConsoleSpoof,        "Spoof a device event.  Usage: spoof <dhm/soj/eoj/impact>"},
    {"events",      EventStoreConsoleList,            "Print event data.  Usage: events"},
    {"data",        MotionMonitorConsoleShowData,     "Show imu data.  Usage: data"},
    {"stream",      MotionMonitorConsoleStream,       "Stream imu data.  Usage: stream <true/false>"},
    {"xyz",         XyzConsoleXyz,                    "Print xyz orientation data.  Usage: xyz"},
    {"allowsleep",  MotionMonitorConsoleAllowSleep,   "Enable / Disable sleep.  Usage: allowsleep <true/false>"},
    {"hibernate",   MotionMonitorConsoleHibernate,    "Put the device into hibernation.  Usage: hibernate"},
    {"peek",        AccelDriverConsolePeek,           "Read a register value.  Usage: peek <address>"},
    {"poke",        AccelDriverConsolePoke,           "Write a register value.  Usage: poke <address> <data>"},
    {"read",        AccelDriverConsoleRead,           "Read multiple bytes from accel.  Usage: read <address> <length>"},
    {"calcs",       DetectEventConsoleCalcs,          "Stream event calculations.  Usage: calcs <true/false>"},
    {"seq",         SequencerConsoleShowState,        "Show sequencer state.  Usage: seq"},
    {"dhm",         MotionMonitorConsoleDhmTime,      "Show last DHM time.  Usage: dhm"},
    {"rts",         MotionMonitorConsoleReadyToSleep, "Check ReadyToSleep and display details.  Usage: rts"},
    {"mini",        MotionMonitorConsoleFakeMiniEvent,"Generate a fake mini-event.  Usage: mini <length>"},
    {"",            NULL,                             ""}
  },
  // No sub-tables
  {NULL}
};


T_ConsoleTable const ConsoleBatteryTable =
{
  "battery",
  {
    {"log",    BatteryMonitorConsoleLog,    "Turn battery logging on / off.  Usage: log <on/off>"},
    {"status", BatteryMonitorConsoleStatus, "Display battery status.  Usage: status"},
    {"peek",   GasGaugeConsolePeek,         "Read a gas gauge register value.  Usage: peek <address>"},
    {"poke",   GasGaugeConsolePoke,         "Write a gas gauge register value.  Usage: poke <address> <data>"},
    {"sync",   BatteryMonitorConsoleSync,   "Force the battery to sync with the charging curve"},
    {"",       NULL,              ""}
  },
  // No sub-tables
  {NULL}
};


T_ConsoleTable const ConsoleChargerTable =
{
  "charger",
  {
    {"status",  ChargerConsoleStatus,  "Display charger status.  Usage: status"},
    {"enable",  ChargerConsoleEnable,  "Enable the charger.  Usage: enable"},
    {"disable", ChargerConsoleDisable, "Disable the charger.  Usage: disable"},
    {"peek",    ChargerConsolePeek,    "Read a register value.  Usage: peek <address>"},
    {"poke",    ChargerConsolePoke,    "Write a register value.  Usage: poke <address> <data>"},
    {"",        NULL,                  ""}
  },
  // No sub-tables
  {NULL}
};


T_ConsoleTable const ConsoleConfigTable =
{
  "config",
  {
    {"default",  ConfigConsoleDefault, "Reset config to default values.  Usage: default"},
    {"get",      ConfigConsoleGet,     "Get a config value.  Usage: get <config item>"},
    {"set",      ConfigConsoleSet,     "Set a config value.  Usage: set <config item> <value>"},
    {"sms",      ConfigConsoleSMS,     "Set config using SMS string.  Usage: sms <sms-config-string>"},
    {"tcp",      GsmConfigConsoleTCP,  "Set config using TCP string.  Usage: tcp <tcp-config-string>"},
    {"",         NULL,                 ""}
  },
  // No sub-tables
  {NULL}
};


T_ConsoleTable const ConsoleRtcTable =
{
  "rtc",
  {
    {"default",  RtcConsoleDefault,    "Reset RTC to default time and date.  Usage: default"},
    {"get",      RtcConsoleGet,        "Get the current time and date.  Usage: get"},
    {"set",      RtcConsoleSet,        "Set the current time and date.  Usage: set dd-mm-yyyy hh:mm:ss wday(optional)"},
    {"ts",       RtcConsoleTimeStamp,  "Get the current timestamp (seconds since 1/1/2000.  Usage: ts"},
    {"",         NULL,                 ""}
  },
  // No sub-tables
  {NULL}
};


T_ConsoleTable const ConsoleModemTable =
{
  "modem",
  {
    {"power",    ModemConsolePower,         "Power modem on or off.  Usage: power <on / off>"},
    {"at",       ModemConsoleSendAtCommand, "Send an AT command to the modem.  Usage: at <command string>"},
    {"smsrx",    ModemConsoleSmsRx,         "Trigger the SMS receive process.  Usage: smsrx"},
    {"smstx",    ModemConsoleSmsTx,         "Send an SMS message to the default number.  Usage: smstx <message>"},
    {"call",     GsmVoiceConsoleCall,       "Make a voice call.  Usage: call <number>"},
    {"ble",      GsmBleConsoleScan,         "Turn BLE scanning on or off.  Usage: ble <on / off / list>"},
    {"duration", GsmVoiceConsoleCallDur,    "Get the current GSM call durations"},
    {"cstatus",  GsmVoiceConsoleCallStatus, "Get the current GSM Call Status"},
    {"upload",   ModemConsoleFileUpload,    "Upload buffer content to the Modem.  Usage: upload"},
    {"playfile", ModemConsoleFilePlay,      "Play the file that uploaded to Modem.  Usage: playfile"},
    {"status",   ModemConsoleStatus,        "Display modem status.  Usage: status"},
    {"test",     ModemConsoleTest,          "Test modem functions.  Usage: test <n> "},
    {"",         NULL,                      ""}
  },
  // No sub-tables
  {NULL}
};


T_ConsoleTable const ConsoleHttpTable =
{
  "http",
  {
    {"config",  GsmHttpConsoleConfigure,  "Configure HTTP / HTTPS.  Usage: config "},
    {"seturl",  GsmHttpConsoleSetUrl,     "Set URL of the HTTP server to access.  Usage: seturl <url>"},
    {"post",    GsmHttpConsolePost,       "POST message to HTTP server.  Usage: post <message>"},
    {"get",     GsmHttpConsoleGet,        "Send GET request to HTTP server.  Usage: get"},
    {"read",    GsmHttpConsoleRead,       "READ response from HTTP server.  Usage: read"},
    {"tcptest", GsmHttpConsoleTest,       "Test response from HTTP server.  Usage: tcptest"},
    {"tcpget",  GsmHttpConsoleFlag,       "Check server connected flag.  Usage: tcpget"},

    {"",        NULL,                     ""}
  },
  // No sub-tables
  {NULL}
};


T_ConsoleTable const ConsoleGnssTable =
{
  "gnss",
  {
    {"list",    GnssEventConsoleList,     "List stored GNSS event data.  Usage: list"},
    {"start",   GsmGnssConsoleStart,      "Start the GNSS receiver.  Usage: start"},
    {"stop",    GsmGnssConsoleStop,       "Stop the GNSS receiver.  Usage: stop"},
    {"read",    GsmGnssConsoleRead,       "Read location, speed and heading.  Usage: read"},
    {"ref",     GsmGnssConsoleSetRef,     "Set reference Lat/Long for faster TTFF. Usage: ref <lat> <long>"},
    {"status",  GsmGnssConsoleStatus,     "Display GNSS status info. Usage: status"},
    {"jam",     GsmGnssConsoleJammer,     "Display GNSS Jammer info. Usage: jam <on> <off> <now> <speed value> "},
    {"",        NULL,                     ""}
  },
  // No sub-tables
  {NULL}
};


T_ConsoleTable const ConsoleAudioTable =
{
  "audio",
  {
    {"config",  GsmAudioConsoleConfigure,  "Configure modem audio.  Usage: config <chan> <volume %%>"},
    {"play",    GsmAudioConsolePlay,       "Play tone or repeating tone.  Usage: play <freq> <on mS> <off mS> <repeat>"},
    {"tone",    GsmAudioConsolePlay,       "Stop/play tone or repeating tone.  Usage: tone <freq> <on mS> <off mS> <repeat> <0/1>"},
    {"",        NULL,                      ""}
  },
  // No sub-tables
  {NULL}
};


T_ConsoleTable const ConsoleOtaTable =
{
  "ota",
  {
    {"upgradenow",  OTAImageConsoleUpgradeNow,      "Start firmware upgrade. Usage: upgradenow"},
    {"setinfo",     OTAImageConsoleSetInfo,         "Write manifest data to SRAM.  Usage: setinfo <filesize> <crc>"},
    {"getinfo",     OTAImageConsoleGetInfo,         "Read manifest data from SRAM.  Usage: getinfo"},
    {"verify",      OTAImageConsoleVerify,          "Verify the OTA Image in SPI SRAM. Usage: verify"},
    {"manifest",    OtaHttpConsoleGetManifest,      "Get manifest file via HTTP. Usage: manifest"},
    {"image",       OtaHttpConsoleGetImageFile,     "Get upgrade image file via HTTP. Usage: image"},
    {"process",     OTAImageConsoleProcessManifest, "Process the manifest file. Usage: process"},
    {"updatebl",    OTAImageConsoleUpdateBL,        "Start boot loader upgrade. Usage: updatebl"},
    {"",            NULL,                           ""}
  },
  // No sub-tables
  {NULL}
};


T_ConsoleTable const ConsoleRamTable =
{
  "ram",
  {
    {"clear",  SerialRamConsoleClear,  "Clears RAM address range.  Usage: clear <addr> <len> "},
    {"read",   SerialRamConsoleRead,   "Read RAM address range.  Usage: read <addr> <len>"},
    {"test",   SerialRamConsoleTest,   "Test RAM read/write.  Usage: test"},
    {"qlist",  MessageQueueConsoleList,"List the message queue with type (e.g. 2:HB 3:SOJ etc, in RAM. Usage qlist <type> "},
    {"",       NULL,              ""}
  },
  // No sub-tables
  {NULL}
};


T_ConsoleTable const ConsoleUserInterfaceTable =
{
  "ui",
  {
    {"led",     LedConsoleLed,       "Control the LEDs.  Usage: led <red/green/test/off>"},
    {"set",     GpioConsoleSet,      "Set GPIO pin state. Usage: set <port><pin no>=0/1"},
    {"get",     GpioConsoleGet,      "Get GPIO pin state. Usage: get <port><pin no>"},
    {"rmp",     FitmentConsoleRmpConfig,   "Update rotation matrix operation.  Usage: rmp <state/list/on/off>"},
    {"hw",      SetHardwareControlData,           "Set hardware type.  Usage: hw <Hex N>"},
    {"",        NULL,                ""}
  },
  // No sub-tables
  {NULL}
};


T_ConsoleTable const ConsoleWifiTable =
{
  "wifi",
  {
    {"prog",    WifiConsoleProg,     "Put WiFi modem into programming mode.  Usage: prog"},
    {"start",   WifiConsoleStart,    "Start the WiFi modem.  Usage: start"},
    {"stop",    WifiConsoleStop,     "Stop the WiFi modem.  Usage: stop"},
    {"fix",     WifiConsoleFix,      "Test program to put Wifi chips to low current.  Usage: fix"},
    {"list",    WifiConsoleList,     "List saved WiFi MAC/RSSI. Or add MAC to list.  Usage: list /<add> <rssi> <mac>"},
    {"",        NULL,                ""}
  },
  // No sub-tables
  {NULL}
};


T_ConsoleTable const ConsoleLogTable =
{
  "log",
  {
    {"list",  StatusLogConsoleList,   "List current error codes.  Usage: list"},
    {"clear", StatusLogConsoleClear,  "Clear current error codes.  Usage: clear"},
    {"add",   StatusLogConsoleAdd,    "Add a number of error codes.  Usage: add <no of events>"},
    {"rem",   StatusLogConsoleRemove, "Remove a number of error codes.  Usage: rem <no of events>"},
    {"",      NULL,                ""}
  },
  // No sub-tables
  {NULL}
};



/*
 **************************************************************************************************
 *                                       CONSOLE ROOT TABLE
 **************************************************************************************************
 *
 * Each sub-table should have an entry here
 * WARNING - All tables MUST end with a blank / NULL entry
 *
 **************************************************************************************************
 */
T_ConsoleTable const ConsoleRootTable =
{
  "root",
  {
    /*
     * System commands - do not edit this section
     */
    {"help",   ConsoleHelp,            "This is it!  Usage: help"},
    {"about",  ConsoleAbout,           "Provides version information.  Usage: about"},
    {"echo",   ConsoleEcho,            "Turn echo on or off.  Usage: echo <on/off>"},
    {"ls",     ConsoleLs,              "List available commands.  Usage: ls" },
    {"stack",  SystemConsoleStack,     "Show stack usage.  Usage: stack" },
    {"reset",  SystemConsoleReset,     "Reset the device.  Usage: reset" },
    {"stats",  StatisticsConsolePrint, "Show device statistics.  Usage: stats" },
    {"..",     ConsoleBack,            "Traverse back up the tree.  Usage: .." },
    {"",       NULL,                   ""}
  },
  {
    /*
     * Add new user command tables here.
     * List must end with NULL
     */
    &ConsoleUARTTable,
    &ConsoleRtcTable,
    &ConsoleImuTable,
    &ConsoleBatteryTable,
    &ConsoleChargerTable,
    &ConsoleConfigTable,
    &ConsoleModemTable,
    &ConsoleHttpTable,
    &ConsoleGnssTable,
    &ConsoleAudioTable,
    &ConsoleOtaTable,
    &ConsoleRamTable,
    &ConsoleUserInterfaceTable,
    &ConsoleWifiTable,
    &ConsoleLogTable,
    NULL
  }
};


#endif /* _CONSOLETABLES_H_ */
