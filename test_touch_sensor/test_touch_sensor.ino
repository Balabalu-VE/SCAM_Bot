const int touchPin = 19;

void setup() {
  pinMode(touchPin, INPUT);
  Serial.begin(9600);
}
void loop() {
  if (digitalRead(touchPin) == HIGH) {
    Serial.println("Touched!");
  }
  delay(100);
}
