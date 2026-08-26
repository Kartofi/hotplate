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

// Settings
const float tempTolerance = 10;
const float MaxTemp = 230;
// Statics
static const unsigned char PROGMEM image_power_bits[] = {
    0x3f, 0xff, 0x80, 0x40, 0x00, 0x40, 0x80, 0x00, 0x20, 0x80, 0x40, 0x20,
    0x80, 0x40, 0x20, 0x82, 0x48, 0x20, 0x84, 0x44, 0x20, 0x88, 0x42, 0x20,
    0x88, 0x42, 0x20, 0x88, 0x42, 0x20, 0x88, 0x02, 0x20, 0x88, 0x02, 0x20,
    0x84, 0x04, 0x20, 0x82, 0x08, 0x20, 0x81, 0xf0, 0x20, 0x80, 0x00, 0x20,
    0x80, 0x00, 0x20, 0xc0, 0x00, 0x60, 0x7f, 0xff, 0xc0, 0x3f, 0xff, 0x80};

static const unsigned char PROGMEM image_power_on_bits[] = {
    0x3f, 0xff, 0x80, 0x7f, 0xff, 0xc0, 0xff, 0xff, 0xe0, 0xff, 0xbf, 0xe0,
    0xff, 0xbf, 0xe0, 0xfd, 0xb7, 0xe0, 0xfb, 0xbb, 0xe0, 0xf7, 0xbd, 0xe0,
    0xf7, 0xbd, 0xe0, 0xf7, 0xbd, 0xe0, 0xf7, 0xfd, 0xe0, 0xf7, 0xfd, 0xe0,
    0xfb, 0xfb, 0xe0, 0xfd, 0xf7, 0xe0, 0xfe, 0x0f, 0xe0, 0xff, 0xff, 0xe0,
    0xff, 0xff, 0xe0, 0xff, 0xff, 0xe0, 0x7f, 0xff, 0xc0, 0x3f, 0xff, 0x80};

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

// State
float temp = 0;
float internalTemp = 0;

float setTemp = 0;

float thermalLag = 10;

bool active = false;

bool lastPlusState = LOW;
bool lastMinusState = LOW;
bool lastOkState = LOW;

bool transistorState = LOW;
//
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

void updateScreen(void) {
  display.clearDisplay();
  // plate_temp
  display.setTextColor(1);
  display.setTextWrap(false);
  display.setCursor(55, 33);
  display.print(String(Temp));
  // pi_temp
  display.setCursor(55, 48);
  display.print(String(internalTemp));
  // Labels
  display.setCursor(5, 48);
  display.print("Pi Pico: ");
  display.setCursor(10, 33);
  display.print("Plate:");

  display.drawRoundRect(0, 26, 91, 38, 10, 1);

  if (active) {
    if (transistorState == HIGH) {
      // Down
      display.drawTriangle(110, 57, 122, 35, 98, 35, 1);
      // Up
      display.fillTriangle(110, 8, 122, 30, 98, 30, 1);

    } else {
      // Down
      display.fillTriangle(110, 57, 122, 35, 98, 35, 1);
      // Up
      display.drawTriangle(110, 8, 122, 30, 98, 30, 1);
    }
  }

  // Power icon
  display.drawBitmap(5, 3, active ? image_power_on_bits : image_power_bits, 19,
                     20, 1);

  // string 10
  display.setCursor(29, 11);
  display.print("Hot Plate v1");
  display.display();
}

void loop() {

  bool btnPlusState = digitalRead(btnPlus);
  bool btnMinusState = digitalRead(btnMinus);
  bool btnOkState = digitalRead(btnOk);
  float newTemp = thermocouple.readCelsius();

  internalTemp = analogReadTemp();

  if (internalTemp >= 50 || !std::isnan(newTemp) && newTemp >= 500) {

    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);

    display.println("THERMAL RUNAWAY");
    Serial.println(F("THERMAL RUNAWAY"));
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

  updateScreen();
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
