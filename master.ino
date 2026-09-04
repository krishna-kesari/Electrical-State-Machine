#include <Wire.h>
#include <Servo.h> 
#define slave_ard 0x08

//Pin assignments
const int ldr_pin = A0;
const int gas_pin = A2;
const int temp_pin = A1;
const int servo_pin = 9;
const int piezo_pin = 8;
const int ir_pin = 2;

//Thresholds
const int   gas_on = 180;   // >180 gas alert
const int   gas_off = 130;   // <130 gas alert clears
const float temp_emergency = 45.0;  // >45C temp emergency
const int   blackout_drop = 30; // sudden drop (out of 100) counts as blackout
const int   blackout_abs = 10; // absolute darkness threshold
const int   blackout_clear = 15; // threshold before declaring light "restored"
Servo ventServo;

//Setting states
enum State { Standby, Active, Gas_Alert, Blackout, Temp_Emergency, Multi_Fault };

State currentState = Standby;
bool activated = false;
bool gasAlert = false;
bool blackoutAlert = false;
bool tempAlert = false;
int previousLight;
int servo_pos = 0;
unsigned long previousServoTime = 0;
const unsigned long servoInterval = 5;  // 15 ms between movements
void setup()
{
  Serial.begin(9600);
  Wire.begin(); // Joining the bus as "Master"
  pinMode(piezo_pin, OUTPUT);
  pinMode(servo_pin, OUTPUT);
  pinMode(ldr_pin, INPUT);
  pinMode(gas_pin, INPUT);
  pinMode(temp_pin, INPUT);
  previousLight = analogRead(ldr_pin);
  ventServo.attach(servo_pin, 500, 2500);
  ventServo.write(servo_pos);
}

void loop()
{
  int lightRaw = analogRead(ldr_pin);
  int gas = analogRead(gas_pin);
  int tempRaw  = analogRead(temp_pin);
  //Converting raw to desired units
  float voltage = tempRaw * (5.0 / 1023.0);
  float temp   = (voltage - 0.5) * 100.0;
  int light = map(lightRaw, 0, 974, 0, 100);
  int tempC = round(temp);

  Serial.print("Temp: \n ");
  Serial.print(gas);
  
  
  evaluateFaults(light, gas, tempC);
  resolveState();
  sendToSlave(light, gas, currentState, tempC);
  
  updateServo();
  
  if (currentState == Multi_Fault) {
    tone(piezo_pin, 1000);
  }
  else { 
    noTone(piezo_pin);
  }
  
  previousLight = light;
  delay(100);
 
}

void evaluateFaults(int light, int gas, float tempC){
  //Temperature has the highest priority
  if (tempC > temp_emergency) {
  tempAlert = true;
  }
  else {
    tempAlert = false;
  }
  
  //Gas Alert on for gas level > 180 and off for gas level < 130
  if (gas > gas_on)  {
    gasAlert = true;
  }
  else if (gas < gas_off) {
    gasAlert = false;
  }
  //Blackout for absoultue light less than 10%, or drop in light by 30%
  int drop = previousLight - light;
  if (!blackoutAlert){
    if (light < blackout_abs) {
      blackoutAlert = true;
    }
    else if (drop > blackout_drop && drop > 0) {
      blackoutAlert = true;
    }
  }
  else if (light > blackout_clear) {
    blackoutAlert = false;
  }
}

void resolveState() {
  if (tempAlert) {
    currentState = Temp_Emergency;
  } else if (gasAlert && blackoutAlert) {
    currentState = Multi_Fault;
  } else if (gasAlert) {
    currentState = Gas_Alert;                        
  } else if (blackoutAlert) {
    currentState = Blackout;
  } else {
    currentState = Active;
  }
}

void updateServo()
{
  unsigned long currentTime = millis();

  if (currentTime - previousServoTime >= servoInterval) {
    previousServoTime = currentTime;

    if (currentState == Temp_Emergency) {

      if (servo_pos < 180) {
        servo_pos+=5;
        ventServo.write(servo_pos);
      }

    }
    else {

      if (servo_pos > 0) {
        servo_pos-=5;
        ventServo.write(servo_pos);
      }
    }
  }
}


void sendToSlave(int light, int gas, int state, int temp)
{
  // I2C transmission will be added here
  Wire.beginTransmission(slave_ard);
  
  // Send gas
  Wire.write(highByte(gas));
  Wire.write(lowByte(gas));

  // Send state
  Wire.write(state);
  
  //Send light
  Wire.write(light);
  
  //Send temp
  Wire.write(highByte(temp));
  Wire.write(lowByte(temp));
  
  Wire.endTransmission();
  
}