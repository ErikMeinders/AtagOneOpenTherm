
#include "framework.h"
#include <opentherm.h>

#include "AtagOneOpenTherm.h"

// WiFi Server object and parameters

WiFiServer server(80);

void setup()
{
  frameworkSetup();
  myOpenthermSetup();
}

void loop()
{ 
  frameworkLoop(); 
  //myOpenthermLoop();
  orgOpenthermLoop();
} 
