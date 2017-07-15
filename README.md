# ANI-Camera-Remote
Remote Control and Timelapse Control of Cameras via [ESP8266](https://en.wikipedia.org/wiki/ESP8266)
![First Remoter](First_Remoter.jpg)
The upper cable connects the device to a USB power bank, the lower cable is a remote release cable for Samsung NX cameras

What you need
-
* ESP8266 device ([NodeMCU](https://en.wikipedia.org/wiki/NodeMCU) recommended) and power (e.g. USB power bank)
* USB cabel to upload the program
* [Arduino IDE](https://www.arduino.cc/en/Main/Software)
* [ESP8266 board manager](http://www.instructables.com/id/Quick-Start-to-Nodemcu-ESP8266-on-Arduino-IDE/)
* Camera that can be controlled by a remote release via push button
* Remote release cable for your camera

What to do
-
* Make a connection to GPIO2 of the ESP8266 device and to your remote release 
  * Best practice is to buy a cable with a camera connector on one end and a separate connector on the push button end (e.g. 3.5mm jack plug)
  * Connect a matching jack to GPIO2
* Connect your remote release cable to the jack and the other end to your camera  
* **__Make sure the output of the GPIO is compatible with your camera!__** Use optocoupler or relay between ESP8266 and jack if unsure
* Connect the ESP8266 device via USB cable to your computer
* Install the Arduino IDE and the ESP8266 board manager into the IDE
* Load the file ANI_CameraRemote.ino into the IDE
* Compile the file ANI_CameraRemote.ino in the Arduino IDE and upload it to your ESP8266 device
* Connect your computer or smartphone to the WiFi network ANI-CameraRemote with the password Remoter12345678
* Open a web browser and browse to the IP address of your ESP8266 device
* Control the remote via the webGUI

Settings
-
The settings can be changed at the beginning of the file ANI_CameraRemote.ino
* ssid = "ANI-CameraRemote" - WiFi SSID
* password = "Remoter12345678" - Set to "" for open access point w/o passwortd
* webServerPort = 80 - Port for web server
* triggerPin = 2 - GPIO2 as trigger output
* default_delayToStart = 0 - Delay in seconds till start of timelapse
* default_numberOfShots = 10 - Number of shots in timelapse mode
* default_delayBetweenShots = 5 - Delay between shots in timelapse mode

License
-
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details

Prototype
-
![Prototype](Remoter_Prototype.jpg)
Using ESP8266 DIL18 and CP2102 (usually more expensive than a NodeMCU that contains a CP2102)
