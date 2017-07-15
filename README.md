# ANI-Camera-Remote
Remote Control and Timelapse Control of Cameras via ESP8266

![First Remoter](http://smss.de/BK/index.php?cmd=image&sfpg=VmVyc2NoaWVkZW5lcy8qUmVtb3Rlcl9mZXJ0aWcuanBnKmI1NTRiMjFlZDllNTA1MWRlNDFiODRmY2E3OGRkZWI3MThkMjU0NWY4NWJjOGFmYmQ0ZTdkY2QyZTIzMGNlZTk)
The upper cable connects the device to a USB power bank, the lower cable is a cable release cable for Samsung NX cameras

What you need
-
* ESP8266 device (NodeMCU recommended) and power (i.e. USB power bank)
* USB-Kabel to upload the program
* Arduino IDE
* Camera that can be controlled by a external cable release via push button
* Cable release for your camera

What to do
-
* Make a connection to GPIO2 of the ESP8266 device and to your cable release 
  * Best practice is to buy a cable with a camera connector on one end and a separate connector on the push button end (i.e. 3.5mm jack plug)
  * Connect a matching jack to GPIO2
* Connect your cable release cable to the jack and the other end to your camera  
* __Make sure the output of the GPIO is compatible with your camera!__
* Connect the ESP8266 device via USB cable to your computer
* Install the Arduino IDE and the Board Manager for ESP8266 into the IDE
* Load the file ANI_CameraRemote.ino into the IDE
* Compile the file ANI_CameraRemote.ino in the Arduino IDE and upload it to your ESP8266-Device
* Connect your Computer or Smartphone to the WiFi network ANI-CameraRemote with the password Remoter12345678
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
![Prototype](http://smss.de/BK/index.php?cmd=image&sfpg=VmVyc2NoaWVkZW5lcy8qUmVtb3Rlcl9Qcm90b3R5cC5qcGcqOGZkOWY1NjFmYjRiOGY5ODdhMWI4OTdlNGZiNzkzNzYwN2I1MTMwNzFkNDUxYjBjZTBmZjhmOThiMzg4MmViYg)
Using ESP8266 DIL18 and CP2102
