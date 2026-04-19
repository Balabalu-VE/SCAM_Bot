#include <Bluepad32.h>
#include <AccelStepper.h>

// ---------------- PIN DEFINITIONS ----------------
#define EN_PIN   4
#define MS1_2    12
#define MS2_2    18

#define DIR_1    16
#define STEP_1   17
#define DIR_2    14
#define STEP_2   13

// ---------------- GLOBAL VARIABLES ----------------
float left_speed = 0;
float right_speed = 0;

int axisY = 0;   // Left joystick
int axisRY = 0;  // Right joystick

bool ForwardR = true;
bool ForwardL = true;

float MaxSpeed = 6000.0;

// ---------------- STEPPERS ----------------
// DRIVER mode: (STEP pin, DIR pin)
AccelStepper stepper1(AccelStepper::DRIVER, STEP_1, DIR_1);
AccelStepper stepper2(AccelStepper::DRIVER, STEP_2, DIR_2);

// ---------------- GAMEPAD ----------------
const uint8_t maxGamepads = 1;
GamepadPtr myGamepads[maxGamepads];

// ---------------- AXIS LIMITS ----------------
const int leftAxisYLow  = -512;
const int leftAxisYHigh = 511;

const int rightAxisYLow  = -512;
const int rightAxisYHigh = 511;

// ---------------- SETUP STEPPERS ----------------
void setup_stepper() {
  pinMode(MS1_2, OUTPUT);
  pinMode(MS2_2, OUTPUT);

  pinMode(DIR_1, OUTPUT);
  pinMode(STEP_1, OUTPUT);
  pinMode(DIR_2, OUTPUT);
  pinMode(STEP_2, OUTPUT);

  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, LOW);   // Enable driver
  delay(10);

  // Microstepping (adjust if needed)
  digitalWrite(MS1_2, HIGH);
  digitalWrite(MS2_2, HIGH);

  stepper1.setMaxSpeed(MaxSpeed);
  stepper1.setAcceleration(100.0);

  stepper2.setMaxSpeed(MaxSpeed);
  stepper2.setAcceleration(100.0);

  Serial.println("Motors setup complete");
}

// ---------------- GAMEPAD CALLBACKS ----------------
void onConnectedGamepad(GamepadPtr gp) {
  for (int i = 0; i < maxGamepads; i++) {
    if (myGamepads[i] == nullptr) {
      myGamepads[i] = gp;
      break;
    }
  }
}

void onDisconnectedGamepad(GamepadPtr gp) {
  for (int i = 0; i < maxGamepads; i++) {
    if (myGamepads[i] == gp) {
      myGamepads[i] = nullptr;
      break;
    }
  }
}

// ---------------- SET SPEEDS ----------------
void set_speeds() {
  GamepadPtr myGamepad = myGamepads[0];

  if (myGamepad && myGamepad->isConnected()) {

    axisY  = myGamepad->axisY();
    axisRY = myGamepad->axisRY();

    // Left motor
    if (abs(axisY) > 60) {
      stepper1.setSpeed((axisY / 511.0) * MaxSpeed);
    } else {
      stepper1.setSpeed(0);
    }

    // Right motor
    if (abs(axisRY) > 60) {
      stepper2.setSpeed((axisRY / 511.0) * MaxSpeed);
    } else {
      stepper2.setSpeed(0);
    }

  } else {
    stepper1.setSpeed(0);
    stepper2.setSpeed(0);
  }
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  setup_stepper();

  // Initialize Bluepad32
  BP32.setup(&onConnectedGamepad, &onDisconnectedGamepad);

  // Uncomment if pairing issues
  // BP32.forgetBluetoothKeys();
}

// ---------------- LOOP ----------------
unsigned long lastControl = 0;
const int controlHz = 200; // 200 Hz control loop
void loop() {
  BP32.update();

  if (millis() - lastControl >= 5) { // 200 Hz
    lastControl = millis();

    set_speeds();
  }

  stepper1.runSpeed();
  stepper2.runSpeed();
}