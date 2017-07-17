/*--------------------------------------------------
  ANI Camera Remote by Andreas Niggemann, 2017
  Versions:
  20170607 Base functions
	20170613 Shot counter for WebGUI
  20170712 Defining triggerPin, settings
  20170717 Refactoring
  --------------------------------------------------*/

#include <ESP8266WiFi.h>

//--------------------------------------------------
// Settings:
//
const char* ssid = "ANI-CameraRemote";             // WiFi SSID
const char* password = "Remoter12345678";          // set to "" for open access point w/o passwortd
const int webServerPort = 80;                      // Port for web server

const int triggerPin = 2;                          // GPIO2 as trigger output

// Timelapse Defaults
const unsigned long default_delayToStart = 0;      // Delay in seconds till start of timelapse
const unsigned long default_numberOfShots = 10;    // Number of shots in timelapse mode
const unsigned long default_delayBetweenShots = 5; // Delay between shots in timelapse mode

// End of settings
//--------------------------------------------------




// Global variables
const String prgTitle = "ANI Camera Remote";
const String prgVersion = "1.1";
const int versionMonth = 7;
const int versionYear = 2017;

const int timeSlot = 250; // timeslot in ms
const int timeSlotsPerSecond = 4;

unsigned long delayToStart = default_delayToStart;
unsigned long numberOfShots = default_numberOfShots;
unsigned long delayBetweenShots = default_delayBetweenShots;
unsigned long previousTimeSlotMillis = 0;
unsigned long currentDelayToStart = 0;
unsigned long currentNShots = 0;
unsigned long NumberOfShotsSinceStart = 0; //
unsigned long currentDelayBetweenShots = 0;

unsigned long secCounter = 0; // count the seconds since start of timelapse
unsigned long timeSlotCounter = 0; // count the timeslot since start of timelapse

enum triggerModes { ON = 0, OFF = 1 };
triggerModes currentTriggerMode = OFF;
enum execModes { ONESHOT, TIMELAPSE, STOP, RESET, SHOTINFO, NONE };
execModes currentExecMode = NONE;

// Create webserver on webServerPort
WiFiServer server(webServerPort);

WiFiClient client;

void setup()
{
  pinMode(triggerPin, OUTPUT); // setup GPIO as camera trigger
  trigger(OFF);
  WiFi.mode(WIFI_AP); // AP mode for connections
  WiFi.softAP(ssid, password);
  server.begin();
}

void loop()
{
  String sResponse, sHeader, sParam = "";

  process();

  client = server.available(); // Check if a client has connected
  String sRequest = getRequest(client);

  if (sRequest == "") // stop client, if request is empty
  {
    client.stop();
    return;
  }

  bool pathOK = checkPathAndGetParameters(sRequest, sParam); // get parameters from request, check path
  if (pathOK) // generate the html page
  {
    extractParams(sParam);
    pre_process();
    sResponse = generateHTMLPage(delayToStart, numberOfShots, delayBetweenShots);
    generateOKHTML(sResponse, sHeader);
  }
  else // 404 if error
    generateErrorHTML(sResponse, sHeader);

  client.print(sHeader); // Send the response to the client
  client.print(sResponse);
  client.stop(); // and stop the client
}

void pre_process()
{
  if (currentExecMode == ONESHOT)
  {
    trigger(ON);
    currentExecMode == NONE;
    currentDelayToStart = 0;
    currentNShots = 0;
    currentDelayBetweenShots = 0;
  }
  else if (currentExecMode == STOP)
  {
    currentExecMode == NONE;
    trigger(OFF);
  }
  else if (currentExecMode == RESET)
  {
    delayToStart = default_delayToStart;
    numberOfShots = default_numberOfShots;
    delayBetweenShots = default_delayBetweenShots;
    currentExecMode = NONE;
    trigger(OFF);
  }
  else if (currentExecMode == TIMELAPSE)
  {
    secCounter = 0;
    timeSlotCounter = 0;
    currentDelayToStart = delayToStart;
    currentDelayBetweenShots = delayBetweenShots;
    NumberOfShotsSinceStart = 0;
    if (delayToStart == 0)
    {
      currentNShots = numberOfShots - 1;
      trigger(ON);
    }
    else
      currentNShots = numberOfShots;
  }
}

void process()
{
  if ((currentExecMode == NONE) || (currentExecMode == STOP) )
    return;
  unsigned long currentTimeSlotMillis = millis();
  if ((currentTimeSlotMillis - previousTimeSlotMillis) > timeSlot) // 250 ms time slot
  {
    if (currentTriggerMode == ON)
      trigger(OFF);

    previousTimeSlotMillis = currentTimeSlotMillis;

    timeSlotCounter++; // Count the timeslots
    if (timeSlotCounter == timeSlotsPerSecond)
    {
      secCounter++; // Count the seconds
      timeSlotCounter = 0;

      if (currentDelayToStart > 0) // Start delay
        currentDelayToStart--;
      else if (currentDelayBetweenShots > 0)
        if ((secCounter % currentDelayBetweenShots ) == 0) // Delay between shots
          if (currentNShots >= 1) // more than one shot timelapse?
          {
            if (currentTriggerMode == OFF)
              trigger(ON);
            currentNShots--;
            NumberOfShotsSinceStart++;
          }
    }
  }
}

String generateHTMLPage(unsigned long sDelay, unsigned long nShots, unsigned long interval)
{
  String retVal = "<html><head><title>ANI Camera Remote</title>"
                  "<style>table, th, td { border: 0px solid black;} button, input[type=number], input[type=submit] "
                  "{width:100px;height:24px;font-size:14pt;}</style></head><body>"
                  "<font color=\"#000000\"><body bgcolor=\"#c0c0c0\">"
                  "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">"
                  "<h2>" + prgTitle + " V" + prgVersion + "</h2>"
                  "<table style=\"font-size:24px;\">"
                  "<tr><td>Timelapse</td><td><a href=?function=RESET><button>Reset</button></a></td></tr>"
                  "<form method=GET>"
                  "<tr><td>Delay to start:</td><td><input id=delayToStart name=delayToStart type=number min=0 step=1 value=" + String(sDelay) + "> sec</td></tr>"
                  "<tr><td>Number of shots:</td><td><input id=numberOfShots name=numberOfShots type=number min=1 step=1 value=" + String(nShots) + "></td></tr>"
                  "<tr><td>Interval:</td><td><input id=delayBetweenShots name=delayBetweenShots type=number min=1 step=1 value=" + String(interval) + "> sec</td></tr>"
                  "<tr><td></td><td></td></tr>"
                  "<tr><td></td><td></td></tr>"
                  "<tr><td><input type=submit value=Start></td>"
                  "</form>"
                  "<td><a href=?function=STOP><button>Stop</button></a></td></tr>"
                  "<tr><td></td><td></td></tr>"
                  "<tr><td></td><td></td></tr>"
                  "<tr><td></td><td></td></tr>"
                  "<tr><td></td><td></td></tr>";
  String buttonState = "disabled";
  if ((currentExecMode == TIMELAPSE) || (currentExecMode == SHOTINFO))
    buttonState = "";
  retVal += "<tr><td><a href=?function=SINGLE_SHOT><button>One Shot</button></a></td><td><a href=?function=SHOT_INFO><button " + buttonState + ">Info</button></a></td></tr>";
  if (currentExecMode == SHOTINFO) // Display remaining number of shots and delay time till start in TIMELAPSE mode
  {
    retVal += "<tr><td>Remaining delay:</td><td>" + String(currentDelayToStart) + " sec</td></tr>";
    retVal += "<tr><td>Remaining shots:</td><td>" + String(currentNShots) + "</td></tr>";
    currentExecMode = TIMELAPSE;
  }
  retVal += "</table></BR>";
  retVal += "<FONT SIZE=-1>GPIO" + String(triggerPin) + " triggers camera shutter.<BR>";
  return retVal;
}

void generateErrorHTML(String& sResp, String& sHead)
{
  sResp = "<html><head><title>404 Not Found</title></head><body><h1>Not Found</h1><p>The requested URL was not found on this server.</p></body></html>";

  sHead = "HTTP/1.1 404 Not found\r\n"
          "Content-Length: " + String(sResp.length()) + "\r\n"
          "Content-Type: text/html\r\n"
          "Connection: close\r\n"
          "\r\n";
}

void generateOKHTML(String& sResp, String& sHead)
{
  sResp += "<FONT SIZE=-3>Andreas Niggemann " + String(versionMonth) + "/" + String(versionYear) + "<BR></body></html>";

  sHead = "HTTP/1.1 200 OK\r\n"
          "Content-Length: " + String(sResp.length()) + "\r\n"
          "Content-Type: text/html\r\n"
          "Connection: close\r\n"
          "\r\n";
}

String getRequest(WiFiClient webclient)
{
  String retVal = "";
  if (webclient)
  {
    unsigned long timeout = millis() + 250;
    while (!webclient.available() && (millis() < timeout) )
      delay(1);
    retVal = webclient.readStringUntil('\r'); // Read the first line of the request
    webclient.flush();
  }
  return retVal;
}

bool checkPathAndGetParameters(String sReq, String& sPar)
{
  String sGetstart = "GET ", sPat = "";
  int iStart, iEndSpace, iEndQuest;
  iStart = sReq.indexOf(sGetstart);
  sPar = "";
  if (iStart >= 0)
  {
    iStart += +sGetstart.length();
    iEndSpace = sReq.indexOf(" ", iStart);
    iEndQuest = sReq.indexOf("?", iStart);

    if (iEndSpace > 0)
    {
      if (iEndQuest > 0)
      {
        sPat  = sReq.substring(iStart, iEndQuest);
        sPar = sReq.substring(iEndQuest, iEndSpace);
      }
      else
        sPat  = sReq.substring(iStart, iEndSpace);
    }
  }
  return sPat == "/";
}

void extractParams(String params)
{
  params[0] = ' ';
  params.trim();
  params.toUpperCase();
  if (params.length() > 0)
  {
    params += "&";
    if (params.indexOf("STOP") >= 0) // STOP timelapse
      currentExecMode = STOP;
    else if (params.indexOf("RESET") >= 0) // RESET form
      currentExecMode = RESET;
    else if (params.indexOf("SINGLE_SHOT") >= 0) // single shot
      currentExecMode = ONESHOT;
    else if (params.indexOf("SHOT_INFO") >= 0) // single shot
      currentExecMode = SHOTINFO;
    else // Timelapse
    {
      currentExecMode = TIMELAPSE;
      String par = params + " ";
      int idx = par.indexOf("&"); // Find element delimiter
      while (idx > 2)
      {
        String elem = par.substring(0, idx);
        par = par.substring(idx + 1, par.length());
        int idx2 = elem.indexOf("=");
        if (idx2 > 1)
        {
          String elemName = elem.substring(0, idx2); // Variable name
          String elemValue = elem.substring(idx2 + 1, elem.length()); // Variable value
          if (elemName == "DELAYTOSTART")
            delayToStart = elemValue.toInt();
          if (elemName == "NUMBEROFSHOTS")
            numberOfShots = elemValue.toInt();
          if (elemName == "DELAYBETWEENSHOTS")
            delayBetweenShots = elemValue.toInt();
        }
        idx = par.indexOf("&");
      }
    }
  }
}

void trigger(triggerModes tMode)
{
  digitalWrite(triggerPin, tMode);
  currentTriggerMode =  tMode;
}


