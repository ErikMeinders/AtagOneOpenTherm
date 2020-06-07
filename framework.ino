

void frameworkSetup()
{
  Serial.begin(115200);
  while(!Serial) { /* wait a bit */ }

  lastReset     = ESP.getResetReason();

  startTelnet();
  
  DebugTf("Booting....[%s]\r\n\r\n", String(_FW_VERSION).c_str());
  
//================ LittleFS ===========================================
  if (LittleFS.begin()) 
  {
    DebugTln(F("LittleFS Mount succesfull\r"));
    LittleFSmounted = true;
  } else { 
    DebugTln(F("LittleFS Mount failed\r"));   // Serious problem with LittleFS 
    LittleFSmounted = false;
}

  readSettings(true);

  // attempt to connect to Wifi network:
  DebugTln("Attempting to connect to WiFi network\r");
  int t = 0;
  while ((WiFi.status() != WL_CONNECTED) && (t < 25))
  {
    delay(100);
    Debug(".");
    t++;
  }
  if ( WiFi.status() != WL_CONNECTED) 
  {
    sprintf(cMsg, "Connect to AP '%s' and configure WiFi on  192.168.4.1   ", _HOSTNAME);
    DebugTln(cMsg);
  }
  // Connect to and initialise WiFi network
  digitalWrite(LED_BUILTIN, HIGH);
  startWiFi(_HOSTNAME, 240);  // timeout 4 minuten
  digitalWrite(LED_BUILTIN, LOW);

  startMDNS(settingHostname);
  startNTP();

  snprintf(cMsg, sizeof(cMsg), "Last reset reason: [%s]\r", ESP.getResetReason().c_str());
  DebugTln(cMsg);

  Debug("\nGebruik 'telnet ");
  Debug (WiFi.localIP());
  Debugln("' voor verdere debugging\r\n");

//================ Start HTTP Server ================================
  setupFSexplorer();
  httpServer.serveStatic("/FSexplorer.png",   LittleFS, "/FSexplorer.png");
  httpServer.on("/",          sendIndexPage);
  httpServer.on("/index",     sendIndexPage);
  httpServer.on("/index.html",sendIndexPage);
  httpServer.serveStatic("/index.css", LittleFS, "/index.css");
  httpServer.serveStatic("/index.js",  LittleFS, "/index.js");
  // all other api calls are catched in FSexplorer onNotFounD!
 
  httpServer.begin();
  DebugTln("\nServer started\r");
  
  // Set up first message as the IP address
  sprintf(cMsg, "%03d.%03d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  DebugTf("\nAssigned IP[%s]\r\n", cMsg);
  
} // setup()

void frameworkLoop()
{
    handleNTP();
    httpServer.handleClient();
    MDNS.update();
}