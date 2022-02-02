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

#include "secrets_local.h"
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

// number of beeps when referee wakeup or summon is received (led remains on until decision)
// set to 0 to disable sound.
const int nbBeeps = 5;
const note_t cfgBeepNote = NOTE_C;
const int cfgBeepOctave = 4; // C4
const int cfgBeepMilliseconds = 100;
const int cfgSilenceMilliseconds = 50; // time between beeps
const int cfgLedDuration = 3000; // led stays for this maximum time;

// pins
const int ledPin = 2;
const int buzzerPin = 21;
const int inGoodPin = 25;
const int inBadPin = 33;

// ====== END CONFIG SECTION ======================================================

#define ELEMENTCOUNT(x)  (sizeof(x) / sizeof(x[0]))

#ifdef TLS
WiFiClientSecure wifiClient;
#else
WiFiClient wifiClient;
#endif
PubSubClient mqttClient;

// networking values
String macAddress;
char mac[50];
char clientId[50];

// owlcms values
char fop[20];
int refNumber = 0;

int prevGoodPinState = -1;
int prevBadPinState = -1;

Tone32 tone32(buzzerPin, 0);
int beepingIterations = 0;
int ledStartedMillis = 0;
int ledDuration = 0;

void setup() {
#ifdef TLS
  wifiClient.setCACert(rootCABuff);
  wifiClient.setInsecure();
#endif
  mqttClient.setKeepAlive(20);
  mqttClient.setClient(wifiClient);
  Serial.begin(115200);
  randomSeed(analogRead(0));

  wifiConnect();
  macAddress = WiFi.macAddress();
  macAddress.toCharArray(clientId, macAddress.length() + 1);

  Serial.print("MQTT server: ");
  Serial.println(mqttServer);
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(callback);

  setupPins();
  refNumber = referee;
  strcpy(fop, platform);

  mqttReconnect();
}

void loop() {
  delay(10);
  if (!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();
  buttonLoop();
  buzzerLoop();
  ledLoop();
}

void wifiConnect() {
  Serial.print("Connecting to WiFi ");
  Serial.print(wifiSSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");
}

void mqttReconnect() {
  long r = random(1000);
  sprintf(clientId, "owlcms-%ld", r);
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnect();
  }
  while (!mqttClient.connected()) {
    Serial.print(macAddress);
    Serial.print(" connecting to MQTT server...");

    if (mqttClient.connect(clientId, mqttUserName, mqttPassword) && mqttClient.connected()) {
      Serial.println(" connected");

      char requestTopic[50];
      sprintf(requestTopic, "owlcms/decisionRequest/%s/+", fop);
      mqttClient.subscribe(requestTopic);

      char ledTopic[50];
      sprintf(ledTopic, "owlcms/led/%s", fop);
      mqttClient.subscribe(ledTopic);

      char summonTopic[50];
      sprintf(summonTopic, "owlcms/summon/%s", fop);
      mqttClient.subscribe(summonTopic);

    } else {
      Serial.print("MQTT connection failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 second");
      delay(5000);
    }
  }
}

void setupPins() {
  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(inGoodPin, INPUT_PULLUP);
  pinMode(inBadPin, INPUT_PULLUP);
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

void buzzerLoop() {
  if (beepingIterations > 0 && !tone32.isPlaying()) {
    if (((beepingIterations % 2) == 0)) {
      Serial.print(millis()); Serial.println(" beep on");
      tone32.playNote(cfgBeepNote, cfgBeepOctave, cfgBeepMilliseconds);
    } else {
      Serial.print(millis()); Serial.println(" beep off");
      tone32.silence(cfgSilenceMilliseconds);
    }
  }
  tone32.update(); // turn off sound if duration reached.
  if (!tone32.isPlaying()) {
    beepingIterations--;
  }
}

void ledLoop() {
  if (ledStartedMillis > 0) {
    //String msg = String("ledLoop");
    //Serial.println(msg + " " + (millis() - ledStartedMillis) + " " + " " + ledDuration);
    if (millis() - ledStartedMillis >= ledDuration) {
      digitalWrite(ledPin, LOW);
      ledStartedMillis = 0;
    }
  }
}

void sendDecision(const char* decision) {
  if (refNumber < 1) {
    return;
  }
  char topic[50];
  sprintf(topic, "owlcms/decision/%s", fop);
  char message[32];
  sprintf(message, "%i %s", refNumber, decision);

  mqttClient.publish(topic, message);
  Serial.print(topic);  Serial.print(" ");  Serial.print(message);  Serial.println(" sent.");
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");  Serial.print(topic);  Serial.print("; Message: ");
  String stMessage;
  // convert byte to char
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    stMessage += (char)message[i];
  }
  Serial.println();

  if (String(topic).startsWith("owlcms/decisionRequest/") && String(topic).endsWith(String(refNumber))) {
    changeWarningStatus(stMessage.startsWith("on"));
  } else if (String(topic).startsWith("owlcms/summon/") && String(topic).endsWith(String(refNumber))) {
    changeWarningStatus(stMessage);
  } else if (String(topic).startsWith("owlcms/led/")) {
    changeWarningStatus(stMessage);
  }
}

void changeWarningStatus(boolean warn) {
  Serial.print("beeping ");  Serial.println(warn);
  if (warn) {
    digitalWrite(ledPin, HIGH);
    beepingIterations = nbBeeps * 2;
    ledStartedMillis = millis();
    ledDuration = cfgLedDuration;
  } else {
    digitalWrite(ledPin, LOW);
    beepingIterations = 0;
    tone32.stopPlaying();
  }
}
