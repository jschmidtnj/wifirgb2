#include "esp8266_peri.h"
#include "i2s_reg.h"
#include "slc_register.h"
#include "user_interface.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <FastLED.h>
#include <PubSubClient.h>
#include <config.h>

#define BAUD_RATE 115200
#define LED_PIN 0
#define BRIGHTNESS 64
#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define UPDATES_PER_SECOND 100

// microphone stuff

#define I2S_CLK_FREQ 160000000 // Hz
#define I2S_24BIT 3            // I2S 24 bit half data
#define I2S_LEFT 2             // I2S RX Left channel

#define I2SI_DATA 12 // I2S data on GPIO12
#define I2SI_BCK 13  // I2S clk on GPIO13
#define I2SI_WS 14   // I2S select on GPIO14

#define SLC_BUF_CNT 8  // Number of buffers in the I2S circular buffer
#define SLC_BUF_LEN 64 // Length of one buffer, in 32-bit words.

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
int r, g, b, a, speed = 1, musicWheelPosition = 255, musicDecay = 0,
                musicDecayCheck = 0, musicWheelSpeed = 3;
uint8_t brightness = 255; // brightness of given mode
long musicPreReact = 0.0f, musicReact = 0.0f;
double brightnessDelta = 0.0, colorBrightness = 0.0, fadePeriod = 0.0;

vector<string> modes{"c",  "m",   "p",  "r",  "rs", "rsb", "pg", "ra",
                     "bw", "bwb", "cl", "pa", "a",  "ab",  "w"};

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

// MICROPHONE STUFF ****************************************

/**
 * Convert I2S data.
 * Data is 18 bit signed, MSBit first, two's complement.
 * Note: We can only send 31 cycles from ESP8266 so we only
 * shift by 13 instead of 14.
 * The 240200 is a magic calibration number I haven't figured
 * out yet.
 */
#define convert(sample) (((int32_t)(sample) >> 13) - 240200)

typedef struct {
  uint32_t blocksize : 12;
  uint32_t datalen : 12;
  uint32_t unused : 5;
  uint32_t sub_sof : 1;
  uint32_t eof : 1;
  volatile uint32_t owner : 1;

  uint32_t *buf_ptr;
  uint32_t *next_link_ptr;
} sdio_queue_t;

static sdio_queue_t i2s_slc_items[SLC_BUF_CNT]; // I2S DMA buffer descriptors
static uint32_t
    *i2s_slc_buf_pntr[SLC_BUF_CNT]; // Pointer to the I2S DMA buffer data
static volatile uint32_t rx_buf_cnt = 0;
static volatile uint32_t rx_buf_idx = 0;
static volatile bool rx_buf_flag = false;

/**
 * Set I2S clock.
 * I2S bits mode only has space for 15 extra bits,
 * 31 in total. The
 */
void i2s_set_rate(uint32_t rate) {
  uint32_t i2s_clock_div = (I2S_CLK_FREQ / (rate * 31 * 2)) & I2SCDM;
  uint32_t i2s_bck_div =
      (I2S_CLK_FREQ / (rate * i2s_clock_div * 31 * 2)) & I2SBDM;

  // RX master mode, RX MSB shift, right first, msb right
  I2SC &= ~(I2STSM | I2SRSM | (I2SBMM << I2SBM) | (I2SBDM << I2SBD) |
            (I2SCDM << I2SCD));
  I2SC |= I2SRF | I2SMR | I2SRMS | (i2s_bck_div << I2SBD) |
          (i2s_clock_div << I2SCD);
}

/**
 * Initialise I2S as a RX master.
 */
void i2s_init() {
  // Config RX pin function
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_I2SI_DATA);
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_I2SI_BCK);
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_I2SI_WS);

  // Enable a 160MHz clock
  I2S_CLK_ENABLE();

  // Reset I2S
  I2SC &= ~(I2SRST);
  I2SC |= I2SRST;
  I2SC &= ~(I2SRST);

  // Reset DMA
  I2SFC &= ~(I2SDE | (I2SRXFMM << I2SRXFM));

  // Enable DMA
  I2SFC |= I2SDE | (I2S_24BIT << I2SRXFM);

  // Set RX single channel (left)
  I2SCC &= ~((I2STXCMM << I2STXCM) | (I2SRXCMM << I2SRXCM));
  I2SCC |= (I2S_LEFT << I2SRXCM);
  i2s_set_rate(16667);

  // Set RX data to be received
  I2SRXEN = SLC_BUF_LEN;

  // Bits mode
  I2SC |= (15 << I2SBM);

  // Start receiver
  I2SC |= I2SRXS;
}

/**
 * Triggered when SLC has finished writing
 * to one of the buffers.
 */
void ICACHE_RAM_ATTR slc_isr(void *para) {
  uint32_t status;

  status = SLCIS;
  SLCIC = 0xFFFFFFFF;

  if (status == 0) {
    return;
  }

  if (status & SLCITXEOF) {
    // We have received a frame
    ETS_SLC_INTR_DISABLE();
    sdio_queue_t *finished = (sdio_queue_t *)SLCTXEDA;

    finished->eof = 0;
    finished->owner = 1;
    finished->datalen = 0;

    for (int i = 0; i < SLC_BUF_CNT; i++) {
      if (finished == &i2s_slc_items[i]) {
        rx_buf_idx = i;
      }
    }
    rx_buf_cnt++;
    rx_buf_flag = true;
    ETS_SLC_INTR_ENABLE();
  }
}

/**
 * Initialize the SLC module for DMA operation.
 * Counter intuitively, we use the TXLINK here to
 * receive data.
 */
void slc_init() {
  for (int x = 0; x < SLC_BUF_CNT; x++) {
    i2s_slc_buf_pntr[x] = (uint32_t *)malloc(SLC_BUF_LEN * 4);
    for (int y = 0; y < SLC_BUF_LEN; y++)
      i2s_slc_buf_pntr[x][y] = 0;

    i2s_slc_items[x].unused = 0;
    i2s_slc_items[x].owner = 1;
    i2s_slc_items[x].eof = 0;
    i2s_slc_items[x].sub_sof = 0;
    i2s_slc_items[x].datalen = SLC_BUF_LEN * 4;
    i2s_slc_items[x].blocksize = SLC_BUF_LEN * 4;
    i2s_slc_items[x].buf_ptr = (uint32_t *)&i2s_slc_buf_pntr[x][0];
    i2s_slc_items[x].next_link_ptr =
        (uint32_t *)((x < (SLC_BUF_CNT - 1)) ? (&i2s_slc_items[x + 1])
                                             : (&i2s_slc_items[0]));
  }

  // Reset DMA
  ETS_SLC_INTR_DISABLE();
  SLCC0 |= SLCRXLR | SLCTXLR;
  SLCC0 &= ~(SLCRXLR | SLCTXLR);
  SLCIC = 0xFFFFFFFF;

  // Configure DMA
  SLCC0 &= ~(SLCMM << SLCM);    // Clear DMA MODE
  SLCC0 |= (1 << SLCM);         // Set DMA MODE to 1
  SLCRXDC |= SLCBINR | SLCBTNR; // Enable INFOR_NO_REPLACE and TOKEN_NO_REPLACE

  // Feed DMA the 1st buffer desc addr
  SLCTXL &= ~(SLCTXLAM << SLCTXLA);
  SLCTXL |= (uint32_t)&i2s_slc_items[0] << SLCTXLA;

  ETS_SLC_INTR_ATTACH(slc_isr, NULL);

  // Enable EOF interrupt
  SLCIE = SLCITXEOF;
  ETS_SLC_INTR_ENABLE();

  // Start transmission
  SLCTXL |= SLCTXLS;
}

void setup() {
  // micrphone stuff
  rx_buf_cnt = 0;
  pinMode(I2SI_WS, OUTPUT);
  pinMode(I2SI_BCK, OUTPUT);
  pinMode(I2SI_DATA, INPUT);
  Serial.begin(BAUD_RATE);
  slc_init();
  i2s_init();

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, num_leds)
      .setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  currentPalette = RainbowColors_p;
  currentBlending = LINEARBLEND;

  WiFi.begin(ssid, wifiPassword);
  connect();
}

void FillLEDsFromPaletteColors(uint8_t colorIndex) {
  for (int i = 0; i < num_leds; i++) {
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

// This function sets up a palette of warm colors.
void SetupWarmPalette() {
  CRGB yellow = CRGB(255, 250, 0);
  CRGB orange = CRGB(255, 136, 0);
  CRGB red = CRGB(255, 70, 0);
  CRGB violet = CRGB(255, 0, 177);

  currentPalette =
      CRGBPalette16(yellow, yellow, orange, orange, red, red, violet, violet,
                    violet, violet, red, red, orange, orange, yellow, yellow);
}

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
      for (int i = 0; i < num_leds; i++) {
        leds[i].setRGB(r, g, b);
        leds[i].fadeLightBy((uint8_t)colorBrightness);
      }
    } else if (strcmp(mode, modes[1].c_str()) == 0) {
      double audioScaled = 0.0;
      if (rx_buf_flag) {
        for (int i = 0; i < SLC_BUF_LEN; i++) {
          if (i2s_slc_buf_pntr[rx_buf_idx][i] > 0) {
            audioScaled =
                (double)convert(i2s_slc_buf_pntr[rx_buf_idx][i] / 4096.0);
            break;
          }
        }
        rx_buf_flag = false;
      }
      if (audioScaled > 0.0) {
        musicPreReact =
            audioScaled * num_leds; // TRANSLATE AUDIO LEVEL TO NUMBER OF LEDs
        if (musicPreReact > musicReact) // ONLY ADJUST LEVEL OF LED IF LEVEL
                                        // HIGHER THAN CURRENT LEVEL
          musicReact = musicPreReact;
      }
      for (int i = num_leds - 1; i >= 0; i--) {
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
      yield();
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
    } else if (strcmp(mode, modes[14].c_str()) == 0) {
      SetupWarmPalette();
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
