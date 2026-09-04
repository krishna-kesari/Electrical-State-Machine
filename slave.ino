#include <Wire.h>
#include <LiquidCrystal.h>
#include <IRremote.hpp>

// --- Constants ---
#define SLAVE 0x08
#define IR_PIN 7
const int TEMP_LIMIT = 45;

// --- LCD wiring ---
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// --- State machine ---
enum Mode { STANDBY, ACTIVE, GAS_ALERT, BLACKOUT, TEMP_EMERGENCY, MULTI_FAULT };
Mode systemMode = STANDBY;
Mode lastMode = (Mode)-1;

// Sensor values
int lightVal = 0;
int gasVal = 0;
int tempVal = 0;
int lastLight =0;
int lastGas=0;

// Display toggle
bool showLight = true;
bool lastShowLight = true;

// --- IR codes (replace with real values from Serial Monitor) ---
unsigned long BTN_POWER  = 0xFF00BF00;
unsigned long BTN_RESET  = 0xF30CBF00;
unsigned long BTN_TOGGLE = 0xEF10BF00;

void setup() {
  Serial.begin(9600);
  IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK);
  pinMode(IR_PIN, INPUT);

  Wire.begin(SLAVE);
  Wire.onReceive(receiveData);

  lcd.begin(16, 2);
  lcd.print("AWAITING RITUAL");
}

void loop() {
  handleIR();
  updateDisplay();
}

// --- I2C receive handler ---
void receiveData(int numBytes) {
  if (numBytes < 6) return;

  int gasHigh = Wire.read();
  int gasLow  = Wire.read();
  byte stateByte = Wire.read();
  int newLight = Wire.read();
  int tempHigh = Wire.read();
  int tempLow  = Wire.read();

  gasVal   = (gasHigh << 8) | gasLow;
  lightVal = newLight;
  tempVal  = (int16_t)((tempHigh << 8) | tempLow);

  if (systemMode == STANDBY || systemMode == TEMP_EMERGENCY) return;
  if (stateByte <= MULTI_FAULT) systemMode = (Mode)stateByte;
}

// --- IR remote handler ---
void handleIR() {
  if (IrReceiver.decode()) {
    unsigned long cmd = IrReceiver.decodedIRData.decodedRawData;
    Serial.print("IR Command: 0x");
    Serial.println(cmd, HEX);

    if (cmd == BTN_POWER && systemMode == STANDBY) {
      systemMode = ACTIVE;
    } else if (cmd == BTN_RESET && tempVal < TEMP_LIMIT && systemMode == TEMP_EMERGENCY) {
      systemMode = STANDBY;
    } else if (cmd == BTN_TOGGLE) {
      showLight = !showLight;
    }

    IrReceiver.resume();
  }
}

// --- LCD update ---
void updateDisplay() {
  if (systemMode == lastMode && showLight == lastShowLight && lastLight == lightVal && lastGas == gasVal) return;
  lastMode = systemMode;
  lastShowLight = showLight;
  lastLight = lightVal;
  lastGas = gasVal;
  
  
  lcd.clear();
  lcd.setCursor(0, 0);

  switch (systemMode) {
    case STANDBY:
      lcd.print("AWAITING RITUAL");
      break;
    case TEMP_EMERGENCY:
      lcd.print("COOKED");
      break;
    case GAS_ALERT:
      lcd.print("TOXIC PURGE");
      break;
    case BLACKOUT:
      lcd.print("NOCTIS PROTOCOL");
      break;
    case MULTI_FAULT:
      lcd.print("MULTIPLE PROBLEM");
      lcd.setCursor(0, 1);
      lcd.print("DETECTED");
      break;
    case ACTIVE:
      if (showLight) {
        lcd.print("Light:");
        lcd.setCursor(0, 1);
        lcd.print(lightVal);
      } else {
        lcd.print("Gas:");
        lcd.setCursor(0, 1);
        lcd.print(gasVal);
      }
      break;
  }
}
