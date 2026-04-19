#include <FastLED.h>

#define LED_PIN     26       // DIN connected to Digital Pin 6
#define NUM_LEDS    45      // Total LEDs in your strip
#define BRIGHTNESS  64      // Set brightness (0-255)
#define LED_TYPE    WS2812B // Or WS2811/SK6812
#define COLOR_ORDER GRB     // Common color order
CRGB leds[NUM_LEDS];
const int touchPin = 19;

void setup() {
  delay(3000); // safety delay
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  pinMode(touchPin, INPUT);
}

void loop() {
  // Set all LEDs to Red
  for(int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Red;
  }
  FastLED.show();
  delay(500);
}
