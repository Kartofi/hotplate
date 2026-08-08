#include "max6675.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <Wire.h>

// Screen configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, OLED_RESET);

// MAX6675 Thermocouple Pins
const int thermoCLK = 6;
const int thermoCS = 7;
const int thermoSO = 8;
MAX6675 thermocouple(thermoCLK, thermoCS, thermoSO);

// Transistor Pin
const int transistorPin = 4;

// Button Pins
const int btnPlus = 9;
const int btnMinus = 10;
const int btnOk = 11;

const float tempTolerance = 10;
const float MaxTemp = 230;

void setup() {

  Serial.begin(9600);

  Serial.println(F("Serial started..."));

  pinMode(transistorPin, OUTPUT);
  pinMode(btnPlus, INPUT_PULLDOWN);
  pinMode(btnMinus, INPUT_PULLDOWN);
  pinMode(btnOk, INPUT_PULLDOWN);

  // Turn off PTC
  digitalWrite(transistorPin, LOW);

  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  Serial.println(F("Pull-ups configured..."));

  Wire1.setSDA(2);
  Wire1.setSCL(3);
  Wire1.begin();
  Serial.println(F("Wire1 I2C initialized..."));

  Serial.println(F("Contacting SSD1306..."));
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);

  Serial.println(F("Display boot successful!"));

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("---- OKIII ----");
  display.println("Plotter READY!");
  display.display();

  delay(1500);
}
float temp = 0;
float setTemp = 0;

float thermalLag = 10;

bool active = false;

bool lastPlusState = LOW;
bool lastMinusState = LOW;
bool lastOkState = LOW;

bool transistorState = LOW;

void maintainTemp() {
  if (active == false) {
    return;
  }
  float diff = temp - setTemp;
  if (abs(diff) >= tempTolerance) {
    if (diff < 0) {
      digitalWrite(transistorPin, HIGH);
      transistorState = HIGH;
    } else {
      digitalWrite(transistorPin, LOW);
      transistorState = LOW;
    }
  }
}

void loop() {

  bool btnPlusState = digitalRead(btnPlus);
  bool btnMinusState = digitalRead(btnMinus);
  bool btnOkState = digitalRead(btnOk);
  float newTemp = thermocouple.readCelsius();

  float internalTemp = analogReadTemp();

  if (internalTemp >= 50 || !std::isnan(newTemp) && newTemp >) {

    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);

    display.println("THERMAL RUNAWAY FROM THE PICO");
    Serial.println(F("THERMAL RUNAWAY FROM THE PICO"));
    digitalWrite(transistorPin, LOW);
    active = false;
    transistorState = LOW;

    delay(1000);
    return;
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);

  if (isnan(newTemp)) {
    display.println("ERR (No Sensor)");
    Serial.println(F("ERR (No TEMP SENSOR)"));
    digitalWrite(transistorPin, LOW);

    while (true) {
      delay(100);
    }
  }

  temp = newTemp + thermalLag;

  if (btnMinusState == HIGH && lastMinusState == LOW && active == false &&
      setTemp >= 5) {
    // Click Minus
    setTemp -= 5;
    delay(50);
  }

  if (btnPlusState == HIGH && lastPlusState == LOW && active == false &&
      setTemp <= MaxTemp - 5) {
    // Click Plus
    setTemp += 5;
    delay(50);
  }
  if (btnOkState == HIGH && lastOkState == LOW) {
    // Click OK
    active = !active;
    delay(50);
  }

  display.println("N Temp C: ");
  display.print(temp);

  display.println("T Temp: ");
  display.print(setTemp);

  display.println("Active: ");
  display.print(active ? "ON" : "OFF");

  display.display();

  // Print to serial
  Serial.print("Temp: ");
  Serial.print(temp);
  Serial.print(" C | PLUS: ");
  Serial.print(btnPlusState);
  Serial.print(" | MINUS: ");
  Serial.print(btnMinusState);
  Serial.print(" | OK: ");
  Serial.println(btnOkState);
  Serial.print(" | Hotplate: ");
  Serial.println(transistorState ? "HIGH" : "LOW");
  //

  lastPlusState = btnPlusState;
  lastMinusState = btnMinusState;
  lastOkState = btnOkState;

  maintainTemp();
  delay(100);
}
