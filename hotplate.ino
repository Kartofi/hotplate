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
const float tempTolerance = 5; // Acceptable range of "error"
const float MaxTemp = 260; // Max tempeture the heater can reach (because of tons of real life variables (temperature of
                           // the air and etc.) it is lower than the heaters (ptc) maximum)
const float thermalLag = 10; // Because of heat transfer limitaions the sensor has around -10 degrees from the heater
const float heatingTimeOvershoot = 5000; // 5 seconds
const float heatingTempOvershoot = 40;   // 40 degrees from we stop the heater because it has worked

// Statics
static const unsigned char PROGMEM image_power_bits[] = {
    0x3f, 0xff, 0x80, 0x40, 0x00, 0x40, 0x80, 0x00, 0x20, 0x80, 0x40, 0x20, 0x80, 0x40, 0x20,
    0x82, 0x48, 0x20, 0x84, 0x44, 0x20, 0x88, 0x42, 0x20, 0x88, 0x42, 0x20, 0x88, 0x42, 0x20,
    0x88, 0x02, 0x20, 0x88, 0x02, 0x20, 0x84, 0x04, 0x20, 0x82, 0x08, 0x20, 0x81, 0xf0, 0x20,
    0x80, 0x00, 0x20, 0x80, 0x00, 0x20, 0xc0, 0x00, 0x60, 0x7f, 0xff, 0xc0, 0x3f, 0xff, 0x80};

static const unsigned char PROGMEM image_power_on_bits[] = {
    0x3f, 0xff, 0x80, 0x7f, 0xff, 0xc0, 0xff, 0xff, 0xe0, 0xff, 0xbf, 0xe0, 0xff, 0xbf, 0xe0,
    0xfd, 0xb7, 0xe0, 0xfb, 0xbb, 0xe0, 0xf7, 0xbd, 0xe0, 0xf7, 0xbd, 0xe0, 0xf7, 0xbd, 0xe0,
    0xf7, 0xfd, 0xe0, 0xf7, 0xfd, 0xe0, 0xfb, 0xfb, 0xe0, 0xfd, 0xf7, 0xe0, 0xfe, 0x0f, 0xe0,
    0xff, 0xff, 0xe0, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xe0, 0x7f, 0xff, 0xc0, 0x3f, 0xff, 0x80};

void setup()
{

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
float temp = 0;         // Current plate temperature
float internalTemp = 0; // The temperature of the PI Pico according to the internal sensor

float setTemp = 170; // The target temperature

bool active = false; // on/off

bool transistorState = LOW; // The state of the PTC heater (it is controlled using a SSR)
// Runtime vars
ulong turnedOnHeating = 0;
bool lastOvershoot = false;
bool overshootProtection = false;

void maintainTemp()
{
    // If its not "active" (or off) we just skip the logic
    if (active == false)
    {
        if (transistorState == HIGH)
        {
            digitalWrite(transistorPin, LOW);
            transistorState = LOW;
            turnedOnHeating = 0;
        }
        return;
    }
    float diff = temp - setTemp;

    bool isOvershooting = diff < 0 && abs(diff) <= heatingTempOvershoot && turnedOnHeating != 0 &&
                          millis() - turnedOnHeating >= heatingTimeOvershoot;
    // Checking if it overshoots (allegedly
    // because the heater does not magically
    // stop and continues to raise the
    // temperature until the whole plate is
    // the same temp as the heater element)

    if (abs(diff) >= tempTolerance && isOvershooting == false && overshootProtection == false)
    {
        // Check if its not in the tolerance area and if its not overshooting it
        if (diff < 0 && transistorState == LOW)
        { // if its below the temp we turn the heater on
            digitalWrite(transistorPin, HIGH);
            transistorState = HIGH;
            turnedOnHeating = millis();
        }
        else if (diff > 0 && transistorState == HIGH)
        { // the oposite
            digitalWrite(transistorPin, LOW);
            transistorState = LOW;
            turnedOnHeating = 0;
        }
    }
    else if (isOvershooting == true && lastOvershoot == false)
    { // in case it is overshooting NOW stop
      // the heater and turn on the protection
        digitalWrite(transistorPin, LOW);
        transistorState = LOW;
        turnedOnHeating = 0;
        overshootProtection = true;
    }

    if (abs(diff) >= tempTolerance && overshootProtection == true && diff < 0)
    { // turn off the overshoot protection if the tempeture is
      // higher than temp + tolerance
        overshootProtection = false;
    }
    lastOvershoot = isOvershooting;
}
//
ulong lastTempCheck = millis();
// Get the tempeture from the thermocoupler. Every 500 millis because of a limitation of read freq
float getTemp()
{
    ulong now = millis();
    if (now - lastTempCheck >= 500)
    {
        lastTempCheck = now;
        return thermocouple.readCelsius();
    }
    return temp - thermalLag;
}
/// Display all the stuff to the ssd1306 screen
void updateScreen(void)
{
    display.clearDisplay();
    // plate_temp
    display.setTextColor(1);
    display.setTextWrap(false);
    display.setCursor(55, 33);
    display.print(String(temp, 0));
    // pi_temp
    display.setCursor(55, 48);
    display.print(String(internalTemp, 0));
    // Labels
    display.setCursor(5, 48);
    display.print("Pi Pico: ");
    display.setCursor(10, 33);
    display.print("Plate:");

    display.drawRoundRect(0, 26, 91, 38, 10, 1);

    if (active)
    {
        if (transistorState == HIGH)
        {
            // Down
            display.drawTriangle(110, 57, 122, 35, 98, 35, 1);
            // Up
            display.fillTriangle(110, 8, 122, 30, 98, 30, 1);
        }
        else
        {
            // Down
            display.fillTriangle(110, 57, 122, 35, 98, 35, 1);
            // Up
            display.drawTriangle(110, 8, 122, 30, 98, 30, 1);
        }
    }

    // Power icon
    display.drawBitmap(5, 3, active ? image_power_on_bits : image_power_bits, 19, 20, 1);

    // Set Temp display
    display.setCursor(29, 11);
    display.print("Set: " + String(setTemp, 0));
    display.display();
}
// Input vars
ulong debounceTimeLast = 0;
const ulong debounceTime = 200;
//
void handleInputs()
{
    bool btnPlusState = digitalRead(btnPlus);
    bool btnMinusState = digitalRead(btnMinus);
    bool btnOkState = digitalRead(btnOk);

    if (btnMinusState == HIGH && active == false && setTemp >= 5 && millis() - debounceTimeLast >= debounceTime)
    {
        // Click Minus
        setTemp -= 5;
        debounceTimeLast = millis();
    }

    if (btnPlusState == HIGH && active == false && setTemp <= MaxTemp - 5 &&
        millis() - debounceTimeLast >= debounceTime)
    {
        // Click Plus
        setTemp += 5;
        debounceTimeLast = millis();
    }
    if (btnOkState == HIGH && millis() - debounceTimeLast >= debounceTime)
    {
        // Click OK
        active = !active;
        overshootProtection = false; // turn it off on start/end

        debounceTimeLast = millis();
    }
}

void loop()
{
    float newTemp = getTemp();
    internalTemp = analogReadTemp();

    if (internalTemp >= 50 || newTemp >= 400)
    {
        // In the absolute worst-case scenario where the whole thing catches on fire, at least we can stop the heater
        // and inform the user (if the screen is still alive) :)
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

    handleInputs();
    temp = newTemp + thermalLag;

    // Print to serial
    Serial.print("Temp: ");
    Serial.print(temp);
    Serial.print(" | Hotplate: ");
    Serial.println(transistorState ? "HIGH" : "LOW");
    //

    maintainTemp();
    updateScreen();

    delay(50);
}
