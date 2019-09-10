#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <FastLED.h>
#include <PubSubClient.h>
#include <config.h>
#include <i2s.h>

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
extern const TProgmemPalette16 myRedWhiteBluePaletteP PROGMEM;

WiFiClient espClient;
PubSubClient client(espClient);
boolean on = false;
boolean justTurnedOff = true;
char *mode;
int r, g, b, a, speed = 1, musicWheelPosition = 255, musicDecay = 0,
                musicDecayCheck = 0, musicWheelSpeed = 3;
uint8_t brightness = 255; // brightness of given mode
long musicPreReact = 0.0f, musicReact = 0.0f;
double brightnessDelta = 0.0, colorBrightness = 0.0, fadePeriod = 0.0;

vector<string> modes{"c",  "m",  "p",   "r",  "rs", "rsb", "pg",
                     "ra", "bw", "bwb", "cl", "pa", "a",   "ab"};

boolean check_in_modes(const char *mode) {
  return find(modes.begin(), modes.end(), mode) != modes.end();
}

void callback(char *thetopic, byte *payload, unsigned int length) {
  // Serial.println(thetopic);
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
    if (!strcmp(data["p"], servicePassword) == 0) {
      string errorMessageStr = "invalid password";
      Serial.println(errorMessageStr.c_str());
      string errorMessageJsonStr = "{\"error\":\"" + errorMessageStr + "\"}";
      client.publish(errorTopic, errorMessageJsonStr.c_str());
      return;
    }
    if (data["m"]) {
      if (strcmp(data["m"], "c") == 0) {
        if (!(data["c"] && data["c"]["r"] >= 0 && data["c"]["r"] <= 255 &&
              data["c"]["g"] >= 0 && data["c"]["g"] <= 255 &&
              data["c"]["b"] >= 0 && data["c"]["b"] <= 255 &&
              data["c"]["a"] >= 0 && data["c"]["a"] <= 255)) {
          string errorMessageStr = "invalid rgba input";
          Serial.println(errorMessageStr.c_str());
          string errorMessageJsonStr =
              "{\"error\":\"" + errorMessageStr + "\"}";
          client.publish(errorTopic, errorMessageJsonStr.c_str());
          return;
        }
        r = data["c"]["r"];
        g = data["c"]["g"];
        b = data["c"]["b"];
        a = data["c"]["a"];
      } else if (!check_in_modes(data["m"])) {
        string errorMessageStr = "invalid mode";
        Serial.println(errorMessageStr.c_str());
        string errorMessageJsonStr = "{\"error\":\"" + errorMessageStr + "\"}";
        client.publish(errorTopic, errorMessageJsonStr.c_str());
        return;
      } else {
        brightness = (uint8_t)data["b"];
      }
      mode = strdup(data["m"]);
    }
    speed = data["s"];
    fadePeriod = data["f"];
    brightnessDelta = a / (double)(fadePeriod * 1000 / UPDATES_PER_SECOND);
    on = data["o"];
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

  i2s_rxtx_begin(true, false);
  i2s_set_rate(11025);

  WiFi.begin(ssid, wifiPassword);
  connect();
}

void FillLEDsFromPaletteColors(uint8_t colorIndex) {
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
  for (int i = 0; i < 16; i++)
    currentPalette[i] = CHSV(random8(), 255, random8());
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
const TProgmemPalette16 myRedWhiteBluePaletteP PROGMEM = {
    CRGB::Red,
    CRGB::Gray, // 'white' is too bright compared to red and blue
    CRGB::Blue,  CRGB::Black, CRGB::Red,   CRGB::Gray, CRGB::Blue,
    CRGB::Black, CRGB::Red,   CRGB::Red,   CRGB::Gray, CRGB::Gray,
    CRGB::Blue,  CRGB::Blue,  CRGB::Black, CRGB::Black};

void ChangePalettePeriodically() {
  uint8_t secondHand = (millis() / 1000) % 60;
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
    currentPalette = myRedWhiteBluePaletteP;
    currentBlending = NOBLEND;
  }
  if (secondHand == 55) {
    currentPalette = myRedWhiteBluePaletteP;
    currentBlending = LINEARBLEND;
  }
}

// FUNCTION TO GENERATE COLOR BASED ON VIRTUAL WHEEL
// https://github.com/NeverPlayLegit/Rainbow-Fader-FastLED/blob/master/rainbow.ino
CRGB music_scroll(int pos) {
  CRGB color(0, 0, 0);
  if (pos < 85) {
    color.g = 0;
    color.r = ((float)pos / 85.0f) * 255.0f;
    color.b = 255 - color.r;
  } else if (pos < 170) {
    color.g = ((float)(pos - 85) / 85.0f) * 255.0f;
    color.r = 255 - color.g;
    color.b = 0;
  } else if (pos < 256) {
    color.b = ((float)(pos - 170) / 85.0f) * 255.0f;
    color.g = 255 - color.b;
    color.r = 1;
  }
  return color;
}

void loop() {
  // first make sure you are connected to wifi
  connect();
  // then get current data
  client.loop();
  // then do the action
  if (on) {
    if (strcmp(mode, modes[0].c_str()) == 0) {
      if (fadePeriod == 0.0) {
        colorBrightness = (double)a;
      } else {
        if (colorBrightness + brightnessDelta < (double)a ||
            colorBrightness + brightnessDelta > 255.0) {
          brightnessDelta = -brightnessDelta;
        }
        colorBrightness = colorBrightness + brightnessDelta;
      }
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i].setRGB(r, g, b);
        leds[i].fadeLightBy((uint8_t)colorBrightness);
      }
    } else if (strcmp(mode, modes[1].c_str()) == 0) {
      int16_t left, right;
      i2s_read_sample(&left, &right, true);
      int audioSample = (int)left;
      if (audioSample > 0) {
        musicPreReact = ((long)NUM_LEDS * (long)audioSample) /
                        1023L; // TRANSLATE AUDIO LEVEL TO NUMBER OF LEDs
        if (musicPreReact > musicReact) // ONLY ADJUST LEVEL OF LED IF LEVEL
                                        // HIGHER THAN CURRENT LEVEL
          musicReact = musicPreReact;
      }
      for (int i = NUM_LEDS - 1; i >= 0; i--) {
        if (i < musicReact) {
          leds[i] = music_scroll((i * 256 / 50 + musicWheelPosition) % 256);
          leds[i].fadeLightBy(brightness);
        } else {
          leds[i] = CRGB(0, 0, 0);
        }
      }
      musicWheelPosition =
          musicWheelPosition - musicWheelSpeed; // SPEED OF COLOR WHEEL
      if (musicWheelPosition < 0)               // RESET COLOR WHEEL
        musicWheelPosition = 255;
      // REMOVE LEDs
      musicDecayCheck++;
      if (musicDecayCheck > musicDecay) // how many ms before one light decay
      {
        musicDecayCheck = 0;
        if (musicReact > 0)
          musicReact--;
      }
    } else if (strcmp(mode, modes[2].c_str()) == 0) {
      ChangePalettePeriodically();
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    } else if (strcmp(mode, modes[3].c_str()) == 0) {
      currentPalette = RainbowColors_p;
      currentBlending = LINEARBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    } else if (strcmp(mode, modes[4].c_str()) == 0) {
      currentPalette = RainbowStripeColors_p;
      currentBlending = NOBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    } else if (strcmp(mode, modes[5].c_str()) == 0) {
      currentPalette = RainbowStripeColors_p;
      currentBlending = LINEARBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    } else if (strcmp(mode, modes[6].c_str()) == 0) {
      SetupPurpleAndGreenPalette();
      currentBlending = LINEARBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    } else if (strcmp(mode, modes[7].c_str()) == 0) {
      SetupTotallyRandomPalette();
      currentBlending = LINEARBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    } else if (strcmp(mode, modes[8].c_str()) == 0) {
      SetupBlackAndWhiteStripedPalette();
      currentBlending = NOBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    } else if (strcmp(mode, modes[9].c_str()) == 0) {
      SetupBlackAndWhiteStripedPalette();
      currentBlending = LINEARBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    } else if (strcmp(mode, modes[10].c_str()) == 0) {
      currentPalette = CloudColors_p;
      currentBlending = LINEARBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    } else if (strcmp(mode, modes[11].c_str()) == 0) {
      currentPalette = PartyColors_p;
      currentBlending = LINEARBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    } else if (strcmp(mode, modes[12].c_str()) == 0) {
      currentPalette = myRedWhiteBluePaletteP;
      currentBlending = NOBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    } else if (strcmp(mode, modes[13].c_str()) == 0) {
      currentPalette = myRedWhiteBluePaletteP;
      currentBlending = LINEARBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    }
    FastLED.show();
  } else {
    if (justTurnedOff) {
      justTurnedOff = false;
      FastLED.clear();
    }
  }
  FastLED.delay(1000 / UPDATES_PER_SECOND);
}
