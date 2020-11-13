#include <EEPROM.h>
#include <LiquidCrystal.h>
// Sorry for the messy code additions, I just started using arduino less than a week ago.
// The changes I've made are; LCD pins, the removal of led2, and the 13 pin led. I coded the animations myself :) I'm pretty happy about how they came out.
// This is the non i2c version, when I get an i2c shield I'll make another one. The only reason why I removed the 13led and led2 were for the whole lot of pins on the lcd.
// And korn101, you're a savior for the toyota-faithful. The days of ebay bee*r clones are over thanks to your work. My celica will be very happy!


const byte interruptPin = 2;
const byte relayPin = 3;
const byte led1 = 5;
const byte button1 = 6;
const byte button2 = 8;
const byte lcButton = 7;
const int rs = 4, en = 9, d4 = 10, d5 = 11, d6 = 12, d7 = 13;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

volatile byte state = LOW;
byte enabled = false;
int lastStart = 0;
int lastEnd = 0;
int betweenStarts = 0;
int betweenEnd = 0;
int pulseWidth = 0; // Pulse width of the pulse.
float rpm = 0;
float rpmEnds = 0;
float rpmAverage = 0;
float rpmEndsAverage = 0;

volatile int lastFall;
volatile int betweenFalls;
volatile float rpmFalls = 0;
volatile float rpmFallsAverage = 0;

float revLimitRpm = 3000; // initial RPM value, will be overwritten if EEPROM contains old value.
int rpmSmoothingConst = 10;
int responseDelay = 20; // 20 - fast
int returnDelay = 20; // delay between spark cut and start, magnified with cutHarshnessFactor.
float revLimitSafetyOffset = 1000; // rpm above rev limit where spark is default allowed, helps for in case handbrake is pulled at speed.

int cutHarshnessFactor = 1;

// EEPROM stuff:
int eeRevLimitAddress = 0;
int eeRevHarshnessAddress = eeRevLimitAddress + sizeof(float);

//
/*
   IGT Signal:


            dwell   advance    10*  TDC
   ______|---------|_______/___/____/
     low    high     low

*/

void setup() {
  lcd.begin(16, 2);
  Serial.begin(115200);
  // SETUP PINS:
  pinMode(interruptPin, INPUT); // or INPUT_PULLUP
  pinMode(relayPin, OUTPUT);
  // Set up IO buttons / leds
  pinMode(led1, OUTPUT);
  pinMode(button1, INPUT_PULLUP);
  pinMode(button2, INPUT_PULLUP);
  // Launch control button, enables LC momentarily:
  pinMode(lcButton, INPUT_PULLUP);
  // Setup Interrupts
  attachInterrupt(digitalPinToInterrupt(interruptPin), intFalling, FALLING);
  // Allow ignition by default
  digitalWrite(relayPin, HIGH); // allow ignition.

  // Play starting chime / sequence with whatever RPM is set:
  startFlash(1);

  delay(1000);

  // Read last Rev limit setting from EEPROM:
  readRevLimit();

  // Read last stored harshness:
  readHarshness();

  // Display both settings back to user:
  displayCurrentRevLimit();
  delay(500);
  displayCurrentHarshness();
  delay(500);

  // Enable by default:
  enableSystem();

}

void readRevLimit() {
  float f = 0;
  EEPROM.get(eeRevLimitAddress, f);
  if (f > 0) {
    revLimitRpm = f;
  }
}

void saveRevLimit() {
  EEPROM.put(eeRevLimitAddress, revLimitRpm);
}

void readHarshness() {
  int f = 0;
  EEPROM.get(eeRevHarshnessAddress, f);
  if (f > 0) {
    cutHarshnessFactor = f;
  }
}

void saveHarshness() {
  EEPROM.put(eeRevHarshnessAddress, cutHarshnessFactor);
}

void startFlash(int noFlashes) {

  for (int i = 0; i < noFlashes; i++) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("     Toyota     ");
    lcd.setCursor(0, 1);
    lcd.print("  Rev  Limiter  ");
    delay(2000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Source by");
    lcd.setCursor(9, 1);
    lcd.print("korn101");
    delay(2000);
    lcd.clear();
    lcd.print("LCD mod by ");
    lcd.setCursor(5, 1);
    lcd.print("loCouyGrAik");
    delay(200);
    lcd.setCursor(5, 1);
    lcd.print("olCoGyuikrA");
    delay(200);
    lcd.setCursor(5, 1);
    lcd.print("ColoyuGAkri");
    delay(200);
    lcd.setCursor(5, 1);
    lcd.print("CoolGuyArik");
    delay(1000);
    for (int i = 0; i <= 16; i++) {
      lcd.setCursor(-1 + i, 0);
      lcd.print("/");
      delay(40 - i);
    }
    for (int i = 0; i <= 16; i++) {
      lcd.setCursor(-1 + i, 1);
      lcd.print("/");
      delay(40 - i);
    }
    for (int i = 0; i <= 16; i++) {
      lcd.setCursor(-1 + i, 0);
      lcd.print(" ");
      delay(40 - i);
    }
    for (int i = 0; i <= 16; i++) {
      lcd.setCursor(-1 + i, 1);
      lcd.print(" ");
      delay(40 - i);
    }
    delay(100);
    digitalWrite(led1, HIGH);
    delay(10);
    digitalWrite(led1, LOW);
    delay(10);
  }

  digitalWrite(led1, LOW);

}

void loop() {

  // Enable/Disable system based on button press:
  if (enabled == false && digitalRead(button1) == LOW) {
    enableSystem();
  } else if (enabled == true && digitalRead(button1) == LOW) {
    disableSystem();
  }

  // only check for mode changes is the system is not active.
  if (enabled == false && digitalRead(button2) == LOW) {
    changeMode();
  }

  // If the system is enabled and the LC button is being held, then limit revs:
  if (enabled == true && digitalRead(lcButton) == HIGH) {
    if (rpmFallsAverage >= revLimitRpm && rpmFallsAverage < revLimitRpm + revLimitSafetyOffset) {
      cutSpark();
      delay(returnDelay * cutHarshnessFactor);
      allowSpark();

    }
    else {
      allowSpark();
    }
  }
  else {

  }



  delay(responseDelay);

}

void cutSpark() {
  digitalWrite(relayPin, LOW);
  digitalWrite(led1, HIGH);
}

void allowSpark() {
  digitalWrite(relayPin, HIGH);
  digitalWrite(led1, LOW);
}

void enableSystem() {
  enabled = true;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("LC Enabled ^-^");
  lcd.setCursor(0, 1);
  lcd.print("Changes applied!");
  digitalWrite(led1, LOW);
  delay(1000);
  for (int i = 0; i <= 16; i++) {
    lcd.setCursor(-1 + i, 0);
    lcd.print("/");
    lcd.setCursor(-1 + i, 1);
    lcd.print("/");
    delay(20);
  }
  for (int i = 0; i <= 16; i++) {
    lcd.setCursor(-1 + i, 0);
    lcd.print(" ");
    lcd.setCursor(-1 + i, 1);
    lcd.print(" ");
    delay(20);
  }


  displayCurrentRevLimit();

}

void disableSystem() {
  enabled = false;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("LC Disabled");
  lcd.setCursor(0, 1);
  lcd.print("Change settings");
  digitalWrite(led1, HIGH);
  delay(1000);
  for (int i = 0; i <= 16; i++) {
    lcd.setCursor(-1 + i, 0);
    lcd.print("/");
    lcd.setCursor(-1 + i, 1);
    lcd.print("/");
    delay(20);
  }
  for (int i = 0; i <= 16; i++) {
    lcd.setCursor(-1 + i, 0);
    lcd.print(" ");
    lcd.setCursor(-1 + i, 1);
    lcd.print(" ");
    delay(20);
  }
  displayCurrentRevLimit();
}

void displayCurrentHarshness() {
  for (int i = 0; i < cutHarshnessFactor; i++) {
    digitalWrite(led1, LOW);
    delay(100);
    digitalWrite(led1, HIGH);
    delay(100);
    digitalWrite(led1, LOW);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("//Delay");
    lcd.setCursor(0, 2);
    lcd.print(cutHarshnessFactor);
  }
}

void displayCurrentRevLimit() {
  for (int i = 0; i < revLimitRpm / 1000; i++) {
    digitalWrite(led1, LOW);
    delay(100);
    digitalWrite(led1, HIGH);
    delay(100);
    digitalWrite(led1, LOW);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("//Rev Limit");
    lcd.setCursor(0, 1);
    lcd.print(revLimitRpm);
    lcd.setCursor(4, 1);
    lcd.print("rpm");
  }
}


void changeCutHarshness() {
  // Change the cutHarshnessFactor to delay the spark cut for longer (make more jerky rev limiter = more flames)
  cutHarshnessFactor = cutHarshnessFactor + 1;
  if (cutHarshnessFactor > 5) {
    cutHarshnessFactor = 1;
  }
  // Now visually display set cut harshness limit:
  displayCurrentHarshness();
  // Save this setting to EEPROM.
  saveHarshness();

}

void changeMode() {
  // change the mode from whatever it was before.
  delay(200);
  delay(200);
  // Check if the button is being held:
  if (digitalRead(button2) == LOW) {
    // button is being held, change cut mode:
    return changeCutHarshness();
  }
  // Now set new revlimit:
  revLimitRpm = revLimitRpm + 1000;
  if (revLimitRpm > 5000) {
    revLimitRpm = 2000;
  }
  // Now visually display rev limit:
  displayCurrentRevLimit();
  // Save rev limit:
  saveRevLimit();
}

void intFalling() {
  // we know the IGT line has fallen from High->Low
  // It's important to note that at 7500 rpm, the falling gap (between falling edges) should never be below 4ms.
  betweenFalls = millis() - lastFall; // measure time between this and the last fall.
  lastFall = millis();
  if (betweenFalls >= 4 && betweenFalls <= 300) {
    // calculate rpm, since 2 pulses = 1 revolution of the crank. 1/(pulse gap) = pulses per second
    // Since 2 pulses occur per revolution, rpm = 2(pulses per second). 60/2 = 30. If using ms as units, it becomes 30,000.
    rpmFalls = (float) 30000.0 / betweenFalls;
    rpmFallsAverage = (float) ( rpmFallsAverage * (rpmSmoothingConst - 1) + rpmFalls ) / rpmSmoothingConst;
  }

}

void intChanged() {
  state = digitalRead(interruptPin);
  if (state == HIGH) {
    betweenStarts = micros() - lastStart;
    lastStart = micros();
    // rpm = (float) -125.0 * ((betweenStarts / 1000.0) - 30.4); // old calculation with fitting.
    rpm = (float) 30000.0 / (betweenStarts / 1000.0);
    rpmAverage = ( rpmAverage * (rpmSmoothingConst - 1) + rpm ) / rpmSmoothingConst;    // it requires a few loops for average to get near "a"
  }

  if (state == LOW) {
    betweenEnd = micros() - lastEnd;
    lastEnd = micros();
    pulseWidth = micros() - lastStart;
    rpmEnds = (float) 30000.0 / (betweenEnd / 1000.0);
    rpmEndsAverage = ( rpmEndsAverage * (rpmSmoothingConst - 1) + rpmEnds ) / rpmSmoothingConst;
  }
}
