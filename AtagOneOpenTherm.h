
//#include <ESP8266WiFi.h>

#include <Timezone.h>           // https://github.com/JChristensen/Timezone
#include <TimeLib.h>            // https://github.com/PaulStoffregen/Time
#include <TelnetStream.h>       // https://github.com/jandrassy/TelnetStream/commit/1294a9ee5cc9b1f7e51005091e351d60c8cddecf
#include "Debug.h"
#include "networkStuff.h"

#define SETTINGS_FILE   "/settings.ini"
#define CMSG_SIZE        512
#define JSON_BUFF_MAX   1024

bool      Verbose = false;
char      cMsg[CMSG_SIZE];
char      fChar[10];
String    lastReset   = "";
uint32_t  ntpTimer    = millis() + 30000;
char      settingHostname[41];
uint32_t   WDTfeedTimer;

const char *weekDayName[]  {  "Unknown", "Zondag", "Maandag", "Dinsdag", "Woensdag"
                            , "Donderdag", "Vrijdag", "Zaterdag", "Unknown" };
const char *flashMode[]    { "QIO", "QOUT", "DIO", "DOUT", "Unknown" };

#define KEEP_ALIVE_PIN    13  //--- GPIO-13 / PIN-14 / D7
#define LED_BLUE           2  //--- GPIO-02 / PIN-3  / D4
#define LED_RED_B          0  //--- GPIO-00 / PIN-4  / D3
#define LED_RED_C         12  //--- GPIO-12 / PIN-13 / D6

#define FEEDTIME      2500

// eof
