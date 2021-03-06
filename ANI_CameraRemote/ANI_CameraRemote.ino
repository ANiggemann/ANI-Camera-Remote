/*--------------------------------------------------
  ANI Camera Remote by Andreas Niggemann, 2018
  Versions:
  20170607 Base functions
  20170613 Shot counter for WebGUI
  20170712 Defining triggerPin, settings
  20170717 Refactoring
  20170720 Bugfix: Restarting timelapse after reconnect removed
  20170720 Information is displayed all the time, refresh button
  20170720 Autorefresh setting added
  20170722 Input pin as timelapse start
  20170724 Setting for autostart timelapse mode
  20170724 Calculate remaining time
  20180506 Preparatory work for M5Stack (display and keyboard)
  20180506 Hint: Use ESP8266 V2.4.0 since V2.4.1 has problems
  20180510 First Version for M5Stack
  20180511 M5Stack: WiFi OFF by default, config menu, C button as value accelerator
  20180528 Setting for trigger duration
  20180529 Preparatory work for timetable
  20180601 Preparatory work for autofocus pin
  20180603 More compact font
  20180603 Show trigger state ON by flashing screen
  --------------------------------------------------*/
#ifdef ESP32
#include <M5Stack.h>
#include <WiFi.h>
#include <Free_Fonts.h>
#define SWITCH_WIFI_ON false
#else
#include <ESP8266WiFi.h>
#define SWITCH_WIFI_ON true
#endif

//--------------------------------------------------
// Settings:
//
const char* ssid = "ANI-CameraRemote";             // WiFi SSID
const char* password = "Remoter12345678";          // set to "" for open access point w/o password
const int webServerPort = 80;                      // Port for web server

const int screenSaverTime = 60;                    // M5Stack: Switch off screen after 60 seconds of inactivity

const int triggerPin = 2;                          // GPIO2 as trigger output
const int autofocusPin = 0;                        // GPIO0 as autofocus pin, -1 = deactivate autofocus start via pin
const int startPin = 5;                            // GPIO5 as start input for timelapse, -1 = deactivate input pin processing

// Timelapse Defaults
const unsigned long default_delayToStart = 0;      // Delay in seconds till start of timelapse
const unsigned long default_numberOfShots = 10;    // Number of shots in timelapse mode
const unsigned long default_delayBetweenShots = 5; // Delay between shots in timelapse mode
const unsigned long default_triggerDuration = 0;   // Duration of trigger ON in seconds (0 = 250ms)
const unsigned long default_autorefresh = 5;       // In timelapse mode autorefresh webGUI every 5 seconds, 0 = autorefresh off
const int timelapseAutoStart = 0;                  // 1 = Autostart timelapse mode, 0 = No autostart

// End of settings
//--------------------------------------------------




// Global variables
const String prgTitle = "ANI Camera Remote";
const String prgVersion = "1.5";
const String screenHeaderLine = prgTitle + " " + prgVersion;
const int versionMonth = 5;
const int versionYear = 2018;

const int timeSlot = 250; // timeslot in ms
const int timeSlotsPerSecond = 1000 / timeSlot;

// timeTable syntax:
// Elements separated by comma
// Elements can be:
//   Delay between shots in seconds (e.g. 5)
//   Repeat number x Delay between shots in seconds (e.g. 1000x13)
//   w and delay in seconds (do nothing) (e.g. w3600)
//   d and duration for trigger ON in seconds (e.g. d2), 0 = 250ms
//   (trigger duration stays the same until set anew via element d)
//
// Examples:
//   10,15,20,25,30
//   w18000,50000x5,20000x10
//   d5,50000x10
// Only loadable via GET parameters, not in UI
// URL Example:
// 192.168.4.1/?timeTable=w18000,50000x5,20000x10,d2,30,40,50
// This waits 18000 seconds, triggers the shutter 50000 times with an interval of 5 seconds, then
// triggers the shutter 20000 times with an interval of 10 seconds, sets a trigger ON duration of 2 seconds,
// triggers the shutter after 30 seconds, then after 40 more seconds and finally after 50 more seconds
String timeTable = "";

unsigned long delayToStart = default_delayToStart;
unsigned long numberOfShots = default_numberOfShots;
unsigned long delayBetweenShots = default_delayBetweenShots;
unsigned long triggerDuration = default_triggerDuration;
unsigned long autorefresh = default_autorefresh;
unsigned long previousTimeSlotMillis = 0;
unsigned long currentDelayToStart = 0;
unsigned long currentNShots = 0;
unsigned long currentTDuration = 0;
unsigned long NumberOfShotsSinceStart = 0;
unsigned long currentDelayBetweenShots = 0;
unsigned long currentTriggerDuration = 0;
unsigned long currentAutorefresh = 0;
int currentTimelapseAutoStart = timelapseAutoStart;

unsigned long secCounter = 0; // count the seconds since start of timelapse
unsigned long timeSlotCounter = 0; // count the timeslots in a second

int highlightLine = 2;
int lastDialogLine = 8;
unsigned long inactivityCounter = 0;
unsigned long previousMilliseconds = millis();
bool wifiON = SWITCH_WIFI_ON;

String myIPStr = "";

enum lcdScreens { MAINSCREEN, CONFIGSCREEN };
lcdScreens actuScreen = MAINSCREEN;
enum lcdStates { LCDOFF, LCDON };
bool lcdIsON = true;
enum triggerModes { ON = 0, OFF = 1 };
triggerModes currentTriggerMode = OFF;
enum execModes { NONE, ONESHOT, TIMELAPSESTART, TIMELAPSERUNNING, TIMELAPSESTOP, WAITFORGPIO, REFRESH, RESET };
execModes currentExecMode = NONE;


// Create webserver on webServerPort
WiFiServer server(webServerPort);
WiFiClient client;

void setup()
{
  displaySetup();

  pinMode(triggerPin, OUTPUT); // setup GPIO as camera trigger
  if (autofocusPin > -1)
    pinMode(autofocusPin, OUTPUT); // setup GPIO for autofocus start function
  trigger(OFF);
  if (startPin > -1)
    pinMode(startPin, INPUT_PULLUP); // Setup GPIO input for start signal

  setupWiFi();

  checkAndProcessAutoStart();
}

void loop()
{
  processTimeSlots();
  checkAndProcessStartPin();
  processKeyboard();
  if (wifiON)
    processWebClient();
}

void setupWiFi()
{
  if (wifiON)
  {
    WiFi.mode(WIFI_AP); // AP mode for connections
    WiFi.softAP(ssid, password);
    server.begin();
    IPAddress ip = WiFi.softAPIP();
    myIPStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
  }
}

void processWebClient()
{
  String sResponse, sHeader, sParam = "";

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
        processMode();
      }
      sResponse = generateHTMLPage(delayToStart, numberOfShots, delayBetweenShots, triggerDuration);
      generateOKHTML(sResponse, sHeader);
    }
    else // 404 if error
      generateErrorHTML(sResponse, sHeader);
    clientOutAndStop(sHeader, sResponse);
  }
  else
    client.stop(); // stop client, if request is empty
}

void setAndDoProcessMode(execModes newExecMode)
{
  currentExecMode = newExecMode;
  processMode();
}

void processMode()
{
  if (currentExecMode == ONESHOT)
  {
    currentAutorefresh = 0;
    trigger(ON);
    currentDelayToStart = 0;
    currentNShots = 0;
    currentDelayBetweenShots = 0;
    currentTriggerDuration = 0;
    currentTDuration = 0;
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
    triggerDuration = default_triggerDuration;
    currentNShots = 0;
    currentTDuration = 0;
    trigger(OFF);
    currentExecMode = NONE;
  }
  else if (currentExecMode == WAITFORGPIO)
  {
    secCounter = 0;
  }
  else if (currentExecMode == TIMELAPSESTART)
  {
    currentAutorefresh = autorefresh;
    secCounter = 0;
    timeSlotCounter = 0;
    currentDelayToStart = delayToStart;
    currentDelayBetweenShots = delayBetweenShots;
    currentTriggerDuration = triggerDuration;
    currentTDuration = triggerDuration;
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

void processTimeSlots()
{
  checkInactivity();

  if ((currentExecMode == NONE) || (currentExecMode == TIMELAPSESTOP) || (currentExecMode == WAITFORGPIO))
    return;

  unsigned long currentTimeSlotMillis = millis();

  if ((currentTimeSlotMillis - previousTimeSlotMillis) > timeSlot) // 250 ms time slot
  {
    previousTimeSlotMillis = currentTimeSlotMillis;

    if ((currentTriggerMode == ON) && (currentTDuration == 0)) // only if trigger duration is 250ms
      switchTriggerOffAndCheckStatus();

    timeSlotCounter++; // Count the timeslots
    if (timeSlotCounter == timeSlotsPerSecond)
    {
      secCounter++; // Count the seconds
      timeSlotCounter = 0;

      if ((currentTriggerMode == ON) && (currentTDuration > 0))
      {
        currentTDuration--;
        if (currentTDuration == 0)
          switchTriggerOffAndCheckStatus();
      }

      if (currentDelayToStart > 0) // Start delay
      {
        currentDelayToStart--;
        onDisplay(MAINSCREEN);
      }
      else if (currentDelayBetweenShots > 0)
        if ((secCounter % currentDelayBetweenShots) == 0) // Delay between shots
        {
          if (currentNShots >= 1) // more than one shot timelapse?
          {
            if (currentTriggerMode == OFF)
              trigger(ON);
            currentNShots--;
            NumberOfShotsSinceStart++;
          }
          onDisplay(MAINSCREEN);
        }
    }
  }
}

void processTimeTable()
{
  /*
    delayToStart = 0; // Reset wait (delay) timer anyway
    numberOfShots = 1; // At least one shot
    trigger(OFF);
    int idx = timeTable.indexOf(","); // Find end of next element
    while (idx >= 1)
    {
      String element = timeTable.substring(0, idx);
      if (element.length() >= 1) // something is there
      {
        int xPosition = element.indexOf("X");
        timeTable = timeTable.substring(idx + 1, timeTable.length()); // remove element from timetable
        if (element[0] == 'W') // set wait (=Delay) time
          delayToStart = atol(element.substring(1, element.length()).c_str());
        else if (element[0] == 'D') // Set trigger duration
          triggerDuration = atol(element.substring(1, element.length()).c_str());
        else if ((xPosition >= 1) && (element.length() >= 3)) // Number of shots and delay between shots in one element
        {
          numberOfShots = atol(element.substring(0, xPosition).c_str());
          delayBetweenShots = atol(element.substring(xPosition + 1, element.length()).c_str());
          break;
        }
        else // delay between shots only
        {
          delayBetweenShots = atol(element.substring(0, element.length()).c_str());
          break;
        }
      }
      idx = timeTable.indexOf(","); // Find end of next element
    }
    if (numberOfShots > 0)
      setAndDoProcessMode(TIMELAPSESTART);
  */
}

void switchTriggerOffAndCheckStatus()
{
  trigger(OFF);
  currentTDuration = currentTriggerDuration;
  if ((currentNShots <= 0) && (currentExecMode == TIMELAPSERUNNING))  // End of Timelapse mode
  {
    currentExecMode = TIMELAPSESTOP;
    if (timeTable != "" )  // Next element of timetable
      processTimeTable();
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
      else if ((params.indexOf("TIMETABLE") >= 0) && (currentNShots <= 0)) // Switch to timetable timelapse
      {
        setTimelapseParams(params);
        processTimeTable();
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
      if (elemName == "TRIGGERDURATION")
        triggerDuration = elemValue.toInt();
      if (elemName == "TIMETABLE")
        timeTable = elemValue + ",";
    }
    idx = par.indexOf("&");
  }
}

String generateHTMLPage(unsigned long sDelay, unsigned long nShots, unsigned long interval, unsigned long tDuration)
{
  String refreshLine = "";
  String remainingTimeLine = "";
  String stateNotInTimelapse = "enabled";
  String buttonState = "disabled";
  String waitForGPIOLine = "";
  String waitForGPIOLineHint = "";
  bool showWaitForGPIOLineHint = true;
  // disable webGUI elements while in timelapse
  if ((((currentExecMode == TIMELAPSERUNNING) || (currentExecMode == REFRESH)) && (currentNShots > 0)) || (timeTable != ""))
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
  {
    refreshLine = "<meta http-equiv=\"refresh\" content=\"" + String(currentAutorefresh) + "; URL=http://" + myIPStr + "/\">";
    String remainingTime = getRemainingTimeStr(sDelay, nShots, interval, secCounter);
    remainingTimeLine = "<tr><td><FONT SIZE=-1>Remaining time:</td><td><FONT SIZE=-1>" + remainingTime + "</td></tr>";
  }
  if (startPin > -1) // webGUI elements für GPIO if defined
  {
    waitForGPIOLine = "<input type=submit value=\"Wait for GPIO" + String(startPin) + "\" " + stateNotInTimelapse + " name=WAITFORGPIO>";
    if (showWaitForGPIOLineHint)
      waitForGPIOLineHint = "<FONT SIZE=-2>GPIO" + String(startPin) + " input LOW starts timelapse<BR>";
  }
  // HTML here
  String retVal = "<html><head><title>ANI Camera Remote</title>" + refreshLine + ""
                  "<style>table, th, td { border: 0px solid black;} button, input[type=number], input[type=submit]"
                  "{width:120px;height:28px;font-size:12pt;}</style></head><body>"
                  "<font color=\"#000000\"><body bgcolor=\"#c0c0c0\">"
                  "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">"
                  "<h2>" + prgTitle + " V" + prgVersion + "</h2>"
                  "<table style=\"font-size:16px;\">"
                  "<tr><td>Timelapse</td><td><a href=?function=RESET><button " + stateNotInTimelapse + ">Reset</button></a></td></tr>"
                  "<form method=GET>"
                  "<tr><td>Delay to start:</td><td><input " + stateNotInTimelapse + " id=delayToStart name=delayToStart type=number min=0 step=1 value=" + String(sDelay) + "> sec</td></tr>"
                  "<tr><td>Number of shots:</td><td><input " + stateNotInTimelapse + " id=numberOfShots name=numberOfShots type=number min=1 step=1 value=" + String(nShots) + "></td></tr>"
                  "<tr><td>Interval:</td><td><input " + stateNotInTimelapse + " id=delayBetweenShots name=delayBetweenShots type=number min=1 step=1 value=" + String(interval) + "> sec</td></tr>"
                  "<tr><td>Trigger duration (0=250ms):</td><td><input " + stateNotInTimelapse + " id=triggerDuration name=triggerDuration type=number min=0 step=1 value=" + String(tDuration) + "> sec</td></tr>"
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
                  "" + remainingTimeLine + ""
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

void checkAndProcessStartPin()
{
  if (startPin > -1) // Input pin defined
    if (digitalRead(startPin) == LOW) // start signal
    {
      while (digitalRead(startPin) == LOW) // wait till button not pressed
        delay(250);
      setAndDoProcessMode(TIMELAPSESTART);
    }
}

void checkAndProcessAutoStart()
{
  if (currentTimelapseAutoStart == 1)
  {
    currentTimelapseAutoStart = 0;
    setAndDoProcessMode(TIMELAPSESTART);
  }
}

String getRemainingTimeStr(unsigned long sDelay, unsigned long nShots, unsigned long interval, unsigned long secCounter)
{
  int seconds = ((nShots * interval) + sDelay) - secCounter;
  int days = seconds / 86400;
  int hours  = seconds % 86400 / 3600;
  int minutes  = seconds % 3600 / 60;
  seconds = seconds % 60;
  char timeFormat[100];
  sprintf(timeFormat, "%02d:%02d:%02d:%02d", days, hours, minutes, seconds);
  return String(timeFormat);
}

void trigger(triggerModes tMode)
{
  if (autofocusPin > -1)
    digitalWrite(autofocusPin, tMode);
  digitalWrite(triggerPin, tMode);
  currentTriggerMode = tMode;
  showTriggerState(tMode);
}

/*--------------------------------------------------
  Board specific methods
  Supported: ESP8266 and ESP32 (M5Stack)
  --------------------------------------------------*/
#ifdef ESP32
void displaySetup()
{
  M5.begin();
  onDisplay(MAINSCREEN);
}

void onDisplay(lcdScreens screen)
{
  int actuLine = 0;
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setFreeFont(FSS9);
  outLCD(1, screenHeaderLine, TFT_BLUE);
  actuScreen = screen;
  switch (screen)
  {
    case MAINSCREEN:
      {
        outLCD(2, "Delay to Start: " + String(delayToStart), TFT_WHITE);
        outLCD(3, "Number of Shots: " + String(numberOfShots), TFT_WHITE);
        outLCD(4, "Interval: " + String(delayBetweenShots), TFT_WHITE);
        outLCD(5, "Trigger duration: " + String(triggerDuration), TFT_WHITE);
        if (startPin > -1)
          outLCD(6, "Wait for GPIO" + String(startPin), TFT_YELLOW);
        outLCD(7, "START", TFT_YELLOW);
        outLCD(8, "STOP", TFT_YELLOW);
        outLCD(9, "ONE SHOT", TFT_YELLOW);
        outLCD(10, "RESET", TFT_YELLOW);
        outLCD(11, "Rem. Delay: " + String(currentDelayToStart) + " Shots: " + String(currentNShots), TFT_GREEN);
        String remainingTimeStr = (currentNShots > 0) ? getRemainingTimeStr(currentDelayToStart, currentNShots, delayBetweenShots, 0) : getRemainingTimeStr(delayToStart, numberOfShots, delayBetweenShots, 0);
        outLCD(12, "Remaining Time: " + remainingTimeStr, TFT_GREEN);
        lastDialogLine = 10;
        break;
      }
    case CONFIGSCREEN:
      {
        outLCD(2, "WIFI ON", TFT_YELLOW);
        outLCD(3, "WIFI OFF", TFT_YELLOW);
        outLCD(4, "EXIT Configuration", TFT_YELLOW);
        lastDialogLine = 4;
        break;
      }
  }
}

void changeValueOrExecFunction(int upDownFactor, lcdScreens screen, int lineNumber)
{
  switch (screen)
  {
    case MAINSCREEN:
      {
        switch (lineNumber)
        {
          case 2: delayToStart = limitsUpDown(upDownFactor, delayToStart, 0, LONG_MAX); break;
          case 3: numberOfShots = limitsUpDown(upDownFactor, numberOfShots, 1, LONG_MAX) ; break;
          case 4: delayBetweenShots = limitsUpDown(upDownFactor, delayBetweenShots, 1, LONG_MAX); break;
          case 5: triggerDuration = limitsUpDown(upDownFactor, triggerDuration, 0, LONG_MAX); break;
          case 6: setAndDoProcessMode(WAITFORGPIO); break;
          case 7: setAndDoProcessMode(TIMELAPSESTART); break;
          case 8: setAndDoProcessMode(TIMELAPSESTOP); break;
          case 9: setAndDoProcessMode(ONESHOT); break;
          case 10: setAndDoProcessMode(RESET); break;
        }
        break;
      }
    case CONFIGSCREEN:
      {
        switch (lineNumber)
        {
          case 2: wifiON = true; setupWiFi(); break;
          case 3: ESP.restart(); break;
          case 4: onDisplay(MAINSCREEN); break;
        }
        break;
      }
  }
}

void outLCD(int lineNumber, String outStr, int textColor)
{
  const int lineFactor = 19;
  M5.Lcd.setTextColor(lineNumber == highlightLine ? TFT_RED : textColor);
  M5.Lcd.setCursor(0, lineNumber * lineFactor);
  M5.Lcd.printf(outStr.c_str());
}

void switchScreenOnOff(lcdStates dState)
{
  if (dState == LCDON)
  {
    M5.Lcd.writecommand(ILI9341_DISPON);
    M5.Lcd.setBrightness(50);
  }
  else
  {
    M5.Lcd.writecommand(ILI9341_DISPOFF);
    M5.Lcd.setBrightness(0);
  }
  lcdIsON = (dState == LCDON);
}

void checkInactivity()
{
  unsigned long currentMilliseconds = millis();
  if ((currentMilliseconds - previousMilliseconds) > 1000)
  {
    inactivityCounter++;
    if (inactivityCounter > screenSaverTime)
    {
      inactivityCounter = 0;
      switchScreenOnOff(LCDOFF);
    }
    previousMilliseconds = currentMilliseconds;
  }
}

long limitsUpDown(int upDownFactor, long number, long lowerLimit, int upperLimit)
{
  long retVal = number + upDownFactor;
  retVal = (retVal >= lowerLimit) ? retVal : lowerLimit;
  retVal = (retVal <= upperLimit) ? retVal : upperLimit;
  return retVal;
}

void processKeyboard()
{
  if (M5.BtnA.isPressed() || M5.BtnB.isPressed() || M5.BtnC.isPressed())
  {
    delay(100);
    inactivityCounter = 0;
    if (!lcdIsON) // LCD is in screensaver mode so we had to switch it on
      switchScreenOnOff(LCDON);
    else
    {
      int upDownFactor = M5.BtnA.isPressed() ? +1 : -1;
      if (M5.BtnA.isPressed() || M5.BtnB.isPressed())
      {
        int scaleFactor = 1;
        scaleFactor = M5.BtnC.isPressed() ? 100 : 1;
        changeValueOrExecFunction(upDownFactor * scaleFactor, actuScreen, highlightLine);
      }
      else if (M5.BtnC.wasPressed()) // Only button C: Move from line to line
        highlightLine = (highlightLine >= lastDialogLine) ? 2 : highlightLine + 1;
      else if (M5.BtnC.pressedFor(5000)) // Enter config menu after 5 seconds press on button C
        actuScreen = CONFIGSCREEN;
      onDisplay(actuScreen);
    }
  }
  M5.update();
}

void showTriggerState(triggerModes tMode)
{
  if (tMode == ON)
    M5.Lcd.fillScreen(TFT_WHITE);
}
#else
// Methods for ESP8266
void displaySetup() { }
void onDisplay(lcdScreens screen) { }
void checkInactivity() { }
void processKeyboard() { }
void showTriggerState(triggerModes tMode) { } // Maybe some LED in the future
#endif
