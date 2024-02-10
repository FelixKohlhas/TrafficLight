#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <Adafruit_NeoPixel.h>

#include <WiFiClient.h>
#include <ESP8266WebServer.h>

#include <EEPROM.h>

#include "config.h"

#define PIN        2
#define NUMPIXELS 16
#define RINGSIZE   8

#define BUZZER     0

WiFiManager wifiManager;
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB);
WiFiClient wifiClient;
ESP8266WebServer server(80);

// Orientation (aka flip): 0 = red first; 1 = green first
int f = 0;

void handleRoot();

void setup() {
  Serial.begin(9600);

  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, HIGH);

  pixels.begin();
  for(int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 100, 100));
  }
  pixels.show();

  ArduinoOTA.onStart([]() {
    // OTA Start
    pixels.clear();
    pixels.show();
  });
  ArduinoOTA.begin();

  // Load orientation from EEPROM
  EEPROM.begin(8);
  f = EEPROM.read(0x00);

  WiFi.mode(WIFI_STA);

  // Disable WiFi sleep
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  // Use static IP (Remove this block to use DHCP)
  IPAddress ip(192, 168, 16, 99);
  IPAddress gateway(192, 168, 16, 1);
  IPAddress subnet(255, 255, 255, 0);
  wifiManager.setSTAStaticIPConfig(ip, gateway, subnet);

  wifiManager.autoConnect(auto_ssid, auto_pass);

  server.on("/", handleRoot);
  server.begin();
}

// Current state: 0 = red; 1 = green
int state = 1;

// Beep duration (0=off)
int beep = 0;

// Last state change (milliseconds since boot)
unsigned long last_change = 0;

// Green time (milliseconds)
int i = 10000;

// Dim red brightness (percent)
int d = 50;

void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  unsigned long now = millis();
  unsigned long elapsed = now - last_change;

  if (state != 0 && elapsed > (unsigned long)i) {
    // Switch back to red after i milliseconds
    last_change = now;
    state = 0;
  }

  if (elapsed < 1000) {
    int c = min(int(elapsed * 3), 255);

    if (state == 0) {
      // Transition to red
      for(int i = 0; i < RINGSIZE; i++) {
        pixels.setPixelColor(i + (RINGSIZE * f), pixels.Color(c * d / 100, 0, 0));
        pixels.setPixelColor(i + RINGSIZE - (RINGSIZE * f), pixels.Color(0, 255 - c, 0));
      }
      pixels.show();
    } else if (state == 1) {
      // Transition to green
      for(int i = 0; i < RINGSIZE; i++) {
        pixels.setPixelColor(i + (RINGSIZE * f), pixels.Color((255 - c) * d / 100, 0, 0));
        pixels.setPixelColor(i + RINGSIZE - (RINGSIZE * f), pixels.Color(0, c, 0));
      }
      pixels.show();

      if (beep != 0) {
        // Beep twice
        if (elapsed / beep == 0 || elapsed / beep == 2) digitalWrite(BUZZER, LOW);
        else digitalWrite(BUZZER, HIGH);
      }
    }
  }
  delay(10);
}

void handleRoot() {
  // http://<ip>/?i=10000&b=255&d=50&f=0&beep=80
  if (server.hasArg("i")) i = server.arg("i").toInt();
  if (server.hasArg("b")) pixels.setBrightness(server.arg("b").toInt());
  if (server.hasArg("d")) d = server.arg("d").toInt();
  if (server.hasArg("f")) {
    // Store orientation to EEPROM
    f = server.arg("f").toInt() % 2;
    EEPROM.put(0x00, f);
    EEPROM.commit();
  }
  if (server.hasArg("beep")) beep = server.arg("beep").toInt();

  state = 1;
  last_change = millis();
  server.send(200, "text/plain", "ok");
}