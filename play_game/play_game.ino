#include <FastLED.h>

#define LED_PIN     26
#define NUM_LEDS    45
#define BRIGHTNESS  64
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];

const int touchPin = 19;

// Pulse settings
int pulseWidth = 5;
int pulseDelay = 30;   // starting speed
const int minDelay = 5;

// Game settings
const int hitZone = 6; // how close to ends counts as a hit

// Pulse state
int pos = 0;
int direction = 1;
bool pulseActive = false;

// Game state
int score = 0;
bool gameRunning = false;

// Touch detection
bool lastTouchState = LOW;

void setup() {
  setup_display();
  delay(3000);
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);

  pinMode(touchPin, INPUT);
}

void loop() {
  bool touchState = digitalRead(touchPin);

  // Detect tap (rising edge)
  if (touchState == HIGH && lastTouchState == LOW) {
    handleTap();
  }
  lastTouchState = touchState;

  if (gameRunning) {
    updatePulse();
    drawGameUI(score, pos, direction, pulseDelay);
  } else {
    if(score == 0){
      draw_startup();
    }else{
      draw_scam();
    }
    idleAnimation();
  }

  delay(pulseDelay);
}

void handleTap() {
  if (!gameRunning) {
    startGame();
    return;
  }

  // Check if pulse is in hit zone
  if (isInHitZone()) {
    score++;
    direction *= -1;

    // Speed up
    pulseDelay = max(minDelay, pulseDelay - 3);

    flashColor(CRGB::Green);
  } else {
    gameOver();
  }
}

bool isInHitZone() {
  return (pos < hitZone || pos > NUM_LEDS - 1 - hitZone);
}

void startGame() {
  gameRunning = true;
  score = 0;
  pulseDelay = 30;
  pos = 0;
  direction = 1;
}

void gameOver() {
  flashColor(CRGB::Red);

  // Optional: show score via blinks
  showScore(score);

  gameRunning = false;
}

void updatePulse() {
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  for (int i = 0; i < pulseWidth; i++) {
    int index = pos - i;
    if (index >= 0 && index < NUM_LEDS) {
      leds[index] = CRGB::Blue;
    }
  }

  FastLED.show();

  pos += direction;

  // Auto bounce (but player should hit before this ideally)
  if (pos >= NUM_LEDS + pulseWidth) {
    direction = -1;
    pos = NUM_LEDS + pulseWidth;
  }

  if (pos < 0) {
    direction = 1;
    pos = 0;
  }
}

void flashColor(CRGB color) {
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
  delay(100);
}



void idleAnimation() {
  static uint8_t hue = 0;
  fill_rainbow(leds, NUM_LEDS, hue++, 5);
  FastLED.show();
}