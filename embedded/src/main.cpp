#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <FastLED.h>
#include <PubSubClient.h>
#include <config.h>

#define BAUD_RATE 115200
#define LED_PIN 0
#define NUM_LEDS 300
#define BRIGHTNESS 64
#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define UPDATES_PER_SECOND 100

using namespace std;

StaticJsonDocument<200> data;

CRGB leds[NUM_LEDS];
CRGBPalette16 currentPalette;
TBlendType currentBlending;
extern CRGBPalette16 myRedWhiteBluePalette;
extern const TProgmemPalette16 myRedWhiteBluePalette_p PROGMEM;

WiFiClient espClient;
PubSubClient client(espClient);
boolean on = false;
boolean justTurnedOff = true;
char *mode;
int r, g, b, a;

void callback(char *thetopic, byte *payload, unsigned int length) {
  if (strcmp(thetopic, controlTopic) == 0) {
    DeserializationError err = deserializeJson(data, payload);
    // Test if parsing succeeds.
    if (err) {
      string errorMessageStr = "deserializeJson() failed: ";
      errorMessageStr += string(err.c_str());
      Serial.println(errorMessageStr.c_str());
      string errorMessageJsonStr = "{\"error\":\"" + errorMessageStr + "\"}";
      client.publish(errorTopic, errorMessageJsonStr.c_str());
      return;
    }
    if (!strcmp(data["password"], servicePassword) == 0) {
      string errorMessageStr = "invalid password";
      Serial.println(errorMessageStr.c_str());
      string errorMessageJsonStr = "{\"error\":\"" + errorMessageStr + "\"}";
      client.publish(errorTopic, errorMessageJsonStr.c_str());
      return;
    }
    if (data["mode"]) {
      if (!(strcmp(data["mode"], "color") == 0 ||
            strcmp(data["mode"], "random") == 0)) {
        string errorMessageStr = "invalid mode";
        Serial.println(errorMessageStr.c_str());
        string errorMessageJsonStr = "{\"error\":\"" + errorMessageStr + "\"}";
        client.publish(errorTopic, errorMessageJsonStr.c_str());
        return;
      }
      if (strcmp(data["mode"], "color") == 0) {
        if (!(data["color"] && data["color"]["r"] && data["color"]["g"] &&
              data["color"]["b"] && data["color"]["a"] &&
              data["color"]["r"] >= 0 && data["color"]["r"] <= 255 &&
              data["color"]["g"] >= 0 && data["color"]["g"] <= 255 &&
              data["color"]["b"] >= 0 && data["color"]["b"] <= 255 &&
              data["color"]["a"] >= 0 && data["color"]["a"] <= 255)) {
          string errorMessageStr = "invalid rgba input";
          Serial.println(errorMessageStr.c_str());
          string errorMessageJsonStr =
              "{\"error\":\"" + errorMessageStr + "\"}";
          client.publish(errorTopic, errorMessageJsonStr.c_str());
          return;
        }
        r = data["color"]["r"];
        g = data["color"]["g"];
        b = data["color"]["b"];
        a = data["color"]["a"];
      }
      mode = strdup(data["mode"]);
    }
    on = data["on"];
    if (!on)
      justTurnedOff = true;
  }
}

void connect() {
  boolean notConnected = false;
  while (WiFi.status() != WL_CONNECTED) {
    notConnected = true;
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  if (notConnected) {
    Serial.println("Connected to WiFi..");
    client.setServer(mqttServer, mqttPort);
    client.setCallback(callback);
  }
  while (!client.connected()) {
    notConnected = true;
    Serial.println("Connecting to MQTT...");
    if (client.connect("ESP8266Client", mqttUser, mqttPassword)) {
      Serial.println("connected");
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
  if (notConnected)
    client.subscribe(controlTopic);
}

void setup() {
  Serial.begin(BAUD_RATE);

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
      .setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  currentPalette = RainbowColors_p;
  currentBlending = LINEARBLEND;

  WiFi.begin(ssid, wifiPassword);
  connect();
}

void FillLEDsFromPaletteColors(uint8_t colorIndex) {
  uint8_t brightness = 255;
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = ColorFromPalette(currentPalette, colorIndex, brightness,
                               currentBlending);
    colorIndex += 3;
  }
}

// There are several different palettes of colors demonstrated here.
//
// FastLED provides several 'preset' palettes: RainbowColors_p,
// RainbowStripeColors_p, OceanColors_p, CloudColors_p, LavaColors_p,
// ForestColors_p, and PartyColors_p.
//
// Additionally, you can manually define your own color palettes, or you can
// write code that creates color palettes on the fly.  All are shown here.

// This function fills the palette with totally random colors.
void SetupTotallyRandomPalette() {
  for (int i = 0; i < 16; i++) {
    currentPalette[i] = CHSV(random8(), 255, random8());
  }
}

// This function sets up a palette of black and white stripes,
// using code.  Since the palette is effectively an array of
// sixteen CRGB colors, the various fill_* functions can be used
// to set them up.
void SetupBlackAndWhiteStripedPalette() {
  // 'black out' all 16 palette entries...
  fill_solid(currentPalette, 16, CRGB::Black);
  // and set every fourth one to white.
  currentPalette[0] = CRGB::White;
  currentPalette[4] = CRGB::White;
  currentPalette[8] = CRGB::White;
  currentPalette[12] = CRGB::White;
}

// This function sets up a palette of purple and green stripes.
void SetupPurpleAndGreenPalette() {
  CRGB purple = CHSV(HUE_PURPLE, 255, 255);
  CRGB green = CHSV(HUE_GREEN, 255, 255);
  CRGB black = CRGB::Black;

  currentPalette =
      CRGBPalette16(green, green, black, black, purple, purple, black, black,
                    green, green, black, black, purple, purple, black, black);
}

// This example shows how to set up a static color palette
// which is stored in PROGMEM (flash), which is almost always more
// plentiful than RAM.  A static PROGMEM palette like this
// takes up 64 bytes of flash.
const TProgmemPalette16 myRedWhiteBluePalette_p PROGMEM = {
    CRGB::Red,
    CRGB::Gray, // 'white' is too bright compared to red and blue
    CRGB::Blue, CRGB::Black,

    CRGB::Red,  CRGB::Gray,  CRGB::Blue,  CRGB::Black,

    CRGB::Red,  CRGB::Red,   CRGB::Gray,  CRGB::Gray,
    CRGB::Blue, CRGB::Blue,  CRGB::Black, CRGB::Black};

void ChangePalettePeriodically() {
  uint8_t secondHand = (millis() / 1000) % 60;
  static uint8_t lastSecond = 99;
  if (lastSecond != secondHand) {
    lastSecond = secondHand;
    if (secondHand == 0) {
      currentPalette = RainbowColors_p;
      currentBlending = LINEARBLEND;
    }
    if (secondHand == 10) {
      currentPalette = RainbowStripeColors_p;
      currentBlending = NOBLEND;
    }
    if (secondHand == 15) {
      currentPalette = RainbowStripeColors_p;
      currentBlending = LINEARBLEND;
    }
    if (secondHand == 20) {
      SetupPurpleAndGreenPalette();
      currentBlending = LINEARBLEND;
    }
    if (secondHand == 25) {
      SetupTotallyRandomPalette();
      currentBlending = LINEARBLEND;
    }
    if (secondHand == 30) {
      SetupBlackAndWhiteStripedPalette();
      currentBlending = NOBLEND;
    }
    if (secondHand == 35) {
      SetupBlackAndWhiteStripedPalette();
      currentBlending = LINEARBLEND;
    }
    if (secondHand == 40) {
      currentPalette = CloudColors_p;
      currentBlending = LINEARBLEND;
    }
    if (secondHand == 45) {
      currentPalette = PartyColors_p;
      currentBlending = LINEARBLEND;
    }
    if (secondHand == 50) {
      currentPalette = myRedWhiteBluePalette_p;
      currentBlending = NOBLEND;
    }
    if (secondHand == 55) {
      currentPalette = myRedWhiteBluePalette_p;
      currentBlending = LINEARBLEND;
    }
  }
}

void loop() {
  // first make sure you are connected to wifi
  connect();
  // then get current data
  client.loop();
  // then do the action
  if (on) {
    if (strcmp(mode, "random") == 0) {
      ChangePalettePeriodically();
      static uint8_t startIndex = 0;
      startIndex = startIndex + 1; /* motion speed */
      FillLEDsFromPaletteColors(startIndex);
    } else if (strcmp(mode, "color") == 0) {
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i].setRGB(r, g, b);
        leds[i].fadeLightBy(a);
      }
    }
    FastLED.show();
  } else {
    Serial.println("turn it off");
    if (justTurnedOff) {
      justTurnedOff = false;
      FastLED.clear();
    }
  }
  FastLED.delay(1000 / UPDATES_PER_SECOND);
}
