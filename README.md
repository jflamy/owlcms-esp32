# owlcms-esp32
Arduino code for an owlcms WiFi refereeing device.

The device uses a low-cost ESP32 device (Arduino-like) which can be bought for about 8$US.
The devices and owlcms communicate via an MQTT server. MQTT is a protocol designed for small IoT (Internet of Things) devices.
You can simulate the device on wokwi.com (see below)

![](https://wokwi.com/cdn-cgi/image/width=1920/https://thumbs.wokwi.com/projects/322138140212986451/thumbnail.jpg?tile&t=1643593581208)

## Simulation on wokwi.com
1. Check out this project, or download the zip.
2. Create a new project on wokwi.com.  Select ESP32 as the type.
3. In the new project, to the right of the "Library Manager" item at the top of the code editor, use the down arrow to upload your files
4. Create a secrets.h file (from the same menu) with the following content
```
// wifi
// Wokwi-GUEST is used by the ESP32 simulator
const char* wifiSSID = "Wokwi-GUEST";
const char* wifiPassword = "";

// MQTT server
// On the cloud, make sure you use a server that enforces username and password
// StackHero on Heroku does.
const char* mqttServer = "test.mosquitto.org";
const char* mqttUserName= "";
const char* mqttPassword = "";
```
5. We recommend that you use a server that supports passwords. For example, create a new Heroku Application and add the StackHero MQTT server in "test" mode, which is available for free (the standard StackHero site does not have that option).
