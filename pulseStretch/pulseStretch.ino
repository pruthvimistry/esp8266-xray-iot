/*
 * Stretches interrupt pulse to PULSE_WIDTH (ms) length.
 * 
 * Interrupt pin: D2 (input)
 * Output pin   : D4 (output)
 */

#define PULSE_WIDTH 200
#define DEBUG

void interruptHandler();

void setup() {
  // put your setup code here, to run once:
  #ifdef DEBUG
    Serial.begin(115200);
  #endif
  attachInterrupt(digitalPinToInterrupt(2),interruptHandler, FALLING);
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);
}

void loop() {
  // put your main code here, to run repeatedly:

}

void interruptHandler(){
  #ifdef DEBUG
    Serial.println("INTERRUPT");
  #endif
  digitalWrite(4, HIGH);
  delay(PULSE_WIDTH);
  digitalWrite(4, LOW);
  delay(PULSE_WIDTH);
  digitalWrite(4, HIGH);
}

