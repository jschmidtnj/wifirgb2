#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <FastLED.h>
#include <time.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include "config.h"

#define NTP_TIMEOUT 1500
// check every hour
#define CHECK_TIME_PERIOD 3600
// shuts off at 11:00
#define SHUTOFF_HOUR 23
// UTC timezone (-4 for EST)
int8_t timeZone = -4;
// uses daylight savings
boolean daylightSavings = true;
const PROGMEM char *ntpServer = "pool.ntp.org";

#define BAUD_RATE 115200
#define LED_PIN 5
#define BRIGHTNESS 64
#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define UPDATES_PER_SECOND 100

using namespace std;

StaticJsonDocument<200> data;

CRGB leds[num_leds];
CRGBPalette16 currentPalette;
TBlendType currentBlending;
extern CRGBPalette16 myRedWhiteBluePalette;
extern const TProgmemPalette16 myRedWhiteBluePaletteP PROGMEM;

WiFiClient espClient;
PubSubClient client(espClient);
boolean on = false;
boolean justTurnedOff = true;
char *mode;
int r, g, b, a, speed = 1;
uint8_t brightness = 255; // brightness of given mode
double brightnessDelta = 0.0, colorBrightness = 0.0, fadePeriod = 0.0;

vector<string> modes{"c", "p", "r", "rs", "rsb", "pg", "ra",
                     "bw", "bwb", "cl", "pa", "a", "ab", "w", "ha",
                     "th", "ch", "ny", "ea"};

bool check_in_modes(const char *mode)
{
  return find(modes.begin(), modes.end(), mode) != modes.end();
}

void callback(char *thetopic, byte *payload, unsigned int length)
{
  if (strcmp(thetopic, controlTopic) == 0)
  {
    DeserializationError err = deserializeJson(data, payload);
    // Test if parsing succeeds.
    if (err)
    {
      string errorMessageStr = "deserializeJson() failed: ";
      errorMessageStr += string(err.c_str());
      Serial.println(errorMessageStr.c_str());
      string errorMessageJsonStr = "{\"error\":\"" + errorMessageStr + "\"}";
      client.publish(messageTopic, errorMessageJsonStr.c_str());
      return;
    }
    if (!strcmp(data["p"], servicePassword) == 0)
    {
      string errorMessageStr = "invalid password";
      Serial.println(errorMessageStr.c_str());
      string errorMessageJsonStr = "{\"error\":\"" + errorMessageStr + "\"}";
      client.publish(messageTopic, errorMessageJsonStr.c_str());
      return;
    }
    if (data["m"])
    {
      if (strcmp(data["m"], "c") == 0)
      {
        if (!(data["c"] && data["c"]["r"] >= 0 && data["c"]["r"] <= 255 &&
              data["c"]["g"] >= 0 && data["c"]["g"] <= 255 &&
              data["c"]["b"] >= 0 && data["c"]["b"] <= 255 &&
              data["c"]["a"] >= 0 && data["c"]["a"] <= 255))
        {
          string errorMessageStr = "invalid rgba input";
          Serial.println(errorMessageStr.c_str());
          string errorMessageJsonStr =
              "{\"error\":\"" + errorMessageStr + "\"}";
          client.publish(messageTopic, errorMessageJsonStr.c_str());
          return;
        }
        r = data["c"]["r"];
        g = data["c"]["g"];
        b = data["c"]["b"];
        a = data["c"]["a"];
      }
      else if (!check_in_modes(data["m"]))
      {
        string errorMessageStr = "invalid mode";
        Serial.println(errorMessageStr.c_str());
        string errorMessageJsonStr = "{\"error\":\"" + errorMessageStr + "\"}";
        client.publish(messageTopic, errorMessageJsonStr.c_str());
        return;
      }
      else
      {
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

const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
char id[17];

const char * generateID()
{
  randomSeed(analogRead(0));
  int i = 0;
  for(i = 0; i < sizeof(id) - 1; i++) {
    id[i] = chars[random(sizeof(chars))];
  }
  id[sizeof(id) -1] = '\0';

  return id;
}

void connect()
{
  boolean notConnected = false;
  while (WiFi.status() != WL_CONNECTED)
  {
    notConnected = true;
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  if (notConnected)
  {
    Serial.println("Connected to WiFi..");
    client.setServer(mqttServer, mqttPort);
    client.setCallback(callback);
    configTime(timeZone * 3600, daylightSavings ? 3600 : 0, ntpServer);
  }
  boolean justConnectedMQTT = false;
  while (!client.connected())
  {
    notConnected = true;
    Serial.println("Connecting to MQTT...");
    if (client.connect(generateID(), mqttUser, mqttPassword))
    {
      Serial.println("connected");
      justConnectedMQTT = true;
    }
    else
    {
      Serial.print("failed with state ");
      Serial.println(client.state());
      delay(2000);
    }
  }
  if (justConnectedMQTT)
  {
    Serial.println("send connection message");
    string errorMessageJsonStr = "{\"info\":\"just connected\"}";
    client.publish(messageTopic, errorMessageJsonStr.c_str());
    Serial.println("connection message sent");
    client.subscribe(controlTopic);
  }
}

void getTime()
{
  static int last_check = millis();
  int current_time = millis();
  if ((current_time - last_check) > CHECK_TIME_PERIOD * 1000)
  {
    last_check = millis();
    struct tm timeData;
    if (!getLocalTime(&timeData))
    {
      Serial.println("Failed to obtain time");
      return;
    }
    if (timeData.tm_hour == SHUTOFF_HOUR && on)
    {
      on = false;
      justTurnedOff = true;
    }
  }
}

void setup()
{
  Serial.begin(BAUD_RATE);

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, num_leds)
      .setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  currentPalette = RainbowColors_p;
  currentBlending = LINEARBLEND;

  WiFi.begin(ssid, wifiPassword);
  connect();
}

void FillLEDsFromPaletteColors(uint8_t colorIndex)
{
  for (int i = 0; i < num_leds; i++)
  {
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
void SetupTotallyRandomPalette()
{
  for (int i = 0; i < 16; i++)
    currentPalette[i] = CHSV(random8(), 255, random8());
}

// This function sets up a palette of black and white stripes,
// using code.  Since the palette is effectively an array of
// sixteen CRGB colors, the various fill_* functions can be used
// to set them up.
void SetupBlackAndWhiteStripedPalette()
{
  // 'black out' all 16 palette entries...
  fill_solid(currentPalette, 16, CRGB::Black);
  // and set every fourth one to white.
  currentPalette[0] = CRGB::White;
  currentPalette[4] = CRGB::White;
  currentPalette[8] = CRGB::White;
  currentPalette[12] = CRGB::White;
}

// This function sets up a palette of purple and green stripes.
void SetupPurpleAndGreenPalette()
{
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
    CRGB::Blue, CRGB::Black, CRGB::Red, CRGB::Gray, CRGB::Blue,
    CRGB::Black, CRGB::Red, CRGB::Red, CRGB::Gray, CRGB::Gray,
    CRGB::Blue, CRGB::Blue, CRGB::Black, CRGB::Black};

// This function sets up a palette of warm colors.
void SetupWarmPalette()
{
  CRGB yellow = CRGB(255, 250, 0);
  CRGB orange = CRGB(255, 136, 0);
  CRGB red = CRGB(255, 70, 0);
  CRGB violet = CRGB(255, 0, 177);

  currentPalette =
      CRGBPalette16(yellow, yellow, orange, orange, red, red, violet, violet,
                    violet, violet, red, red, orange, orange, yellow, yellow);
}

void SetupHalloweenPalette()
{
  CRGB orange = CRGB(247, 95, 28);
  CRGB yellow = CRGB(255, 154, 0);
  CRGB black = CRGB(0, 0, 0);
  CRGB purple = CRGB(136, 30, 228);

  currentPalette =
      CRGBPalette16(orange, orange, yellow, yellow, black, black, purple, purple,
                    purple, purple, black, black, yellow, yellow, orange, orange);
}

void SetupThanksgivingPalette()
{
  CRGB yellow = CRGB(241, 185, 48);
  CRGB red = CRGB(181, 71, 48);
  CRGB brown = CRGB(158, 104, 42);
  CRGB green = CRGB(138, 151, 72);

  currentPalette =
      CRGBPalette16(yellow, yellow, red, red, green, green, brown, brown,
                    brown, brown, green, green, red, red, yellow, yellow);
}

void SetupChristmasPalette()
{
  CRGB green = CRGB(92, 137, 30);
  CRGB light_green = CRGB(116, 150, 63);
  CRGB red_pink = CRGB(236, 70, 58);
  CRGB orange_red = CRGB(193, 51, 40);

  currentPalette =
      CRGBPalette16(green, green, light_green, light_green, red_pink, red_pink, orange_red, orange_red,
                    orange_red, orange_red, red_pink, red_pink, light_green, light_green, green, green);
}

void SetupNewYearsPalette()
{
  CRGB dark_blue = CRGB(22, 36, 161);
  CRGB light_blue = CRGB(162, 189, 242);
  CRGB pink = CRGB(252, 91, 141);
  CRGB purple = CRGB(168, 2, 110);

  currentPalette =
      CRGBPalette16(dark_blue, dark_blue, light_blue, light_blue, pink, pink, purple, purple,
                    purple, purple, pink, pink, light_blue, light_blue, dark_blue, dark_blue);
}

void SetupEasterPalette()
{
  CRGB pink = CRGB(255, 212, 229);
  CRGB purple = CRGB(224, 205, 255);
  CRGB green = CRGB(183, 215, 132);
  CRGB yellow = CRGB(254, 255, 162);

  currentPalette =
      CRGBPalette16(pink, pink, purple, purple, green, green, yellow, yellow,
                    yellow, yellow, green, green, purple, purple, pink, pink);
}

void ChangePalettePeriodically()
{
  uint8_t secondHand = (millis() / 1000) % 60;
  if (secondHand == 0)
  {
    currentPalette = RainbowColors_p;
    currentBlending = LINEARBLEND;
  }
  if (secondHand == 10)
  {
    currentPalette = RainbowStripeColors_p;
    currentBlending = NOBLEND;
  }
  if (secondHand == 15)
  {
    currentPalette = RainbowStripeColors_p;
    currentBlending = LINEARBLEND;
  }
  if (secondHand == 20)
  {
    SetupPurpleAndGreenPalette();
    currentBlending = LINEARBLEND;
  }
  if (secondHand == 25)
  {
    SetupTotallyRandomPalette();
    currentBlending = LINEARBLEND;
  }
  if (secondHand == 30)
  {
    SetupBlackAndWhiteStripedPalette();
    currentBlending = NOBLEND;
  }
  if (secondHand == 35)
  {
    SetupBlackAndWhiteStripedPalette();
    currentBlending = LINEARBLEND;
  }
  if (secondHand == 40)
  {
    currentPalette = CloudColors_p;
    currentBlending = LINEARBLEND;
  }
  if (secondHand == 45)
  {
    currentPalette = PartyColors_p;
    currentBlending = LINEARBLEND;
  }
  if (secondHand == 50)
  {
    currentPalette = myRedWhiteBluePaletteP;
    currentBlending = NOBLEND;
  }
  if (secondHand == 55)
  {
    currentPalette = myRedWhiteBluePaletteP;
    currentBlending = LINEARBLEND;
  }
}

uint64_t loop_count = 1;

void loop()
{
  // first make sure you are connected to wifi
  connect();

  loop_count++;
  // Serial.printf("loop count %d\n", loop_count);
  // then get current data
  client.loop();
  // then do the action
  if (on)
  {
    if (strcmp(mode, modes[0].c_str()) == 0)
    {
      if (fadePeriod == 0.0)
      {
        colorBrightness = (double)a;
      }
      else
      {
        if (colorBrightness + brightnessDelta < (double)a ||
            colorBrightness + brightnessDelta > 255.0)
        {
          brightnessDelta = -brightnessDelta;
        }
        colorBrightness = colorBrightness + brightnessDelta;
      }
      for (int i = 0; i < num_leds; i++)
      {
        leds[i].setRGB(r, g, b);
        leds[i].fadeLightBy((uint8_t)colorBrightness);
      }
    }
    else if (strcmp(mode, modes[1].c_str()) == 0)
    {
      ChangePalettePeriodically();
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    }
    else if (strcmp(mode, modes[2].c_str()) == 0)
    {
      currentPalette = RainbowColors_p;
      currentBlending = LINEARBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    }
    else if (strcmp(mode, modes[3].c_str()) == 0)
    {
      currentPalette = RainbowStripeColors_p;
      currentBlending = NOBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    }
    else if (strcmp(mode, modes[4].c_str()) == 0)
    {
      currentPalette = RainbowStripeColors_p;
      currentBlending = LINEARBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    }
    else if (strcmp(mode, modes[5].c_str()) == 0)
    {
      SetupPurpleAndGreenPalette();
      currentBlending = LINEARBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    }
    else if (strcmp(mode, modes[6].c_str()) == 0)
    {
      SetupTotallyRandomPalette();
      currentBlending = LINEARBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    }
    else if (strcmp(mode, modes[7].c_str()) == 0)
    {
      SetupBlackAndWhiteStripedPalette();
      currentBlending = NOBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    }
    else if (strcmp(mode, modes[8].c_str()) == 0)
    {
      SetupBlackAndWhiteStripedPalette();
      currentBlending = LINEARBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    }
    else if (strcmp(mode, modes[9].c_str()) == 0)
    {
      currentPalette = CloudColors_p;
      currentBlending = LINEARBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    }
    else if (strcmp(mode, modes[10].c_str()) == 0)
    {
      currentPalette = PartyColors_p;
      currentBlending = LINEARBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    }
    else if (strcmp(mode, modes[11].c_str()) == 0)
    {
      currentPalette = myRedWhiteBluePaletteP;
      currentBlending = NOBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    }
    else if (strcmp(mode, modes[12].c_str()) == 0)
    {
      currentPalette = myRedWhiteBluePaletteP;
      currentBlending = LINEARBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    }
    else if (strcmp(mode, modes[13].c_str()) == 0)
    {
      SetupWarmPalette();
      currentBlending = LINEARBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    }
    else if (strcmp(mode, modes[14].c_str()) == 0)
    {
      SetupHalloweenPalette();
      currentBlending = LINEARBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    }
    else if (strcmp(mode, modes[15].c_str()) == 0)
    {
      SetupThanksgivingPalette();
      currentBlending = LINEARBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    }
    else if (strcmp(mode, modes[16].c_str()) == 0)
    {
      SetupChristmasPalette();
      currentBlending = LINEARBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    }
    else if (strcmp(mode, modes[17].c_str()) == 0)
    {
      SetupNewYearsPalette();
      currentBlending = LINEARBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    }
    else if (strcmp(mode, modes[18].c_str()) == 0)
    {
      SetupEasterPalette();
      currentBlending = LINEARBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + speed;
      FillLEDsFromPaletteColors(startIndex);
    }
    FastLED.show();
  }
  else
  {
    if (justTurnedOff)
    {
      justTurnedOff = false;
      FastLED.clear();
    }
  }
  // comment this to test mqtt on its own
  FastLED.delay(1000 / UPDATES_PER_SECOND);
}
