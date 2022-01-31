# owlcms-esp32
Arduino code for an owlcms WiFi refereeing device.

The device uses a low-cost ESP32 device (Arduino-like) which can be bought for about 8$US.
The devices and owlcms communicate via an MQTT server. MQTT is a protocol designed for small IoT (Internet of Things) devices.
You can simulate the device on wokwi.com (see below)

![](https://wokwi.com/cdn-cgi/image/width=1920/https://thumbs.wokwi.com/projects/322138140212986451/thumbnail.jpg?tile&t=1643593581208)

## Simulation on wokwi.com
1. Check out this project, or download the zip.
2. Create a new project on wokwi.com.  Select ESP32 as the type.
3. In the new project, upload the files you have checked out using the down arrow to the right of the "Library Manager" menu.
4. Select the `secrets_template.h` file and rename it (same menu) to `secrets.h`   the following values are ok for initial testing.
```
const char* mqttServer = "test.mosquitto.org";
const char* mqttUserName= "";
const char* mqttPassword = "";
```
5. We recommend that you use a server that supports passwords.  StackHero has a free test mode that is available as a Heroku add on.
   1. create a new empty Heroku Application.  This is because only one free test application is available per account.
   2. add the StackHero MQTT server in the "Test" pricing plan (which is free)
   3. add a user - this will be your mqttUserName, and use the generated password as the mqttPassword
   4. For testing, allow the "dangerous" mode without TLS.
