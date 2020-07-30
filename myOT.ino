#include "timing.h"

// Wemos D1 on 1of!-Wemos board

#define THERMOSTAT_IN   16
#define THERMOSTAT_OUT   4
#define BOILER_IN        5
#define BOILER_OUT      14

// temparatures 

#define TEMP_CH_MAX         32.0 // maximum temperature for CH water -- 40.0 max

#define TEMP_CH_BASE_LINE   25.0 // minimum temperature for CH water

#define TEMP_OAT_BASE_LINE  20.0 // OAT below which water temp will increase
#define TEMP_BASE_FACTOR     1.0 // factor 

int8_t mode=0;
bool onoffOverride = false;
int lastOnOffOverride = 0;

#define MODE_LISTEN_MASTER  0
#define MODE_GET_MESSAGE_FROM_MASTER 2
#define MODE_FORWARD_MESSAGE_TO_SLAVE 3

#define MODE_LISTEN_SLAVE   1
#define MODE_GET_MESSAGE_FROM_SLAVE 4
#define MODE_FORWARD_MESSAGE_TO_MASTER 5

#define MODE_ERROR 9

struct override {
    int8_t  msgNr;
    float   newValue;
    bool    enabled;
}; 

struct override OverRides[] = { 
  {
    OT_MSGID_CH_SETPOINT, 42.1, false
  },
  {
    OT_MSGID_ROOM_SETPOINT, 20.1, false
  },
  {
    OT_MSGID_ROOM_TEMP, 20.1, false
  }
};

int nrOverRides = sizeof(OverRides)/sizeof(struct override);

struct binfo {
    int     status;             // 0
    float   modulation_level;   // 17
    float   ch_pres;            // 18
    float   Tboiler;            // 25
    float   Tdhw;               // 26
    float   Toutside;           // 27
    float   Tret;               // 28
    int8_t  DHWLow, DHWHigh;    // 48
    int8_t  CHLow, CHHigh;      // 49
} binfo;

struct bcmd {
    float   Tset;       // 1
    float   MaxRelMod;  // 14
    float   TrSet;      // 16
    float   Tr;         // 24
    float   TdhwSet;    // 56
} bcmd;

OpenthermData busMessage;
OpenthermData ownMessage;
OpenthermData message;

int8_t messageSource = 0;

#define OT_MSG_SOURCE_BUS 0
#define OT_MSG_SOURCE_SELF 1

#define boilerOn() switchBoiler(1)
#define boilerOff() switchBoiler(0)

#define DumpF(txt) sprintf(msgTxt, "%20.20s %6.2f", txt, data.f88());
#define DumpI(txt) sprintf(msgTxt, "%20.20s %6d",   txt, data.u16());
void DumpX(char *str, OpenthermData &data)
{
    Debugf("[ %s HB -> ", str);

    // valueHB

    if ( data.valueHB & B00000001)
        Debug ("CH ");
    if ( data.valueHB & B00000010)
        Debug ("DHW ");
        
    // valueLB

    if ( data.valueLB)
        Debugf(" LB <- ");
    if ( data.valueLB & B00000010)
        Debug ("CH ");
    if ( data.valueLB & B00000100)
        Debug ("DHW ");
    if ( data.valueLB & B00001000)
        Debug ("FLM ");

    Debug("]");
}

void printToDebug(int mode, OpenthermData &data) 
{
  char msgTxt[60];
  msgTxt[0]='\0';

  switch (mode) {
    case MODE_LISTEN_MASTER:
      DebugT("LM: ");
      break;
    case MODE_LISTEN_SLAVE:
      DebugT("LS: ");
      break;
    default:
      DebugTf("%02d: ", mode);
  }
  
  if (data.type == OT_MSGTYPE_READ_DATA) {
    Debug("ReadData ");
  }
  else if (data.type == OT_MSGTYPE_READ_ACK) {
    Debug("ReadAck  ");
  }
  else if (data.type == OT_MSGTYPE_WRITE_DATA) {
    Debug("WriteData");
  }
  else if (data.type == OT_MSGTYPE_WRITE_ACK) {
    Debug("WriteAck ");
  }
  else if (data.type == OT_MSGTYPE_INVALID_DATA) {
    Debug("Inv.Data ");
  }
  else if (data.type == OT_MSGTYPE_DATA_INVALID) {
    Debug("DataInv. ");
  }
  else if (data.type == OT_MSGTYPE_UNKNOWN_DATAID) {
    Debug("Unknw.Id ");
  }
  else {
    Debug(data.type, BIN);
  }
  
  Debugf(" %03d %02x %02x", data.id, data.valueHB, data.valueLB);
  
  switch (data.id) {
    case 0:     DumpX("Status", data);
                break;
    case 1:     DumpF("Tset");
                break;
    case 14:    DumpF("MaxRelMod")
                break;
    case 16:    DumpF("Trset");
                break;
    case 17:    DumpF("RelMod");
                break;
    case 18:    DumpF("CH-pres");
                break;
    case 19:    DumpF("DHWflowRate");
                break;
    case 24:    DumpF("Tr");
                break;
    case 25:    DumpF("Tboiler");
                break;
    case 26:    DumpF("DHWTemp");
                break;
    case 27:    DumpF("Toutside");
                break;
    case 28:    DumpF("Tret");
                break;
    case 116:   DumpI("Burner Starts");
                break;
    case 120:   DumpI("Burner ops hrs");
                break;
              
  }
  Debugln(msgTxt);
}

void updateInfo(int mode, OpenthermData &data)
{
    // update info record based on response on read request

    // data read from boiler

    if (data.type == OT_MSGTYPE_READ_ACK)
    {
        switch (data.id) {
        case 0:     binfo.status = data.valueLB;
                    break;
        case 5:     // error flag
                    break;
        case 17:    binfo.modulation_level= data.f88();
                    break;
        case 18:    binfo.ch_pres = data.f88();
                    break;
        case 25:    binfo.Tboiler = data.f88();
                    break;
        case 26:    binfo.Tdhw = data.f88();
                    break;
        case 27:    binfo.Toutside = data.f88();
                    break;
        case 28:    binfo.Tret = data.f88();
                    break;
        case 48:    binfo.DHWLow = data.valueHB;
                    binfo.DHWHigh = data.valueLB;
                    break;
        case 49:    binfo.CHLow = data.valueHB;
                    binfo.CHHigh = data.valueLB;
                    break;

        default:    Debugf("Not caputed READ_ACK for %d\n", data.id);
        }
    }
  
    // data written form thermostate to boiler

    if (data.type == OT_MSGTYPE_WRITE_DATA)
    {
        switch (data.id) {
        case 1:     bcmd.Tset = data.f88();
                    break;
        case 2:     // config
                    break;
        case 14:    bcmd.MaxRelMod = data.f88();
                    break;
        case 16:    bcmd.TrSet = data.f88();
                    break;
        case 24:    bcmd.Tr = data.f88();
                    break;
        case 56:    bcmd.TdhwSet = data.f88();
                    break;
        default:    Debugf("Not caputed WRITE_DATA for %d\n", data.id);
        }
    }

}

void switchBoiler(int8_t onoff)
{
  ownMessage = { OT_MSGTYPE_WRITE_DATA, OT_MSGID_MASTER_CONFIG, onoff, 0x30};

  OPENTHERM::send(BOILER_OUT, ownMessage);
  messageSource = OT_MSG_SOURCE_SELF;
  mode = MODE_LISTEN_SLAVE;
  lastOnOffOverride = onoff;
}
 
void modifyMessage(int mode, OpenthermData &data)
{
    bool modified=false;

    if ( data.type == OT_MSGTYPE_WRITE_DATA )   // room thermostate sends info we might want to change
    {
        for (int o=0 ; o < nrOverRides ; o++)
        {
          Debugf("Override check %d\n", o);
          if ( OverRides[o].msgNr == data.id )
          {
            Debug("Override found: ");
            if( OverRides[o].enabled )
            {
              data.f88(OverRides[o].newValue);
              modified = true;
              Debugf("and changed to %.1f\n", OverRides[o].newValue);
            } else {
              Debugln("change disabled");
            }
            o = nrOverRides; // don't search further
          }
        }
    } else {
      if (data.type == OT_MSGTYPE_READ_DATA && data.id == OT_MSGID_STATUS) {
        // READ COMMAND SWITCHES BOILER STATE !
        uint8_t newHB;
        if (bcmd.Tset > 0){
          newHB = data.valueHB | 0x01;
          Debugln("CH on");
        } else {
          newHB = data.valueHB & 0xFE;
          Debugln("CH off");
        }
        if(newHB != data.valueHB)
        {
          modified = true;
          data.valueHB = newHB;
        }
      }
    }

    if(modified) {
        Debugln(">> [MODIFIED - NEW VERSION BELOW] << ");
        printToDebug(mode, data);
    }
}

void myOpenthermSetup()
{
 
  pinMode(THERMOSTAT_IN, INPUT);
  digitalWrite(THERMOSTAT_IN, HIGH); // pull up

  digitalWrite(THERMOSTAT_OUT, HIGH);
  pinMode(THERMOSTAT_OUT, OUTPUT); // low output = high current, high output = low current

  pinMode(BOILER_IN, INPUT);
  digitalWrite(BOILER_IN, HIGH); // pull up

  digitalWrite(BOILER_OUT, LOW);
  pinMode(BOILER_OUT, OUTPUT); // low output = high voltage, high output = low voltage

  httpServer.on("/api", HTTP_GET, processAPI);
  httpServer.on("/api/v0/override", HTTP_PUT, overrideAPI);  

}

void myOpenthermLoop()
{
  if(OPENTHERM::isError())
    mode = MODE_ERROR;

  switch(mode){
  case MODE_LISTEN_MASTER:   // listen to room controller
    DebugTln("MODE_LISTEN_MASTER");
    if (OPENTHERM::isSent() || OPENTHERM::isIdle() || OPENTHERM::isError()) 
        OPENTHERM::listen(THERMOSTAT_IN);
    
    if (OPENTHERM::hasMessage())
      mode=MODE_GET_MESSAGE_FROM_MASTER;

    break;

  case MODE_GET_MESSAGE_FROM_MASTER:
    DebugTln("MODE_GET_MESSAGE_FROM_MASTER");

    if (OPENTHERM::getMessage(busMessage)) 
    {
      printToDebug(mode,busMessage);
      updateInfo(mode, busMessage);

      // hijack this message type and return response without waiting for boiler!
/*
      if(busMessage.id == OT_MSGID_MASTER_CONFIG && onoffOverride) {
        Debugln("Hijacked message, returning ACK");
        ownMessage = { OT_MSGTYPE_WRITE_ACK, OT_MSGID_MASTER_CONFIG, busMessage.valueHB, busMessage.valueLB };
        delay(300);
        OPENTHERM::send(THERMOSTAT_OUT, ownMessage);
        mode = MODE_LISTEN_MASTER;
        return;
      }
*/
      modifyMessage(mode, busMessage);

      mode=MODE_FORWARD_MESSAGE_TO_SLAVE;

    } else mode = MODE_ERROR ;
    break;

  case MODE_FORWARD_MESSAGE_TO_SLAVE:
    DebugTln("MODE_FORWARD_MESSAGE_TO_SLAVE");

    //if( OPENTHERM::isIdle() || OPENTHERM::isError() ){
        OPENTHERM::send(BOILER_OUT, busMessage); // forward message to boiler
        mode = MODE_LISTEN_SLAVE;
    //}
    break;

  case MODE_LISTEN_SLAVE:  // listen to boiler
    DebugTln("MODE_LISTEN_SLAVE");

    if( OPENTHERM::isSent()) {
        OPENTHERM::listen(BOILER_IN, 800); // response need to be send back by boiler within 800ms
        if( OPENTHERM::hasMessage() )
          mode = MODE_GET_MESSAGE_FROM_SLAVE;
        else
          mode = MODE_ERROR;
        
    }
    break;

  case MODE_GET_MESSAGE_FROM_SLAVE:
    DebugTln("MODE_GET_MESSAGE_FROM_SLAVE");

    if (OPENTHERM::getMessage(busMessage)) 
    {
      printToDebug(mode, busMessage);
      updateInfo(mode, busMessage);
      mode=MODE_FORWARD_MESSAGE_TO_MASTER;
    } else mode = MODE_ERROR;
    break;

  case MODE_FORWARD_MESSAGE_TO_MASTER:
    DebugTln("MODE_FORWARD_MESSAGE_TO_MASTER");
    if( OPENTHERM::isIdle()) {
        OPENTHERM::send(THERMOSTAT_OUT, busMessage); // send message back to thermostat
       mode = MODE_LISTEN_MASTER;
    }
    break;

  default:
    if (OPENTHERM::isError()) {
      Debugln("<- Timeout");
      OPENTHERM::setIdle();
    }
    mode = MODE_LISTEN_MASTER;

  }
 
}

// API part of the application

void sendStatus() 
{
  sendStartJsonObj("otinfo");

  sendJsonObj("status",           binfo.status);             // 0
  sendJsonObj("modulation_level", binfo.modulation_level);   // 17
  sendJsonObj("ch_pres",          binfo.ch_pres);            // 18
  sendJsonObj("Tboiler",          binfo.Tboiler);            // 25
  sendJsonObj("Tdhw",             binfo.Tdhw);               // 26
  sendJsonObj("Toutside",         binfo.Toutside);           // 27
  sendJsonObj("Tret",             binfo.Tret);               // 28
 
  sendJsonObj("Tset",             bcmd.Tset);       // 1  
  sendJsonObj("MaxRelMod",        bcmd.MaxRelMod);  // 14
  sendJsonObj("TrSet",            bcmd.TrSet);      // 16
  sendJsonObj("Tr",               bcmd.Tr);         // 24
  sendJsonObj("TdhwSet",          bcmd.TdhwSet);    // 56

  sendJsonObj("onoffOverride",    onoffOverride ? 1 : 0);
  sendJsonObj("lastOnOffOverride",lastOnOffOverride);
  
  sendEndJsonObj();

}

float stooklijn()
{
  float toReturn = min(TEMP_CH_MAX, TEMP_CH_BASE_LINE + TEMP_BASE_FACTOR * max(0.0, TEMP_OAT_BASE_LINE - binfo.Toutside));
  DebugTf("Stooklijn = %5.2f\n", toReturn);
  return toReturn;
}

void overrideAPI() 
{
  char fName[40] = "";
  char URI[50]   = "";
  String words[10];

  DebugTln ("Entering overrideAPI");
  strncpy( URI, httpServer.uri().c_str(), sizeof(URI) );

  if (httpServer.method() != HTTP_PUT) 
  {
    httpServer.send(401, "text/plain", "401: internal server error\r\n");
    return;
  }

  DebugTf("from[%s] URI[%s] method[PUT] \r\n",
          httpServer.client().remoteIP().toString().c_str(), URI); 
  
  int8_t wc = splitString(URI, '/', words, 10);
  
  if (Verbose) 
  {
    DebugT(">>");
    for (int w=0; w<wc; w++)
    {
      Debugf("word[%d] => [%s], ", w, words[w].c_str());
    }
    Debugln(" ");
  }

  if (words[1] != "api")
  {
    sendApiNotFound(URI);
    return;
  }

  if (words[2] != "v0")
  {
    sendApiNotFound(URI);
    return;
  }

  // use /api/v0/override/<OTparmNR>/<FloatValue>
  // or  /api/v0/override/<OTparmNR>/disable

  if (words[3] == "override")
  {
    int8_t param = atoi(words[4].c_str());
    bool found = false;
    for (int8_t o=0 ; o < nrOverRides && !found ; o++)
    {
      if ( OverRides[o].msgNr == param ) {
        found=true;
        if ( !strcmp(words[5].c_str(),"disable")) {
          OverRides[o].enabled = false;
        } else {
          float newValue = atof(words[5].c_str());
          OverRides[o].enabled = true;
          OverRides[o].newValue = newValue;

          // received value for setpoint? 

          if(param == OT_MSGID_CH_SETPOINT && newValue != 0.0 && TEMP_BASE_FACTOR != 0)
          {
            OverRides[o].newValue = stooklijn();
          }
        }
      }
    }
    if (!found) {
      sendApiNotFound(URI);
      return;
    } else {
      sendStatus();
    }
  } else if (words[3] == "boiler") {
    Debugln("Boiler API called!");
    if(words[4] == "on" ) {
      boilerOn();
      onoffOverride = true;
      sendStatus();
    } else if(words[4] == "off") {
      boilerOff();
      onoffOverride = true;
      sendStatus();
    } else if (words[4] == "auto"){
      onoffOverride = false;
      sendStatus();
    } else sendApiNotFound(URI);

  } else {
    sendApiNotFound(URI);
    return;
  }
  
}

void orgOpenthermLoop()
{
  if (mode == MODE_LISTEN_MASTER) {
    if (OPENTHERM::isSent() || OPENTHERM::isIdle() || OPENTHERM::isError()) {
      OPENTHERM::listen(THERMOSTAT_IN);
    }
    else if (OPENTHERM::getMessage(message)) {

      printToDebug(mode, message);
      modifyMessage(mode, message);
      updateInfo(mode, message);

      OPENTHERM::send(BOILER_OUT, message); // forward message to boiler
      mode = MODE_LISTEN_SLAVE;
    }
  }
  else if (mode == MODE_LISTEN_SLAVE) {
    if (OPENTHERM::isSent()) {
      OPENTHERM::listen(BOILER_IN, 800); // response need to be send back by boiler within 800ms
    }
    else if (OPENTHERM::getMessage(message)) {
      printToDebug(mode, message);
      modifyMessage(mode, message);
      updateInfo(mode, message);

      OPENTHERM::send(THERMOSTAT_OUT, message); // send message back to thermostat
      mode = MODE_LISTEN_MASTER;
    }
    else if (OPENTHERM::isError()) {
      mode = MODE_LISTEN_MASTER;
      Debugln("<- Timeout");
    }
  }
}
