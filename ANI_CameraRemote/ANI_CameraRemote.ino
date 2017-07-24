/*--------------------------------------------------
  ANI Camera Remote by Andreas Niggemann, 2017
  Versions:
  20170607 Base functions
	20170613 Shot counter for WebGUI
  20170712 Defining triggerPin, settings
  20170717 Refactoring
  20170720 Bugfix: Restarting timelapse after reconnect removed
  20170720 Information is displayed all the time, refresh button
  20170720 Autorefresh setting added
  20170722 Input pin as timelapse start
  --------------------------------------------------*/

#include <ESP8266WiFi.h>

//--------------------------------------------------
// Settings:
//
const char* ssid = "ANI-CameraRemote";             // WiFi SSID
const char* password = "Remoter12345678";          // set to "" for open access point w/o passwortd
const int webServerPort = 80;                      // Port for web server

const int triggerPin = 2;                          // GPIO2 as trigger output
const int startPin = 5;                            // GPIO5 as start input for timelapse, -1 = deactivate input pin processing

// Timelapse Defaults
const unsigned long default_delayToStart = 0;      // Delay in seconds till start of timelapse
const unsigned long default_numberOfShots = 10;    // Number of shots in timelapse mode
const unsigned long default_delayBetweenShots = 5; // Delay between shots in timelapse mode
const unsigned long default_autorefresh = 15;      // In timelapse mode autorefresh webGUI every 15 seconds, 0 = autorefresh off

// End of settings
//--------------------------------------------------




// Global variables
const String prgTitle = "ANI Camera Remote";
const String prgVersion = "1.2";
const int versionMonth = 7;
const int versionYear = 2017;

const int timeSlot = 250; // timeslot in ms
const int timeSlotsPerSecond = 4;

unsigned long delayToStart = default_delayToStart;
unsigned long numberOfShots = default_numberOfShots;
unsigned long delayBetweenShots = default_delayBetweenShots;
unsigned long autorefresh = default_autorefresh;
unsigned long previousTimeSlotMillis = 0;
unsigned long currentDelayToStart = 0;
unsigned long currentNShots = 0;
unsigned long NumberOfShotsSinceStart = 0; //
unsigned long currentDelayBetweenShots = 0;
unsigned long currentAutorefresh = 0;

unsigned long secCounter = 0; // count the seconds since start of timelapse
unsigned long timeSlotCounter = 0; // count the timeslot since start of timelapse

String myIPStr = "";

enum triggerModes { ON = 0, OFF = 1 };
triggerModes currentTriggerMode = OFF;
enum execModes { NONE, ONESHOT, TIMELAPSESTART, TIMELAPSERUNNING, TIMELAPSESTOP, WAITFORGPIO, REFRESH, RESET };
execModes currentExecMode = NONE;


// Create webserver on webServerPort
WiFiServer server(webServerPort);

WiFiClient client;

void setup()
{
  if (startPin > -1)
    pinMode(startPin, INPUT_PULLUP); // Setup GPIO input for start signal
  pinMode(triggerPin, OUTPUT); // setup GPIO as camera trigger
  trigger(OFF);
  WiFi.mode(WIFI_AP); // AP mode for connections
  WiFi.softAP(ssid, password);
  server.begin();
  IPAddress ip = WiFi.softAPIP();
  myIPStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
}

void loop()
{
  String sResponse, sHeader, sParam = "";

  process();
  CheckAndProcessInputPin(startPin);

  client = server.available(); // Check if a client has connected
  String sRequest = getRequest(client);
  if (sRequest != "") // There is a request
  {
    bool pathOK = checkPathAndGetParameters(sRequest, sParam); // get parameters from request, check path
    if (pathOK) // generate the html page
    {
      if (sParam != "")
      {
        extractParams(sParam);
        process_mode();
      }
      sResponse = generateHTMLPage(delayToStart, numberOfShots, delayBetweenShots);
      generateOKHTML(sResponse, sHeader);
    }
    else // 404 if error
      generateErrorHTML(sResponse, sHeader);
    clientOutAndStop(sHeader, sResponse);
  }
  else
    client.stop(); // stop client, if request is empty
}

void process_mode()
{
  if (currentExecMode == ONESHOT)
  {
    currentAutorefresh = 0;
    trigger(ON);
    currentDelayToStart = 0;
    currentNShots = 0;
    currentDelayBetweenShots = 0;
    currentExecMode == NONE;
  }
  else if (currentExecMode == TIMELAPSESTOP)
  {
    currentAutorefresh = 0;
    currentNShots = 0;
    trigger(OFF);
    currentExecMode == NONE;
  }
  else if (currentExecMode == RESET)
  {
    currentAutorefresh = 0;
    delayToStart = default_delayToStart;
    numberOfShots = default_numberOfShots;
    delayBetweenShots = default_delayBetweenShots;
    currentNShots = 0;
    trigger(OFF);
    currentExecMode = NONE;
  }
  else if (currentExecMode == TIMELAPSESTART)
  {
    currentAutorefresh = autorefresh;
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
    currentExecMode = TIMELAPSERUNNING;
  }
  previousTimeSlotMillis = millis();
}

void process()
{
  if ((currentExecMode == NONE) || (currentExecMode == TIMELAPSESTOP) )
    return;
  unsigned long currentTimeSlotMillis = millis();
  if ((currentTimeSlotMillis - previousTimeSlotMillis) > timeSlot) // 250 ms time slot
  {
    if (currentTriggerMode == ON)
    {
      trigger(OFF);
      if ((currentNShots <= 0) && (currentExecMode == TIMELAPSERUNNING))  // End of Timelapse mode
        currentExecMode = TIMELAPSESTOP;
    }

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

void extractParams(String params)
{
  params[0] = ' ';
  params.trim();
  params.toUpperCase();
  if (params.length() > 0)
  {
    params += "&";
    if ((currentExecMode == TIMELAPSERUNNING) || (currentExecMode == REFRESH)) // Only refresh and stop allowed in timelapse mode
    {
      if (params.indexOf("REFRESH") >= 0) // Refresh webGUI
        currentExecMode = REFRESH;
      if (params.indexOf("STOP") >= 0) // STOP timelapse
        currentExecMode = TIMELAPSESTOP;
    }
    else // blocked if timelapse running
    {
      if (params.indexOf("RESET") >= 0) // RESET form
        currentExecMode = RESET;
      else if (params.indexOf("SINGLE_SHOT") >= 0) // single shot
        currentExecMode = ONESHOT;
      else if ((params.indexOf("WAITFORGPIO") >= 0) && (startPin > -1)) // Wait for signal on input pin
      {
        currentExecMode = WAITFORGPIO;
        setTimelapseParams(params);
      }
      else if ((params.indexOf("DELAYTOSTART") >= 0) && (currentNShots <= 0)) // Switch to timelapse
      {
        currentExecMode = TIMELAPSESTART;
        setTimelapseParams(params);
      }
    }
  }
}

void setTimelapseParams(String params) // Set timelapse parameters for modes WAITFORGPIO and START
{
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

String generateHTMLPage(unsigned long sDelay, unsigned long nShots, unsigned long interval)
{
  String refreshLine = "";
  String stateNotInTimelapse = "enabled";
  String buttonState = "disabled";
  String waitForGPIOLine = "";
  String waitForGPIOLineHint = "";
  bool showWaitForGPIOLineHint = true;
  // disable webGUI elements while in timelapse
  if (((currentExecMode == TIMELAPSERUNNING) || (currentExecMode == REFRESH)) && (currentNShots > 0))
  {
    buttonState = "enabled";
    stateNotInTimelapse = "disabled";
    showWaitForGPIOLineHint = false;
  }
  else if (currentExecMode == WAITFORGPIO)
    currentAutorefresh = autorefresh;
  else
    currentAutorefresh = 0;
  if (currentAutorefresh > 0)
    refreshLine = "<meta http-equiv=\"refresh\" content=\"" + String(currentAutorefresh) + "; URL=http://" + myIPStr + "/\">";
  if (startPin > -1) // webGUI elements für GPIO if defined
  {
    waitForGPIOLine = "<input type=submit value=\"Wait for GPIO" + String(startPin) + "\" " + stateNotInTimelapse + " name=WAITFORGPIO>";
    if (showWaitForGPIOLineHint)
      waitForGPIOLineHint = "<FONT SIZE=-2>GPIO" + String(startPin) + " input LOW starts timelapse<BR>";
  }
  // HTML here
  String retVal = "<html><head><title>ANI Camera Remote</title>" + refreshLine + ""
                  "<style>table, th, td { border: 0px solid black;} button, input[type=number], input[type=submit]"
                  "{width:120px;height:20px;font-size:12pt;}</style></head><body>"
                  "<font color=\"#000000\"><body bgcolor=\"#c0c0c0\">"
                  "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">"
                  "<h2>" + prgTitle + " V" + prgVersion + "</h2>"
                  "<table style=\"font-size:16px;\">"
                  "<tr><td>Timelapse</td><td><a href=?function=RESET><button " + stateNotInTimelapse + ">Reset</button></a></td></tr>"
                  "<form method=GET>"
                  "<tr><td>Delay to start:</td><td><input " + stateNotInTimelapse + " id=delayToStart name=delayToStart type=number min=0 step=1 value=" + String(sDelay) + "> sec</td></tr>"
                  "<tr><td>Number of shots:</td><td><input " + stateNotInTimelapse + " id=numberOfShots name=numberOfShots type=number min=1 step=1 value=" + String(nShots) + "></td></tr>"
                  "<tr><td>Interval:</td><td><input " + stateNotInTimelapse + " id=delayBetweenShots name=delayBetweenShots type=number min=1 step=1 value=" + String(interval) + "> sec</td></tr>"
                  "<tr><td></td><td></td></tr>"
                  "<tr><td></td><td></td></tr>"
                  "<tr><td>" + waitForGPIOLine + ""
                  "<input type=submit value=Start " + stateNotInTimelapse + " name=START></td>"
                  "</form>"
                  "<td><a href=?function=STOP><button " + buttonState + ">Stop</button></a></td></tr>"
                  "<tr><td></td><td></td></tr>"
                  "<tr><td></td><td></td></tr>"
                  "<tr><td></td><td></td></tr>"
                  "<tr><td></td><td></td></tr>"
                  "<tr><td><a href=?function=SINGLE_SHOT><button " + stateNotInTimelapse + ">One Shot</button></a></td><td><a href=?function=REFRESH><button " + buttonState + ">Refresh</button></a></td></tr>"
                  "<tr><td><FONT SIZE=-1>Remaining delay:</td><td><FONT SIZE=-1>" + String(currentDelayToStart) + " sec</td></tr>"
                  "<tr><td><FONT SIZE=-1>Remaining shots:</td><td><FONT SIZE=-1>" + String(currentNShots) + "</td></tr>"
                  "</table></BR>"
                  "" + waitForGPIOLineHint + ""
                  "<FONT SIZE=-2>GPIO" + String(triggerPin) + " triggers camera shutter<BR>"
                  ;
  if (refreshLine != "")
    retVal += "<FONT SIZE=-2>Page will refresh every " + String(currentAutorefresh) +  " seconds<BR>";
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

void clientOutAndStop(String sHdr, String sRespo)
{
  client.print(sHdr); // Send the response to the client
  client.print(sRespo);
  client.stop(); // and stop the client
}

void CheckAndProcessInputPin(int inputPin)
{
  if (inputPin > -1) // Input pin defined
    if (digitalRead(inputPin) == LOW) // start signal
    {
      while (digitalRead(inputPin) == LOW) // wait till button not pressed
        delay(250);
      currentExecMode = TIMELAPSESTART;
      process_mode();
    }
}

void trigger(triggerModes tMode)
{
  digitalWrite(triggerPin, tMode);
  currentTriggerMode =  tMode;
}

