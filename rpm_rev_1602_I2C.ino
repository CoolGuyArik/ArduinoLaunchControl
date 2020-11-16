#include <EEPROM.h>
#include <Wire.h> // Library for I2C communication
#include <LiquidCrystal_I2C.h> // Library for LCD
// Korn101, you're a savior for the toyota-faithful. The days of ebay bee*r clones are over thanks to your work. My celica will be very happy!

// THIS IS THE 16x2 I2C VERSION! This version has alot of nice improvements to korn101's code, such as the addition of an extra button for delay,
// and a readout screen that displays both values at once when the LC is enabled, and led1 doesn't have for{# of value flash} commands anymore, making the
// lcd experience more responsive. They are still there after "//" though, so if you so choose, you can put them back.
// This is really, really fun to add stuff to, and I'm definately loving arduino, and by extent, programming, more and more every day.
// You can message me on discord @ ARIK#4810 and on instagram @ coolguyarik

byte scaleOne[] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00011,
  B11111
};

byte scaleTwo[] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B00001,
  B01111,
  B11111,
  B11111
};

byte scaleThr[] = {
  B00000,
  B00000,
  B00000,
  B00111,
  B11111,
  B11111,
  B11111,
  B11111
};

byte scaleFou[] = {
  B00000,
  B00011,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111
};

byte scaleFiv[] = {
  B01111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111
};

byte buttonChar[] = {
  B00000,
  B00000,
  B01110,
  B11111,
  B11111,
  B01110,
  B00000,
  B00000
};


const byte interruptPin = 2;
const byte relayPin = 3;
const byte led1 = 5;
const byte button1 = 6;
const byte button2 = 7;
const byte button3 = 8;
const byte lcButton = 10;
LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 16, 2);
//                                         ^  *note

// *Use an I2C scanner script to find the address. For me, that was 0x27, so change that to what your program displays.

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
  lcd.init();
  lcd.backlight();
  Serial.begin(115200);

  // CHAR SETUP:
  lcd.createChar(1, scaleOne);
  lcd.createChar(2, scaleTwo);
  lcd.createChar(3, scaleThr);
  lcd.createChar(4, scaleFou);
  lcd.createChar(5, scaleFiv);
  lcd.createChar(6, buttonChar);

  // SETUP PINS:
  pinMode(interruptPin, INPUT); // or INPUT_PULLUP
  pinMode(relayPin, OUTPUT);
  // Set up IO buttons / leds
  pinMode(led1, OUTPUT);
  pinMode(button1, INPUT_PULLUP);
  pinMode(button2, INPUT_PULLUP);
  pinMode(button3, INPUT_PULLUP);
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
  delay(1000);
  displayCurrentHarshness();
  delay(1000);

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
    lcd.print("lcd mod by ");
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
    if(enabled == false && digitalRead(button2) == LOW){
    changeCutHarshness();
  }
  if(enabled == false && digitalRead(button3) == LOW) {
    changeRevLimitRpm();
  }

  // Enable/Disable system based on button press:
  if (enabled == false && digitalRead(button1) == LOW) {
    enableSystem();
  } else if (enabled == true && digitalRead(button1) == LOW) {
    disableSystem();
  }

  // only check for mode changes is the system is not active.
  if (enabled == false && digitalRead(button2) == LOW) {
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
  delay(50);
  readOuts();
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
  delay(50);
  changeInfo();
}   


void displayCurrentHarshness() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("//Delay ////////");
  lcd.setCursor(0, 1);
  lcd.print(cutHarshnessFactor);
  for (int i = 0; i < cutHarshnessFactor; i++) {
    lcd.setCursor(11 + i, 1);
    lcd.write(i + 1);
  }
  lcd.setCursor(0, 1);
  lcd.print(cutHarshnessFactor);
  //   for (int i = 0; i < cutHarshnessFactor; i++) { 
  //    digitalWrite(led1, LOW);
  //    delay(100);
  //    digitalWrite(led1, HIGH);
  //    delay(100);
  //    digitalWrite(led1, LOW);
  // }
}


void displayCurrentRevLimit() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("//Rev Limit ////");
  lcd.setCursor(0, 1);
  lcd.print(revLimitRpm);
  lcd.setCursor(4, 1);
  lcd.print("rpm");
  for (int i = 0; i < revLimitRpm / 1000; i++) {
    lcd.setCursor(11 + i, 1);
    lcd.write(i + 1);
  }
  //  for (int i = 0; i < revLimitRpm / 1000; i++) {
  //    digitalWrite(led1, LOW);
  //    delay(100);
  //    digitalWrite(led1, HIGH);
  //    delay(100);
  //    digitalWrite(led1, LOW);
  //  }
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
delay(500);
}

void changeRevLimitRpm() {
  revLimitRpm = revLimitRpm + 1000;
  if (revLimitRpm > 5000) {
    revLimitRpm = 2000;
  }
  // Now visually display rev limit:
  displayCurrentRevLimit();
  // Save rev limit:
  saveRevLimit();
  delay(500);
}

void readOuts() {
  lcd.clear();
  lcd.home();
  lcd.print("Limiter  ");
  lcd.print(revLimitRpm);
  lcd.setCursor(13,0);
  lcd.print("rpm");
  lcd.setCursor(0,1);
  lcd.print("Delay          ");
  lcd.print(cutHarshnessFactor);
}

void changeInfo() {
  lcd.clear();
  lcd.home();
  lcd.print("//Button  info//");
  lcd.setCursor(0,1);
  lcd.write(6);
  lcd.print(" RPM    ");
  lcd.write(6);
  lcd.print(" Delay");
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
