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

//Setup for reading input
const int BUFFER_SIZE = 50;
char buffer[BUFFER_SIZE];
int bufferIndex = 0;

bool messageReady = false;

float distance = 0;
float x_coordinate = 0;
float kp_x = 2;
float kp_distance = .5;
//------------------------------

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
      stepper1.setSpeed((-axisY / 511.0) * MaxSpeed);
    } else {
      //stepper1.setSpeed(0);
      //Do autonomous code instead
      stepper1.setSpeed(max(distance*kp_distance - x_coordinate*kp_x, MaxSpeed));
      if(distance < 50){
        stepper1.setSpeed(0.0);
      }
    }

    // Right motor
    if (abs(axisRY) > 60) {
      stepper2.setSpeed((axisRY / 511.0) * MaxSpeed);
    } else {
      //stepper2.setSpeed(0);
      //Do autonomous code instead
      stepper2.setSpeed(-max(distance*kp_distance + x_coordinate*kp_x, MaxSpeed));
      if(distance < 50){
        stepper2.setSpeed(0.0);
      }
    }

  } else {
    stepper1.setSpeed(0);
    stepper2.setSpeed(0);
  }
}
//-------------Code for reading input ---------------------------
void readSerialNonBlocking() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n') {  // End of message
      buffer[bufferIndex] = '\0';  // Null terminate
      bufferIndex = 0;
      messageReady = true;
    }
    else {
      if (bufferIndex < BUFFER_SIZE - 1) {
        buffer[bufferIndex++] = c;
      }
    }
  }
}

void parseMessage() {
  // Example: [123.4],[56.7]

  char *token1 = strtok(buffer, ",");
  char *token2 = strtok(NULL, ",");

  if (token1 && token2) {
    distance = atof(removeBrackets(token1));
    x_coordinate = atof(removeBrackets(token2));

    // Debug
    Serial.print("Distance: ");
    Serial.println(distance);
    Serial.print("X: ");
    Serial.println(x_coordinate);
  } else {
    Serial.println("Parse error");
  }
}

char* removeBrackets(char* str) {
  while (*str == '[') str++;  // skip leading [

  char* end = str + strlen(str) - 1;
  while (end > str && *end == ']') {
    *end = '\0';
    end--;
  }

  return str;
}
//-------------------------------



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
  //Reading serial
  readSerialNonBlocking();

  if (messageReady) {
    parseMessage();
    messageReady = false;
  }
  //------------------

  BP32.update();

  if (millis() - lastControl >= 5) { // 200 Hz
    lastControl = millis();

    set_speeds();
  }

  stepper1.runSpeed();
  stepper2.runSpeed();
}