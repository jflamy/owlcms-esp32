// owlcms refereeing device using MQTT
//
// ESP32 Arduino-style module is used to communicate over WiFi with owlcms.
// Both owlcms and the refereeing device connect to a MQTT server.
// The refereeing device publishes the decisions.
// Conversely, it subscribes to requests to warn the last referee when a decision is pending.
// Derived from https://randomnerdtutorials.com/esp32-mqtt-publish-subscribe-arduino-ide

#ifdef TLS
#include <WiFiClientSecure.h>
#include "certificates.h"
#else
#include <WiFi.h>
#endif

#include "Tone32.hpp"
#include "PubSubClient.h"

// ====== START CONFIG SECTION ======================================================

#include "secrets_stackhero.h"
// create your own secrets.h file (use secrets_example.h as starting point)
// in Wokwi, uses the down arrow to the right of the Library Manager menu

#ifdef TLS
const int mqttPort = 8883;
#else
const int mqttPort = 1883;
#endif


// owlcms parameters (used if no dispwitches are present or are switched off)
const int referee = 1;
const char* platform = "A";

// number of beeps when referee wakeup is received (led remains on until decision)
// set to 0 to disable sound.
const int nbBeeps = 5;
const note_t cfgBeepNote = NOTE_C;
const int cfgBeepOctave = 4; // C4
const int cfgBeepMilliseconds = 100;
const int cfgSilenceMilliseconds = 50; // time between beeps

// pins
const int ledPin = 2;
const int buzzerPin = 21;
const int inGoodPin = 25;
const int inBadPin = 33;

// kept if someone wants to add dip switches to control on-site config.
// dip switches for refs 1 2 and 3 respectively
int refPins[] = {26, 27, 14};
// dip switches 7 and 8 for platform selection
// 00 = default platform see above
// other switches use one digit as platform name.
// 01 = 1, 10 = 2, 11 = 3
int fopPins[] = {13, 12};

// ====== END CONFIG SECTION ======================================================

#define ELEMENTCOUNT(x)  (sizeof(x) / sizeof(x[0]))

#ifdef TLS
WiFiClientSecure wifiClient;
#else
WiFiClient wifiClient;
#endif
PubSubClient mqttClient;

String address;
boolean firstConnection = true;
String stMac;
char mac[50];
char clientId[50];
char fop[20];

int refNumber = 0;
int fopNumber = -1;

int prevGoodPinState = -1;
int prevBadPinState = -1;

Tone32 tone32(buzzerPin, 0);
int beepingIterations = 0;

void setup() {
#ifdef TLS
  wifiClient.setCACert(rootCABuff);
  wifiClient.setInsecure();
#endif
  mqttClient.setClient(wifiClient);
  Serial.begin(115200);
  randomSeed(analogRead(0));

  delay(10);

  Serial.print("Connecting to WiFi ");
  Serial.print(wifiSSID);
  wifiConnect();
  address = WiFi.macAddress();
  address.toCharArray(clientId, address.length() + 1);
  //Serial.println("");
  //Serial.println("WiFi connected");
  //Serial.println("IP address: ");
  //Serial.println(WiFi.localIP());
  //Serial.println(WiFi.macAddress());

  Serial.print("MQTT server: ");
  Serial.println(mqttServer);
  //wifiClient.setInsecure();
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(callback);

  setupPins();
  readDipSwitches();
}

void loop() {
  delay(5);
  if (!mqttClient.connected()) {
    mqttReconnect();
  }

  // no dip switches in this design, no need to check that they have changed
  // readDipSwitches();

  mqttClient.loop();
  buttonLoop();
  if (beepingIterations > 0 && !tone32.isPlaying()) {
    Serial.println(beepingIterations);
    if (((beepingIterations % 2) == 0)) {
      Serial.print(millis());
      Serial.println(" beep on");
      tone32.playNote(cfgBeepNote, cfgBeepOctave, cfgBeepMilliseconds);
    } else {
      Serial.print(millis());
      Serial.println(" beep off");
      tone32.silence(cfgSilenceMilliseconds);
    }
  }
  tone32.update(); // turn off sound if duration reached.
  if (!tone32.isPlaying()) {
    beepingIterations--;
  }
}

void wifiConnect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");
}

void mqttReconnect() {
  while (!mqttClient.connected()) {
    //long r = random(1000);
    //sprintf(clientId, "clientId-%ld", r);

    if (firstConnection) {
      Serial.print(address);
      Serial.print(" connecting to MQTT server...");
    }
    if (mqttClient.connect(clientId, mqttUserName, mqttPassword)) {
      if (firstConnection) {
        //Serial.print(clientId);
        Serial.println(" connected");
      }
      firstConnection = false;
      char requestTopic[50] = "owlcms/decisionRequest/";
      strcat(requestTopic, fop);
      strcat(requestTopic, "/+");
      mqttClient.subscribe(requestTopic);
      char ledTopic[50] = "owlcms/led/";
      strcat(ledTopic, fop);
      mqttClient.subscribe(ledTopic);
    } else {
      Serial.print("MQTT connection failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 1 second");
      delay(1000);
    }
  }
}

void setupPins() {
  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  pinMode(inGoodPin, INPUT_PULLUP);
  pinMode(inBadPin, INPUT_PULLUP);
  for (int j = 0; j < ELEMENTCOUNT(refPins); j++) {
    pinMode(refPins[j], INPUT_PULLUP);
  }
  for (int j = 0; j < ELEMENTCOUNT(fopPins); j++) {
    pinMode(fopPins[j], INPUT_PULLUP);
  }
}

/*
    This sets the refNumber and the fop variables that are actually used in the code.
    If there are no dip switches, or if they are off, the values come from
    the referee and platform variables, respectively.
*/
void readDipSwitches() {
  boolean configChanged = false;

  // first dip switch on determines which ref.
  int oRefNumber = refNumber;
  refNumber = referee;
  for (int j = 0; j < ELEMENTCOUNT(refPins); j++) {
    int pinNo = refPins[j];
    int state = digitalRead(pinNo);
    if (state == HIGH) {
    } else if (state == LOW) {
      refNumber = j + 1;
      break;
    }
  }
  if (oRefNumber != refNumber) {
    if (refNumber == 0) {
      // 0 -- use the software configuration
      refNumber = referee;
    }
    Serial.print("Referee ");
    Serial.print(refNumber);
    Serial.println(" selected.");
    configChanged = true;
  }

  // fopNumber is binary encoded.  7 and 8 off = software config
  // 7 off 8 on = 01, 7 on 8 off = 2, 7 on 8 on = 3.
  int oFopNumber = fopNumber;
  fopNumber = 0;
  for (int j = 0; j < ELEMENTCOUNT(fopPins); j++) {
    int pinNo = fopPins[j];
    int state = digitalRead(pinNo);
    if (state == HIGH) {
    } else if (state == LOW) {
      fopNumber = fopNumber + (j == 0 ? 1 : 2);
    }
  }
  if (oFopNumber != fopNumber) {
    if (fopNumber != 0) {
      itoa(fopNumber, fop, 10);
    } else {
      strcpy(fop, platform);
    }
    Serial.print("Platform ");
    Serial.print(fop);
    Serial.println(" selected.");
    configChanged = true;
  }
  if (configChanged) {
    mqttClient.disconnect();
    firstConnection = true; // not a reconnect on failure, show message
    mqttReconnect();
  }
}

void buttonLoop() {
  // check red button
  int state = digitalRead(inBadPin);
  int prevState = prevBadPinState;
  if (state != prevState) {
    prevBadPinState = state;
    if (state == LOW) {
      sendDecision("bad");
      return;
    }
  }

  // check white button
  state = digitalRead(inGoodPin);
  prevState = prevGoodPinState;
  if (state != prevState) {
    prevGoodPinState = state;
    if (state == LOW) {
      sendDecision("good");
      return;
    }
  }
}

void sendDecision(const char* decision) {
  if (refNumber < 1) {
    return;
  }
  char message[32];
  char topic[50] = "owlcms/decision/";
  strcat(topic, fop);
  itoa(refNumber, message, 10);
  strcat(message, " ");
  strcat(message, clientId);
  strcat(message, " ");
  strcat(message, decision);
  mqttClient.publish(topic, message);
  Serial.print(topic);
  Serial.print(" ");
  Serial.print(message);
  Serial.println(" sent.");
}


void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String stMessage;

  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    stMessage += (char)message[i];
  }
  Serial.println();

  if (String(topic).startsWith("owlcms/decisionRequest/") && String(topic).endsWith(String(refNumber))) {
    if (stMessage.startsWith("on")) {
      Serial.println("on");
      digitalWrite(ledPin, HIGH);
      beepingIterations = nbBeeps * 2;
    }
    else if (stMessage.startsWith("off")) {
      Serial.println("off");
      digitalWrite(ledPin, LOW);
      beepingIterations = 0;
      tone32.stopPlaying();
    }
  } else if (String(topic).startsWith("owlcms/led/")) {
    Serial.print("Changing output to ");
    if (stMessage.startsWith("on")) {
      Serial.println("on");
      digitalWrite(ledPin, HIGH);
      beepingIterations = nbBeeps * 2;
    }
    else if (stMessage.startsWith("off")) {
      Serial.println("off");
      digitalWrite(ledPin, LOW);
      beepingIterations = 0;
      tone32.stopPlaying();
    }
  }
}
