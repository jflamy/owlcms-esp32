# owlcms-esp32
Arduino code for an owlcms WiFi refereeing device.  owlcms is a complete package for running Olympic Weightlifting competitions locally or in the cloud ([owlcms overview and documentation](https://owlcms.github.io/owlcms4/#/index), [owlcms Github](https://github.com/jflamy/owlcms4)).

A weightlifting competition uses three referees and five jury members.  This initial release covers the refereeing devices.  Referees need to give decisions. They also need to be prompted to enter a decision, or summoned by the jury, hence the need for bi-directional communications.

The device uses a low-cost ESP32 device (Arduino-like) which can be bought for about 8$US in small quantities.  The devices and owlcms communicate via an MQTT server. MQTT is a protocol designed for small IoT (Internet of Things) devices.

You can simulate the device on wokwi.com (see below)

![](https://wokwi.com/cdn-cgi/image/width=1920/https://thumbs.wokwi.com/projects/322138140212986451/thumbnail.jpg?tile&t=1643593581208)



## MQTT and owlcms preparation

Instructions for setting up an MQTT server and configuring owlcms be found at [this location](https://github.com/jflamy/owlcms4/blob/develop/docs/MQTT.md).

## Simulation on wokwi.com

The simulator is fast enough that you could actually use it to referee a competition - you would run the simulation in 3 different browsers, with a different referee number.

1. Check out this project, or download the zip.
2. Create a new project on wokwi.com.  Select ESP32 as the type.
3. In the new project, upload the files you have checked out using the down arrow to the right of the "Library Manager" menu.
4. Select the `secrets_template.h` file and rename it (same menu) to `secrets.h`   the following values are ok for initial testing.
```
const char* mqttServer = "test.mosquitto.org";
const char* mqttUserName= "";
const char* mqttPassword = "";
```
5. You should look at the `sketch.ino` file and change the referee number.  Normally you will compile and load the project 3 times, for referees 1, 2 and 3.
6. You should connect to their club (pick a monthly amount you can afford) which will give you access to `wokwigw`.  This will enable you to use your own Internet connection, and to connect to your own local MQTT server.

## Use on a real device

> If you actually build the device, please provide feedback.  Sound features cannot be simulated completely yet and may need tweaking.

Two options:

-  check out this project to your Arduino IDE and perform the same configuration of a secrets.h file, or

- work on wokwi to do the configuration using the technique above. You can then use the F1 key in the code editor to compile and download an HEX file.  This will give you a working .bin which can be loaded to the ESP32 as follows (the com port would be the one where you connect the ESP32)

  - install python. On windows `winget install Python`

  - install esptool. `pip install esptool`

  - Flash the program `python -m esptool -p com6 -b 900000 write_flash 0x1000 myfile.bin`

    
