// owlcms refereeing device using MQTT
//
// ESP32 Arduino-style module that communicates with owlcms over WiFi.
// Both owlcms and the the refereeing device connect to a MQTT server.
// The refereeing device publishes the decisions and
// subscribes to requests to remind or summon referees

// ====== START CONFIG SECTION ======================================================

// MQTT passwords
// copy secrets_example.h to secrets.h and include secrets.h (which is .gitignored)
#include "secrets_local.h"

// owlcms parameters
// referee = 0 is used when all three referees are wired to the same device
// if each referee has their own device, set referee to 1, 2 or 3
const int referee = 0;
const char* platform = "A";

// pins for refs 1 2 and 3 (1 good, 1 bad, 2 good, etc.)
int decisionPins[] = {14, 27, 26, 25, 33, 32};
int ledPins[] = {15, 5, 19};
int buzzerPins[] = {4, 18, 21};

// number of beeps when referee wakeup is received
// set to 0 to disable beeps.
const int nbBeeps = 3;
const note_t cfgBeepNote = NOTE_F;
const int cfgBeepOctave = 7; // F7
const int cfgBeepMilliseconds = 100;
const int cfgSilenceMilliseconds = 50; // time between beeps
const int cfgLedDuration = 5000; // led stays for this maximum time;

// referee summon parameters
const int nbSummonBeeps = 1;
const note_t cfgSummonNote = NOTE_F;
const int cfgSummonOctave = 7; // F7
const int cfgSummonBeepMilliseconds = 3000;
const int cfgSummonSilenceMilliseconds = 0;
const int cfgSummonLedDuration = cfgSummonBeepMilliseconds;

// ====== END CONFIG SECTION ======================================================

#ifdef TLS
#include <WiFiClientSecure.h>
#include "certificates.h"
const int mqttPort = 8883;
#else
#include <WiFi.h>
const int mqttPort = 1883;
#endif

#include "Tone32.hpp"
#include "PubSubClient.h"

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
int ref13Number = 0;

// 6 pins where we have to detect transitions. initial state unknown.
int prevDecisionPinState[] = {-1, -1, -1, -1, -1, -1};

// for each referee, a Tone generator, and control for a LED
Tone32 tones[3] = {Tone32(buzzerPins[0], 0), Tone32(buzzerPins[1], 1), Tone32(buzzerPins[2], 2)};
int beepingIterations[] = {0, 0, 0};
int ledStartedMillis[] = {0, 0, 0};
int ledDuration[] = {0, 0, 0};
note_t beepNote;
int beepOctave;
int silenceMilliseconds;
int beepMilliseconds;

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
  setupTones();

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

    if (mqttClient.connect(clientId, mqttUserName, mqttPassword)) {
      Serial.println(" connected");

      char requestTopic[50];
      sprintf(requestTopic, "owlcms/decisionRequest/%s/+", fop);
      mqttClient.subscribe(requestTopic);

      char ledTopic[50];
      sprintf(ledTopic, "owlcms/led/#", fop);
      mqttClient.subscribe(ledTopic);

      char summonTopic[50];
      sprintf(summonTopic, "owlcms/summon/#", fop);
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
  for (int j = 0; j < ELEMENTCOUNT(decisionPins); j++) {
    pinMode(decisionPins[j], INPUT_PULLUP);
    prevDecisionPinState[j] = digitalRead(decisionPins[j]);
  }
  for (int j = 0; j < ELEMENTCOUNT(ledPins); j++) {
    pinMode(ledPins[j], OUTPUT);
  }
  for (int j = 0; j < ELEMENTCOUNT(buzzerPins); j++) {
    pinMode(buzzerPins[j], OUTPUT);
  }
}

void setupTones() {
  for (int j = 0; j < ELEMENTCOUNT(tones); j++) {
    tones[j] = Tone32(buzzerPins[j], j);
  }
}

void buttonLoop() {
  for (int j = 0; j < ELEMENTCOUNT(decisionPins); j++) {
    int state = digitalRead(decisionPins[j]);
    int prevState = prevDecisionPinState[j];
    if (state != prevState) {
      prevDecisionPinState[j] = state;
      if (state == LOW) {
        if (j % 2 == 0) {
          sendDecision(j / 2, "good");
        } else {
          sendDecision(j / 2, "bad");
        }
        return;
      }
    }
  }
}

void buzzerLoop() {
  for (int j = 0; j < ELEMENTCOUNT(beepingIterations); j++) {
    if (beepingIterations[j] > 0 && !tones[j].isPlaying()) {
      if (((beepingIterations[j] % 2) == 0)) {
        Serial.print(millis()); Serial.println(" sound on");
        tones[j].playNote(beepNote, beepOctave, beepMilliseconds);
      } else {
        Serial.print(millis()); Serial.println(" sound off");
        tones[j].silence(silenceMilliseconds);
      }
    }
    tones[j].update(); // turn off sound if duration reached.
    if (!tones[j].isPlaying()) {
      beepingIterations[j]--;
    }
  }
}

void ledLoop() {
  for (int j = 0; j < ELEMENTCOUNT(ledStartedMillis); j++) {
    if (ledStartedMillis[j] > 0) {
      ;
      if (millis() - ledStartedMillis[j] >= ledDuration[j]) {
        digitalWrite(ledPins[j], LOW);
        ledStartedMillis[j] = 0;
      }
    }
  }
}

void sendDecision(int ref02Number, const char* decision) {
  if (referee > 0) {
    // software configuration
    ref02Number = referee - 1;
  }
  char topic[50];
  sprintf(topic, "owlcms/decision/%s", fop);
  char message[32];
  sprintf(message, "%i %s", ref02Number + 1, decision);

  mqttClient.publish(topic, message);
  Serial.print(topic);  Serial.print(" ");  Serial.print(message);  Serial.println(" sent.");
}

String decisionRequestTopic = String("owlcms/decisionRequest/") + fop;
String summonTopic = String("owlcms/summon/") + fop;
String ledTopic = String("owlcms/led/") + fop;
void callback(char* topic, byte* message, unsigned int length) {

  String stTopic = String(topic);
  Serial.print("Message arrived on topic: ");  Serial.print(stTopic);  Serial.print("; Message: ");

  String stMessage;
  // convert byte to char
  for (int i = 0; i < length; i++) {
    stMessage += (char)message[i];
  }
  Serial.println(stMessage);

  int refIndex = stTopic.lastIndexOf("/") + 1;
  String refString = stTopic.substring(refIndex);
  int ref13Number = refString.toInt();

  if (stTopic.startsWith(decisionRequestTopic)) {
    changeReminderStatus(ref13Number - 1, stMessage.startsWith("on"));
  } else if (stTopic.startsWith(summonTopic)) {
    if (ref13Number == 0) {
      // topic did not end with number, blink all devices
      for (int j = 0; j < ELEMENTCOUNT(ledPins); j++) {
        changeSummonStatus(j, stMessage.startsWith("on"));
      }
    } else {
      changeSummonStatus(ref13Number - 1, stMessage.startsWith("on"));
    }
  } else if (stTopic.startsWith(ledTopic)) {
    if (ref13Number == 0) {
      // topic did not end with number, blink all devices
      for (int j = 0; j < ELEMENTCOUNT(ledPins); j++) {
        changeSummonStatus(j, stMessage.startsWith("on"));
      }
    } else {
      changeSummonStatus(ref13Number - 1, stMessage.startsWith("on"));
    }
  }
}

void changeReminderStatus(int ref02Number, boolean warn) {
  Serial.print("reminder "); Serial.print(warn); Serial.print(" "); Serial.println(ref02Number+1);
  if (warn) {
    digitalWrite(ledPins[ref02Number], HIGH);
    beepingIterations[ref02Number] = nbBeeps * 2;
    ledStartedMillis[ref02Number] = millis();
    ledDuration[ref02Number] = cfgLedDuration;
    beepNote = cfgBeepNote;
    beepOctave = cfgBeepOctave;
    silenceMilliseconds = cfgSilenceMilliseconds;
    beepMilliseconds = cfgBeepMilliseconds;
  } else {
    digitalWrite(ledPins[ref02Number], LOW);
    beepingIterations[ref02Number] = 0;
    tones[ref02Number].stopPlaying();
  }
}

void changeSummonStatus(int ref02Number, boolean warn) {
  Serial.print("summon ");  Serial.print(warn); Serial.print(" "); Serial.println(ref02Number+1);
  if (warn) {
    digitalWrite(ledPins[ref02Number], HIGH);
    beepingIterations[ref02Number] = nbSummonBeeps * 2;
    ledStartedMillis[ref02Number] = millis();
    ledDuration[ref02Number] = cfgSummonLedDuration;
    beepNote = cfgSummonNote;
    beepOctave = cfgSummonOctave;
    silenceMilliseconds = cfgSummonSilenceMilliseconds;
    beepMilliseconds = cfgSummonBeepMilliseconds;
  } else {
    digitalWrite(ledPins[ref02Number], LOW);
    beepingIterations[ref02Number] = 0;
    tones[ref02Number].stopPlaying();
  }
}
